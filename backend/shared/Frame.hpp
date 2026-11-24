#pragma once

#include "Geometry.hpp"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace RoadSign {
class Frame {
public:
    [[nodiscard]] static std::optional<Frame> copyOf(
        std::uint64_t frameId, double sourceTimeMs,
        std::optional<double> clientCaptureTimeMs, ImageSize size,
        std::span<const std::uint8_t> pixels, std::size_t channels = 3) {
        const auto required = size.pixelBytes(channels);
        if (!required || pixels.size() != *required || !std::isfinite(sourceTimeMs) ||
            sourceTimeMs < 0.0 ||
            (clientCaptureTimeMs &&
             (!std::isfinite(*clientCaptureTimeMs) || *clientCaptureTimeMs < 0.0)))
            return std::nullopt;
        return Frame(frameId, sourceTimeMs, clientCaptureTimeMs, size,
                     std::make_shared<const std::vector<std::uint8_t>>(pixels.begin(), pixels.end()));
    }

    [[nodiscard]] std::uint64_t frameId() const noexcept { return frameId_; }
    [[nodiscard]] double sourceTimeMs() const noexcept { return sourceTimeMs_; }
    [[nodiscard]] std::optional<double> clientCaptureTimeMs() const noexcept {
        return clientCaptureTimeMs_;
    }
    [[nodiscard]] ImageSize size() const noexcept { return size_; }
    [[nodiscard]] const std::shared_ptr<const std::vector<std::uint8_t>>& pixels() const noexcept {
        return pixels_;
    }

private:
    Frame(std::uint64_t frameId, double sourceTimeMs,
          std::optional<double> clientCaptureTimeMs, ImageSize size,
          std::shared_ptr<const std::vector<std::uint8_t>> pixels)
        : frameId_(frameId), sourceTimeMs_(sourceTimeMs),
          clientCaptureTimeMs_(clientCaptureTimeMs), size_(size), pixels_(std::move(pixels)) {}

    std::uint64_t frameId_;
    double sourceTimeMs_;
    std::optional<double> clientCaptureTimeMs_;
    ImageSize size_;
    std::shared_ptr<const std::vector<std::uint8_t>> pixels_;
};
}
