#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace RoadSign::Announcements {
struct AcceptedInformation {
    std::string trackId;
    std::uint32_t revision;
    std::uint32_t categoryId;
    std::string displayText;
    std::string normalizedText;
    std::uint8_t priority;
    std::uint64_t frameId{};
    std::optional<double> clientCaptureTimeMs;
};
} // namespace RoadSign::Announcements
