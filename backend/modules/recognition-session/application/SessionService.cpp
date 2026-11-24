#include "SessionService.hpp"

#include <uuid/uuid.h>
#include <sys/random.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <stdexcept>

namespace RoadSign::RecognitionSession {
namespace {
constexpr auto kHeartbeatLease = std::chrono::seconds{15};
constexpr auto kEventAttachWindow = std::chrono::seconds{10};
constexpr auto kUploadAdmissionLifetime = std::chrono::seconds{65};
constexpr std::size_t kMaxPendingUploadEvents = 32;

bool isUuid(std::string_view value) {
    if (value.size() != 36 || value[8] != '-' || value[13] != '-' ||
        value[18] != '-' || value[23] != '-') return false;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index == 8 || index == 13 || index == 18 || index == 23) continue;
        if (!std::isxdigit(static_cast<unsigned char>(value[index]))) return false;
    }
    return true;
}

} // namespace

SessionService::SessionService(std::uint32_t maxUploadAttempts, std::string trustedOrigin)
    : maxUploadAttempts_(maxUploadAttempts), trustedOrigin_(std::move(trustedOrigin)) {
    if (maxUploadAttempts > 100 || trustedOrigin_.empty())
        throw std::invalid_argument("invalid session service configuration");
}

std::string SessionService::randomUuid() {
    uuid_t bytes;
    uuid_generate_random(bytes);
    char output[37]{};
    uuid_unparse_lower(bytes, output);
    return output;
}

std::string SessionService::randomToken() {
    std::array<unsigned char, 32> bytes{};
    std::size_t filled = 0;
    while (filled < bytes.size()) {
        const auto count = getrandom(bytes.data() + filled, bytes.size() - filled, 0);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) throw std::runtime_error("secure random source unavailable");
        filled += static_cast<std::size_t>(count);
    }
    constexpr char digits[] = "0123456789abcdef";
    std::string token;
    token.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        token.push_back(digits[byte >> 4U]);
        token.push_back(digits[byte & 0x0fU]);
    }
    return token;
}

CreateResult SessionService::create(SourceKind sourceKind, std::string_view origin,
                                    std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(mutex_);
    if (origin != trustedOrigin_ || active_) return {CreateResult::Status::busy, std::nullopt};
    Entity session{};
    session.sessionId = randomUuid();
    session.cookieToken = randomToken();
    session.sourceKind = sourceKind;
    session.csrfToken = randomToken();
    session.maxUploadAttempts = maxUploadAttempts_;
    session.eventAttachDeadline = now + kEventAttachWindow;
    session.heartbeatDeadline = now + kHeartbeatLease;
    active_ = session;
    uploadEvents_.clear();
    return {CreateResult::Status::created, session};
}

bool SessionService::attachEvents(std::string_view sessionId,
                                  std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(mutex_);
    if (!active_ || active_->sessionId != sessionId || active_->eventsAttached ||
        active_->state != State::created || now > active_->eventAttachDeadline)
        return false;
    active_->eventsAttached = true;
    active_->state = State::awaitingInput;
    ++active_->stateRevision;
    active_->heartbeatDeadline = now + kHeartbeatLease;
    return true;
}

void SessionService::expireUnattached(std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(mutex_);
    if (!active_ || active_->eventsAttached || now <= active_->eventAttachDeadline)
        return;
    active_->state = State::ended;
    active_.reset();
    uploadEvents_.clear();
}

bool SessionService::authenticateLocked(std::string_view sessionId,
                                        std::string_view csrfToken,
                                        std::string_view origin) const {
    return active_ && active_->state != State::ended && active_->sessionId == sessionId &&
           active_->csrfToken == csrfToken && origin == trustedOrigin_;
}

bool SessionService::authenticate(std::string_view sessionId, std::string_view csrfToken,
                                   std::string_view origin) const {
    std::lock_guard lock(mutex_);
    return authenticateLocked(sessionId, csrfToken, origin);
}

