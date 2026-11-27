#pragma once

#include "AcceptImageInformationUseCase.hpp"
#include "modules/sign-recognition/public/TrackEvidenceDto.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace RoadSign::Announcements {
struct ConsolidationDecisionDto {
    bool shouldConfirm;
    bool shouldScheduleOcr;
    std::optional<SignRecognition::TextReadingDto> acceptedText;
    std::optional<StillImageDescription> acceptedObstacle;
};

class ConsolidateInformationUseCase {
public:
    [[nodiscard]] ConsolidationDecisionDto evaluate(
        const SignRecognition::TrackEvidenceDto& evidence, ImageSize frameSize = {});
    void forget(std::string_view trackId);
    void reset() noexcept;

private:
    std::optional<std::uint64_t> currentFrameId_;
    std::size_t cropsThisFrame_{};
    std::unordered_map<std::string, double> lastOcrTimeByTrack_;
    std::unordered_map<std::string, double> obstacleLastSeenByTrack_;
};
} // namespace RoadSign::Announcements
