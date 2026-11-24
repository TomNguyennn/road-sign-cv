#pragma once

#include "shared/Geometry.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace RoadSign::SignRecognition {
struct SignObservation {
    std::uint64_t frameId;
    std::uint32_t categoryId;
    double confidence;
    Box box;
    std::optional<std::string> textCandidate;
    std::optional<double> ocrConfidence;
    double sourceTimeMs;

    [[nodiscard]] bool isValid(std::uint32_t epoch, std::uint32_t expectedEpoch,
                               ImageSize frameSize) const noexcept {
        return epoch == expectedEpoch && categoryId < 8 && confidence >= 0.0 &&
               confidence <= 1.0 && std::isfinite(confidence) &&
               std::isfinite(sourceTimeMs) && sourceTimeMs >= 0.0 &&
               (!ocrConfidence || (std::isfinite(*ocrConfidence) &&
                                    *ocrConfidence >= 0.0 && *ocrConfidence <= 1.0)) &&
               box.clipped(frameSize).has_value();
    }
};
} // namespace RoadSign::SignRecognition
