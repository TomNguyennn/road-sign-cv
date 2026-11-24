#include "SessionService.hpp"

#include <uuid/uuid.h>

#include <algorithm>

namespace RoadSign::RecognitionSession {
namespace {
constexpr auto kHeartbeatLease = std::chrono::seconds{15};
constexpr auto kEventAttachWindow = std::chrono::seconds{10};

} // namespace

SessionService::SessionService(std::uint32_t maxUploadAttempts, std::string trustedOrigin)
    : maxUploadAttempts_(std::min(maxUploadAttempts, 100U)),
      trustedOrigin_(std::move(trustedOrigin)) {}

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
    session.heartbeatDeadline = now + kHeartbeatLease;
    active_ = session;
    return {CreateResult::Status::created, session};
}

bool SessionService::attachEvents(std::string_view sessionId,
                                  std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(mutex_);
    if (!active_ || active_->sessionId != sessionId || active_->eventsAttached ||
        active_->state != State::created || now > active_->heartbeatDeadline -
                                                     (kHeartbeatLease - kEventAttachWindow))
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
                               std::string_view origin,
                               std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(mutex_);
    if (!authenticateLocked(sessionId, csrfToken, origin) || active_->state == State::ending)
        return false;
    active_->heartbeatDeadline = now + kHeartbeatLease;
    ++active_->stateRevision;
    return true;
}

bool SessionService::beginEnd(std::string_view sessionId, std::string_view csrfToken,
                              std::string_view origin,
                              std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(mutex_);
    if (!authenticateLocked(sessionId, csrfToken, origin) || active_->state == State::ending)
        return false;
    active_->state = State::ending;
    active_->endingSince = now;
    ++active_->stateRevision;
    return true;
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

bool SessionService::acceptsCallback(std::string_view sessionId, std::uint32_t epoch,
                                     std::uint32_t audioGeneration) const {
    std::lock_guard lock(mutex_);
    return active_ && active_->sessionId == sessionId && active_->state != State::ending &&
           active_->state != State::ended && active_->epoch == epoch &&
           active_->audioGeneration == audioGeneration;
}

std::optional<Entity> SessionService::snapshot() const {
    std::lock_guard lock(mutex_);
    return active_;
}
} // namespace RoadSign::RecognitionSession
