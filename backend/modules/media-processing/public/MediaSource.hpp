#pragma once

#include <cstdint>
#include <memory>
#include <filesystem>
#include <optional>
#include <system_error>

namespace RoadSign::MediaProcessing {
enum class SourceKind { camera, image, video };

struct OwnedTemporaryFile {
    std::filesystem::path path;

    ~OwnedTemporaryFile() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};

struct MediaSource {
    SourceKind kind;
    std::uint32_t width;
    std::uint32_t height;
    std::optional<std::uint64_t> durationMs;
    std::optional<std::uint64_t> positionMs;
    std::shared_ptr<const OwnedTemporaryFile> ownedTemporaryFile;
};
} // namespace RoadSign::MediaProcessing
