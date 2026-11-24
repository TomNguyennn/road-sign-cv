#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace RoadSign {
struct ImageSize {
    std::uint32_t width{};
    std::uint32_t height{};

    [[nodiscard]] std::optional<std::size_t> pixelBytes(std::size_t channels) const noexcept {
        if (width == 0 || height == 0 || channels == 0) return std::nullopt;
        const auto max = std::numeric_limits<std::size_t>::max();
        if (width > max / height || static_cast<std::size_t>(width) * height > max / channels)
            return std::nullopt;
        return static_cast<std::size_t>(width) * height * channels;
    }
};

struct Box {
    double x{};
    double y{};
    double width{};
    double height{};

    [[nodiscard]] bool isFinitePositive() const noexcept {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(width) &&
               std::isfinite(height) && width > 0.0 && height > 0.0;
    }
    [[nodiscard]] std::optional<Box> clipped(ImageSize size) const noexcept {
        if (!isFinitePositive() || size.width == 0 || size.height == 0) return std::nullopt;
        const double left = std::clamp(x, 0.0, static_cast<double>(size.width));
        const double top = std::clamp(y, 0.0, static_cast<double>(size.height));
        const double right = std::clamp(x + width, 0.0, static_cast<double>(size.width));
        const double bottom = std::clamp(y + height, 0.0, static_cast<double>(size.height));
        if (right <= left || bottom <= top) return std::nullopt;
        return Box{left, top, right - left, bottom - top};
    }
};
}
