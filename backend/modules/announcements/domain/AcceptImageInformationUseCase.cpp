#include "modules/announcements/public/AcceptImageInformationUseCase.hpp"

namespace RoadSign::Announcements {
std::optional<StillImageDescription> AcceptImageInformationUseCase::describeObstacle(
    const SignRecognition::SignObservation& observation, ImageSize imageSize) {
    constexpr std::uint32_t obstacleCategory = 7;
    constexpr double stillScore = 0.75;
    if (observation.categoryId != obstacleCategory || observation.confidence < stillScore ||
        !observation.isValid(0, 0, imageSize)) return std::nullopt;

    const auto visibleBox = observation.box.clipped(imageSize);
    if (!visibleBox) return std::nullopt;
    const double centre = (visibleBox->x + visibleBox->width / 2) / imageSize.width;
    std::string position = "right";
    if (centre < 1.0 / 3.0) position = "left";
    else if (centre < 2.0 / 3.0) position = "centre";
    return StillImageDescription{"Possible obstacle on the " + position + " of the camera view", 0};
}
} // namespace RoadSign::Announcements
