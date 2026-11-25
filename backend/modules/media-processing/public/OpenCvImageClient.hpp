#pragma once

#include "MediaSource.hpp"
#include "../../../shared/Geometry.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace RoadSign::MediaProcessing {
struct ImageLimits {
    std::size_t maxBytes;
    std::uint32_t maxPixels;
    std::uint32_t maxWidth;
    std::uint32_t maxHeight;
};

struct DecodedImage {
    ImageSize size;
    std::vector<std::uint8_t> rgbPixels;
};

class OpenCvImageClient {
public:
    [[nodiscard]] static bool isPng(std::string_view encoded) noexcept;
    [[nodiscard]] static std::optional<DecodedImage> decode(
        std::string_view encoded, SourceKind kind, ImageLimits limits);
    [[nodiscard]] static std::optional<DecodedImage> decodeOwned(
        const MediaSource& source, const std::filesystem::path& spoolRoot, ImageLimits limits);
};
} // namespace RoadSign::MediaProcessing
