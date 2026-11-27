#pragma once

#include "MediaSource.hpp"
#include "OpenCvImageClient.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

namespace RoadSign::MediaProcessing {
struct VideoLimits {
    std::uint64_t maxBytes;
    std::uint32_t maxWidth;
    std::uint32_t maxHeight;
    double maxDurationSeconds;
    double maxFramesPerSecond;
};

struct DecodedVideoFrame {
    std::uint64_t frameId;
    std::uint64_t sourceTimeMs;
    DecodedImage image;
};

class OpenCvVideoClient {
public:
    ~OpenCvVideoClient();
    OpenCvVideoClient(const OpenCvVideoClient&) = delete;
    OpenCvVideoClient& operator=(const OpenCvVideoClient&) = delete;
    [[nodiscard]] static std::unique_ptr<OpenCvVideoClient> open(
        const MediaSource& source, const std::filesystem::path& spoolRoot, VideoLimits limits);

    [[nodiscard]] std::optional<DecodedVideoFrame> nextSample(std::uint64_t minimumSourceTimeMs);
    [[nodiscard]] bool seek(std::uint64_t positionMs);
    [[nodiscard]] bool hasFailed() const noexcept { return isFailed_; }

private:
    struct State;
    OpenCvVideoClient(std::unique_ptr<State> state, MediaSource source);

    std::unique_ptr<State> state_;
    MediaSource source_;
    std::uint64_t nextFrameId_{1};
    std::optional<std::uint64_t> lastDecodedTimeMs_;
    std::optional<std::uint64_t> lastSampleTimeMs_;
    bool isFailed_{};
};
} // namespace RoadSign::MediaProcessing
