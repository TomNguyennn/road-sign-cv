#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace RoadSign::Speech {
struct AudioArtifact {
    std::uint64_t announcementId;
    std::shared_ptr<const std::vector<std::uint8_t>> wavBytes;
    std::uint32_t sampleRateHz;
    std::uint32_t durationMs;
    std::uint32_t epoch;
    std::uint32_t audioGeneration;
};
} // namespace RoadSign::Speech
