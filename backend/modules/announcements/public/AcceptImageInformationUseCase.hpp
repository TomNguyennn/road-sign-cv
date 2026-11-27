#pragma once

#include "shared/Geometry.hpp"
#include "modules/sign-recognition/public/SignObservation.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace RoadSign::Announcements {
struct StillImageDescription {
    std::string text;
    std::uint8_t priority;
};

class AcceptImageInformationUseCase {
public:
    [[nodiscard]] static std::optional<StillImageDescription> describeObstacle(
        const SignRecognition::SignObservation& observation, ImageSize imageSize);
};
} // namespace RoadSign::Announcements
