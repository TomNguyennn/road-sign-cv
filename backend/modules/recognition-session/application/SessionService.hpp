#pragma once

#include "../domain/RecognitionSession.hpp"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

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

class SessionService {
public:
    explicit SessionService(std::uint32_t maxUploadAttempts, std::string trustedOrigin);

    [[nodiscard]] CreateResult create(SourceKind sourceKind, std::string_view origin,
                                      std::chrono::steady_clock::time_point now);
    [[nodiscard]] bool attachEvents(std::string_view sessionId,
                                    std::chrono::steady_clock::time_point now);
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

private:
    [[nodiscard]] static std::string randomUuid();
    [[nodiscard]] static std::string randomToken();
    [[nodiscard]] bool authenticateLocked(std::string_view sessionId,
                                          std::string_view csrfToken,
                                          std::string_view origin) const;

    const std::uint32_t maxUploadAttempts_;
    const std::string trustedOrigin_;
    mutable std::mutex mutex_;
    std::optional<Entity> active_;
};
} // namespace RoadSign::RecognitionSession