bool SessionService::heartbeat(std::string_view sessionId, std::string_view csrfToken,
                               std::string_view origin, std::uint32_t epoch,
                               std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(mutex_);
    if (!authenticateLocked(sessionId, csrfToken, origin) || active_->state == State::ending ||
        active_->epoch != epoch || now >= active_->heartbeatDeadline)
        return false;
    active_->heartbeatDeadline = now + kHeartbeatLease;
    return true;
}

std::optional<std::uint64_t> SessionService::beginEnd(
    std::string_view sessionId, std::string_view csrfToken, std::string_view origin,
    std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(mutex_);
    if (!authenticateLocked(sessionId, csrfToken, origin) || active_->state == State::ending)
        return std::nullopt;
    active_->state = State::ending;
    active_->endingSince = now;
    ++active_->stateRevision;
    return active_->stateRevision;
}

bool SessionService::completeCleanup(std::string_view sessionId) {
    std::lock_guard lock(mutex_);
    if (!active_ || active_->sessionId != sessionId || active_->state != State::ending)
        return false;
    active_->state = State::ended;
    ++active_->stateRevision;
    active_.reset();
    uploadEvents_.clear();
    return true;
}

void SessionService::shutdown() noexcept {
    std::lock_guard lock(mutex_);
    if (!active_) return;
    active_->state = State::ending;
    ++active_->stateRevision;
    active_->state = State::ended;
    ++active_->stateRevision;
    active_.reset();
    uploadEvents_.clear();
}

bool SessionService::acceptsCallback(std::string_view sessionId, std::uint32_t epoch,
                                     std::uint32_t audioGeneration) const {
    std::lock_guard lock(mutex_);
    return active_ && active_->sessionId == sessionId && active_->state != State::ending &&
           active_->state != State::ended && active_->epoch == epoch &&
           active_->audioGeneration == audioGeneration;
}

bool SessionService::beginProcessing(std::string_view sessionId) {
    std::lock_guard lock(mutex_);
    if (!active_ || active_->sessionId != sessionId || active_->state != State::awaitingInput)
        return false;
    active_->state = State::processing;
    ++active_->stateRevision;
    return true;
}

bool SessionService::finishStillImage(std::string_view sessionId) {
    std::lock_guard lock(mutex_);
    if (!active_ || active_->sessionId != sessionId || active_->state != State::processing)
        return false;
    active_->state = State::reviewing;
    ++active_->stateRevision;
    return true;
}

UploadAdmissionResult SessionService::admitUpload(
    std::string_view sessionId, std::string_view csrfToken, std::string_view origin,
    std::uint32_t epoch, std::string uploadAttemptId,
    std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(mutex_);
    if (!active_ || active_->sessionId != sessionId)
        return {UploadAdmissionResult::Status::missing, std::nullopt, std::nullopt};
    if (!authenticateLocked(sessionId, csrfToken, origin))
        return {UploadAdmissionResult::Status::forbidden, std::nullopt, std::nullopt};
    if (active_->sourceKind == SourceKind::camera)
        return {UploadAdmissionResult::Status::wrongSource, std::nullopt, active_};
    if (active_->epoch != epoch)
        return {UploadAdmissionResult::Status::staleEpoch, std::nullopt, active_};
    if (!isUuid(uploadAttemptId))
        return {UploadAdmissionResult::Status::invalidAttempt, std::nullopt, active_};
    if (active_->uploadReceipt.uploadAttemptId == uploadAttemptId)
        return {UploadAdmissionResult::Status::duplicateAttempt, std::nullopt, active_};
    if (active_->hasAcceptedUpload)
        return {UploadAdmissionResult::Status::sourceAccepted, std::nullopt, active_};
    if (active_->maxUploadAttempts > 0 &&
        active_->uploadAttemptsUsed >= active_->maxUploadAttempts)
        return {UploadAdmissionResult::Status::exhausted, std::nullopt, active_};
    if (!active_->canAdmitUpload())
        return {UploadAdmissionResult::Status::busy, std::nullopt, active_};

    auto nonce = randomUuid();
    ++active_->uploadAttemptsUsed;
    active_->uploadReceipt = {std::move(uploadAttemptId), UploadStatus::receiving, std::nullopt};
    active_->uploadAdmissionNonce = nonce;
    active_->uploadAdmissionDeadline = now + kUploadAdmissionLifetime;
    ++active_->stateRevision;
    queueUploadEventLocked();
    return {UploadAdmissionResult::Status::admitted, std::move(nonce), active_};
}

