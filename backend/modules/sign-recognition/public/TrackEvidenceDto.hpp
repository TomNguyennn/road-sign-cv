#pragma once

#include "SignObservation.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace RoadSign::SignRecognition {
struct TextReadingDto {
    std::string displayText;
    std::string normalizedText;
    double confidence;
    double sourceTimeMs;
};

struct TrackEvidenceDto {
    std::string trackId;
    std::uint32_t categoryId;
    std::uint32_t revision;
    bool isActive;
    bool isConfirmed;
    double lastSourceTimeMs;
    std::vector<SignObservation> detections;
    std::vector<TextReadingDto> ocrSamples;
};
} // namespace RoadSign::SignRecognition
