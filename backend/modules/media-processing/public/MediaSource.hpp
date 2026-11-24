#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

namespace RoadSign::MediaProcessing {
enum class SourceKind { camera, image, video };

struct MediaSource {
    SourceKind kind;
    std::uint32_t width;
    std::uint32_t height;
    std::optional<std::uint64_t> durationMs;
    std::optional<std::uint64_t> positionMs;
    std::filesystem::path ownedTemporaryFile;
};
} // namespace RoadSign::MediaProcessing