bool SessionService::setUploadValidating(std::string_view sessionId,
                                         std::string_view uploadAttemptId,
                                         std::string_view admissionNonce,
                                         std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(mutex_);
    if (!active_ || active_->sessionId != sessionId ||
        active_->uploadReceipt.uploadAttemptId != uploadAttemptId ||
        active_->uploadReceipt.status != UploadStatus::receiving ||
        !active_->uploadAdmissionNonce || *active_->uploadAdmissionNonce != admissionNonce ||
        !active_->uploadAdmissionDeadline || now >= *active_->uploadAdmissionDeadline)
        return false;
    active_->uploadReceipt.status = UploadStatus::validating;
    ++active_->stateRevision;
    queueUploadEventLocked();
    return true;
}

std::optional<std::string> SessionService::expireUploadAdmission(
    std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(mutex_);
    if (!active_ || !active_->uploadAdmissionDeadline ||
        now < *active_->uploadAdmissionDeadline)
        return std::nullopt;
    active_->uploadReceipt.status = UploadStatus::aborted;
    active_->uploadReceipt.errorCode = "UPLOAD_TIMEOUT";
    active_->uploadAdmissionNonce.reset();
    active_->uploadAdmissionDeadline.reset();
    active_->state = State::ending;
    active_->endingSince = now;
    ++active_->stateRevision;
    queueUploadEventLocked();
    return active_->sessionId;
}

bool SessionService::finishUpload(std::string_view sessionId,
                                  std::string_view uploadAttemptId, bool accepted,
                                  std::optional<std::string> safeErrorCode,
                                  std::optional<MediaProcessing::MediaSource> media) {
    std::lock_guard lock(mutex_);
    if (!active_ || active_->sessionId != sessionId ||
        active_->uploadReceipt.uploadAttemptId != uploadAttemptId ||
        active_->uploadReceipt.status != UploadStatus::validating ||
        (accepted && (safeErrorCode || !media)) ||
        (!accepted && (!safeErrorCode || media)))
        return false;
    active_->uploadReceipt.status = accepted ? UploadStatus::accepted : UploadStatus::rejected;
    active_->uploadReceipt.errorCode = std::move(safeErrorCode);
    active_->hasAcceptedUpload = accepted;
    active_->acceptedMedia = std::move(media);
    active_->uploadAdmissionNonce.reset();
    active_->uploadAdmissionDeadline.reset();
    ++active_->stateRevision;
    queueUploadEventLocked();
    return true;
}

void SessionService::queueUploadEventLocked() {
    if (uploadEvents_.size() >= kMaxPendingUploadEvents) {
        active_->state = State::ending;
        ++active_->stateRevision;
        return;
    }
    uploadEvents_.push_back({active_->epoch, active_->stateRevision, active_->uploadReceipt,
        active_->uploadAttemptsUsed, active_->maxUploadAttempts, active_->hasAcceptedUpload,
        active_->canAdmitUpload()});
}

std::vector<UploadEventSnapshot> SessionService::drainUploadEvents(std::string_view sessionId) {
    std::lock_guard lock(mutex_);
    if (!active_ || active_->sessionId != sessionId) return {};
    std::vector<UploadEventSnapshot> events;
    events.reserve(uploadEvents_.size());
    while (!uploadEvents_.empty()) {
        events.push_back(std::move(uploadEvents_.front()));
        uploadEvents_.pop_front();
    }
    return events;
}

std::optional<Entity> SessionService::snapshot() const {
    std::lock_guard lock(mutex_);
    return active_;
}
} // namespace RoadSign::RecognitionSession
