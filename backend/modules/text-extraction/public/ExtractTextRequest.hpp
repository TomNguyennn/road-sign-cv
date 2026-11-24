#pragma once

#include "shared/Frame.hpp"
#include "shared/Geometry.hpp"

#include <cstdint>

namespace RoadSign::TextExtraction {
struct ExtractTextRequest {
    Frame frame;
    Box region;
    std::uint32_t epoch;
};
} // namespace RoadSign::TextExtraction
