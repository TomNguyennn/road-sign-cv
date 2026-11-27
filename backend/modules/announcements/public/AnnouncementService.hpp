#pragma once

#include "AcceptedInformation.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace RoadSign::Announcements {
using AnnouncementTime = std::chrono::steady_clock::time_point;

enum class AnnouncementState { pending, synthesizing, offered, playing };

struct AnnouncementDto {
    std::string announcementId;
    std::string trackId;
    std::uint32_t revision;
    std::uint64_t frameId;
    std::optional<double> clientCaptureTimeMs;
    std::string text;
    std::uint8_t priority;
    AnnouncementTime createdAt;
    AnnouncementState state;
};

struct EnqueueDecisionDto {
    bool isQueued;
    std::optional<std::string> droppedAnnouncementId;
};

class AnnouncementService {
public:
    [[nodiscard]] EnqueueDecisionDto enqueue(const AcceptedInformation& information,
                                             std::string announcementId, AnnouncementTime now);
    [[nodiscard]] std::optional<AnnouncementDto> beginSynthesis(AnnouncementTime now);
    [[nodiscard]] bool offer(std::string_view announcementId, AnnouncementTime now);
    [[nodiscard]] bool started(std::string_view announcementId, AnnouncementTime now);
    [[nodiscard]] bool completed(std::string_view announcementId);
    [[nodiscard]] bool failedSynthesis(std::string_view announcementId);
    [[nodiscard]] bool invalidateTrack(std::string_view trackId,
                                       std::optional<std::uint32_t> currentRevision,
                                       std::string cancellationId,
                                       std::uint32_t cancelThroughAudioGeneration,
                                       AnnouncementTime now);
    [[nodiscard]] bool expire(AnnouncementTime now, std::string cancellationId,
                              std::uint32_t cancelThroughAudioGeneration);
    [[nodiscard]] bool acknowledgeCancellation(std::string_view cancellationId,
                                                std::uint32_t cancelThroughAudioGeneration,
                                                AnnouncementTime now);
    void forgetTrack(std::string_view trackId);
    void reset() noexcept;

    [[nodiscard]] std::size_t pendingCount() const noexcept { return pending_.size(); }
    [[nodiscard]] const std::optional<AnnouncementDto>& active() const noexcept { return active_; }
    [[nodiscard]] bool hasCancellationBarrier() const noexcept { return barrier_.has_value(); }
    [[nodiscard]] bool isEnded() const noexcept { return isEnded_; }

private:
    struct CancellationBarrier {
        std::string cancellationId;
        std::uint32_t generationCutoff;
        AnnouncementTime deadline;
    };

    void expirePending(AnnouncementTime now);
    [[nodiscard]] bool hasSignature(std::string_view trackId, std::uint32_t revision) const;
    void startCancellation(std::string cancellationId,
                           std::uint32_t cancelThroughAudioGeneration, AnnouncementTime now);
    [[nodiscard]] bool barrierTimedOut(AnnouncementTime now) noexcept;

    std::vector<AnnouncementDto> pending_;
    std::optional<AnnouncementDto> active_;
    std::unordered_map<std::string, std::uint32_t> startedRevisionByTrack_;
    std::optional<CancellationBarrier> barrier_;
    bool isEnded_{};
};
} // namespace RoadSign::Announcements
