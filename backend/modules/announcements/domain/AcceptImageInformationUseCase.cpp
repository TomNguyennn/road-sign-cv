#include "modules/announcements/public/AcceptImageInformationUseCase.hpp"

namespace RoadSign::Announcements {
std::optional<StillImageDescription> AcceptImageInformationUseCase::describeObstacle(
    const SignRecognition::SignObservation& observation, ImageSize imageSize) {
    constexpr std::uint32_t obstacleCategory = 7;
    constexpr double stillScore = 0.75;
    if (observation.categoryId != obstacleCategory || observation.confidence < stillScore ||
        !observation.isValid(0, 0, imageSize)) return std::nullopt;

    return describePosition(observation.box, imageSize);
}

std::optional<StillImageDescription> AcceptImageInformationUseCase::describePosition(
    Box box, ImageSize imageSize) {
    const auto visibleBox = box.clipped(imageSize);
    if (!visibleBox) return std::nullopt;
    const double centre = (visibleBox->x + visibleBox->width / 2) / imageSize.width;
    if (centre < 1.0 / 3.0)
        return StillImageDescription{"Possible obstacle on the left of the camera view", 0};
    if (centre < 2.0 / 3.0)
        return StillImageDescription{"Possible obstacle in the centre of the camera view", 0};
    return StillImageDescription{"Possible obstacle on the right of the camera view", 0};
}
} // namespace RoadSign::Announcements
