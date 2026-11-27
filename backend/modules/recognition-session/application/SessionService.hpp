#pragma once

#include "../domain/RecognitionSession.hpp"

#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace RoadSign::RecognitionSession {
struct CreateResult {
    enum class Status { created, busy } status;
    std::optional<Entity> session;
};

struct UploadAdmissionResult {
    enum class Status {
        admitted, missing, forbidden, invalidAttempt, wrongSource, staleEpoch,
        duplicateAttempt, sourceAccepted, exhausted, busy
    } status;
    std::optional<std::string> nonce;
    std::optional<Entity> snapshot;
};

struct UploadEventSnapshot {
    std::uint32_t epoch;
    std::uint64_t stateRevision;
    UploadReceipt receipt;
    std::uint32_t uploadAttemptsUsed;
    std::uint32_t maxUploadAttempts;
    bool hasAcceptedUpload;
    bool canRetryUpload;
};

class SessionService {
public:
    explicit SessionService(std::uint32_t maxUploadAttempts, std::string trustedOrigin);

    [[nodiscard]] CreateResult create(SourceKind sourceKind, std::string_view origin,
                                      std::chrono::steady_clock::time_point now);
    [[nodiscard]] bool attachEvents(std::string_view sessionId,
                                    std::chrono::steady_clock::time_point now);
    void expireUnattached(std::chrono::steady_clock::time_point now);
    [[nodiscard]] bool authenticate(std::string_view sessionId, std::string_view csrfToken,
                                    std::string_view origin) const;
    [[nodiscard]] bool heartbeat(std::string_view sessionId, std::string_view csrfToken,
                                 std::string_view origin, std::uint32_t epoch,
                                 std::chrono::steady_clock::time_point now);
    [[nodiscard]] std::optional<std::uint64_t> beginEnd(
        std::string_view sessionId, std::string_view csrfToken, std::string_view origin,
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] bool completeCleanup(std::string_view sessionId);
    void shutdown() noexcept;
    [[nodiscard]] bool acceptsCallback(std::string_view sessionId, std::uint32_t epoch,
                                       std::uint32_t audioGeneration) const;
    [[nodiscard]] bool beginProcessing(std::string_view sessionId);
    [[nodiscard]] bool finishStillImage(std::string_view sessionId);
    [[nodiscard]] bool advanceVideoPosition(std::string_view sessionId, std::uint32_t epoch,
                                            std::uint64_t positionMs);
    [[nodiscard]] bool finishVideo(std::string_view sessionId, std::uint32_t epoch);
    [[nodiscard]] std::optional<Entity> seekVideo(
        std::string_view sessionId, std::string_view csrfToken, std::string_view origin,
        std::uint32_t expectedEpoch, std::uint64_t positionMs);
    [[nodiscard]] UploadAdmissionResult admitUpload(
        std::string_view sessionId, std::string_view csrfToken, std::string_view origin,
        std::uint32_t epoch, std::string uploadAttemptId,
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] bool setUploadValidating(std::string_view sessionId,
                                           std::string_view uploadAttemptId,
                                           std::string_view admissionNonce,
                                           std::chrono::steady_clock::time_point now);
    [[nodiscard]] bool finishUpload(std::string_view sessionId,
                                    std::string_view uploadAttemptId, bool accepted,
                                    std::optional<std::string> safeErrorCode = std::nullopt,
                                    std::optional<MediaProcessing::MediaSource> media = std::nullopt);
    [[nodiscard]] std::optional<std::string> expireUploadAdmission(
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] std::optional<Entity> snapshot() const;
    [[nodiscard]] std::vector<UploadEventSnapshot> drainUploadEvents(std::string_view sessionId);

private:
    [[nodiscard]] static std::string randomUuid();
    [[nodiscard]] static std::string randomToken();
    [[nodiscard]] bool authenticateLocked(std::string_view sessionId,
                                          std::string_view csrfToken,
                                          std::string_view origin) const;
    void queueUploadEventLocked();

    const std::uint32_t maxUploadAttempts_;
    const std::string trustedOrigin_;
    mutable std::mutex mutex_;
    std::optional<Entity> active_;
    std::deque<UploadEventSnapshot> uploadEvents_;
};
} // namespace RoadSign::RecognitionSession
