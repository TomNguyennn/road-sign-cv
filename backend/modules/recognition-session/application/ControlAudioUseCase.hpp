#pragma once

#include "../domain/RecognitionSession.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace RoadSign::RecognitionSession {
enum class AudioControlStatus { accepted, empty, muted, activationRequired, staleDescription };

struct AudioOffer {
    std::string announcementId;
    std::string descriptionId;
    std::string text;
    std::string origin;
    bool isReplay{};
    std::optional<std::string> trackId;
    std::optional<std::uint32_t> revision;
    std::optional<std::uint64_t> frameId;
    std::optional<double> clientCaptureTimeMs;
};

struct AudioCancellation {
    std::string cancellationId;
    std::uint32_t cancelThroughAudioGeneration;
    std::chrono::steady_clock::time_point deadline;
};

struct AudioControlResult {
    AudioControlStatus status;
    std::optional<AudioOffer> offer;
    std::optional<AudioCancellation> cancellation;
};

class ControlAudioUseCase {
public:
    explicit ControlAudioUseCase(Entity& session) : session_(session) {}

    void acceptDescription(std::string descriptionId, std::string text,
                           std::chrono::steady_clock::time_point now);
    [[nodiscard]] std::optional<AudioOffer> offerAutomatic(
        std::string_view descriptionId, std::string trackId, std::uint32_t revision,
        std::uint64_t frameId, std::chrono::steady_clock::time_point now);
    [[nodiscard]] AudioControlResult mute(std::chrono::steady_clock::time_point now);
    [[nodiscard]] AudioControlResult unmute(std::chrono::steady_clock::time_point now);
    [[nodiscard]] AudioControlResult enable(std::chrono::steady_clock::time_point now);
    [[nodiscard]] AudioControlResult repeat(std::chrono::steady_clock::time_point now);
    [[nodiscard]] AudioControlResult recover(std::string_view descriptionId, bool activated,
                                             std::chrono::steady_clock::time_point now);
    [[nodiscard]] bool recordPlaybackStarted(std::string_view announcementId,
                                             std::string_view descriptionId,
                                             std::uint32_t generation);
    [[nodiscard]] bool recordPlaybackEnded(std::string_view announcementId,
                                           std::uint32_t generation);
    void recordPlaybackBlocked(std::string_view announcementId, std::uint32_t generation);
    [[nodiscard]] bool acknowledgeCancellation(std::string_view cancellationId,
                                                std::uint32_t cutoff);
    void expireCancellation(std::chrono::steady_clock::time_point now);
    void removeSourceHistory() { latestDescription_.reset(); }
    void clearHistory();
    [[nodiscard]] bool isActivated() const noexcept { return isActivated_; }
    [[nodiscard]] std::optional<AudioCancellation> pendingCancellation() const {
        return pendingCancellation_;
    }

private:
    struct Description { std::string id; std::string text; };
    [[nodiscard]] static std::string randomUuid();
    [[nodiscard]] AudioOffer manualOffer(const Description& description, std::string origin);

    Entity& session_;
    std::optional<Description> latestDescription_;
    std::optional<Description> latestStarted_;
    std::string lastAutomaticDescriptionId_;
    std::optional<AudioOffer> activeOffer_;
    std::optional<AudioCancellation> pendingCancellation_;
    std::optional<AudioCancellation> completedCancellation_;
    bool isActivated_{};
};
} // namespace RoadSign::RecognitionSession
