#include "SessionService.hpp"

#include <uuid/uuid.h>

#include <algorithm>
#include <stdexcept>

namespace RoadSign::RecognitionSession {
namespace {
constexpr auto kHeartbeatLease = std::chrono::seconds{15};
constexpr auto kEventAttachWindow = std::chrono::seconds{10};

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

CreateResult SessionService::create(SourceKind sourceKind, std::string_view origin,
                                    std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(mutex_);
    if (origin != trustedOrigin_ || active_) return {CreateResult::Status::busy, std::nullopt};
    Entity session{};
    session.sessionId = randomUuid();
    session.sourceKind = sourceKind;
    session.csrfToken = randomUuid();
    session.maxUploadAttempts = maxUploadAttempts_;
    session.eventAttachDeadline = now + kEventAttachWindow;
    session.heartbeatDeadline = now + kHeartbeatLease;
    active_ = session;
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

bool SessionService::beginUpload(std::string_view sessionId, std::string uploadAttemptId) {
    std::lock_guard lock(mutex_);
    if (!active_ || active_->sessionId != sessionId || !active_->canAdmitUpload() ||
        uploadAttemptId.empty())
        return false;
    ++active_->uploadAttemptsUsed;
    active_->uploadReceipt = {std::move(uploadAttemptId), UploadStatus::receiving, std::nullopt};
    ++active_->stateRevision;
    return true;
}

bool SessionService::setUploadValidating(std::string_view sessionId,
                                         std::string_view uploadAttemptId) {
    std::lock_guard lock(mutex_);
    if (!active_ || active_->sessionId != sessionId ||
        active_->uploadReceipt.uploadAttemptId != uploadAttemptId ||
        active_->uploadReceipt.status != UploadStatus::receiving)
        return false;
    active_->uploadReceipt.status = UploadStatus::validating;
    ++active_->stateRevision;
    return true;
}

bool SessionService::finishUpload(std::string_view sessionId,
                                  std::string_view uploadAttemptId, bool accepted,
                                  std::optional<std::string> safeErrorCode) {
    std::lock_guard lock(mutex_);
    if (!active_ || active_->sessionId != sessionId ||
        active_->uploadReceipt.uploadAttemptId != uploadAttemptId ||
        active_->uploadReceipt.status != UploadStatus::validating ||
        (accepted && safeErrorCode) || (!accepted && !safeErrorCode))
        return false;
    active_->uploadReceipt.status = accepted ? UploadStatus::accepted : UploadStatus::rejected;
    active_->uploadReceipt.errorCode = std::move(safeErrorCode);
    active_->hasAcceptedUpload = accepted;
    active_->uploadAdmissionNonce.reset();
    active_->uploadAdmissionDeadline.reset();
    ++active_->stateRevision;
    return true;
}

std::optional<Entity> SessionService::snapshot() const {
    std::lock_guard lock(mutex_);
    return active_;
}
} // namespace RoadSign::RecognitionSession
