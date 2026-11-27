#include "modules/media-processing/public/OpenCvVideoClient.hpp"

#include <opencv2/imgproc.hpp>

#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace RoadSign::MediaProcessing {
namespace {
constexpr std::uint64_t SAMPLE_INTERVAL_MS = 200;

enum class Container { mp4, webm, unsupported };

Container containerOf(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::array<char, 12> header{};
    if (!input.read(header.data(), header.size())) {
        return Container::unsupported;
    }
    if (std::string_view(header.data() + 4, 4) == "ftyp") {
        return Container::mp4;
    }
    constexpr std::array<unsigned char, 4> webmSignature{0x1A, 0x45, 0xDF, 0xA3};
    for (std::size_t index = 0; index < webmSignature.size(); ++index) {
        if (static_cast<unsigned char>(header[index]) != webmSignature[index]) {
            return Container::unsupported;
        }
    }
    return Container::webm;
}

bool hasAllowedCodec(const cv::VideoCapture& capture, Container container) {
    const auto code = static_cast<std::uint32_t>(capture.get(cv::CAP_PROP_FOURCC));
    std::string fourcc(4, '\0');
    for (std::size_t index = 0; index < fourcc.size(); ++index) {
        const auto character = static_cast<unsigned char>((code >> (index * 8U)) & 0xFFU);
        fourcc[index] = static_cast<char>(std::tolower(character));
    }
    if (container == Container::mp4) {
        return fourcc == "avc1" || fourcc == "avc3" || fourcc == "h264";
    }
    return fourcc == "vp80" || fourcc == "vp90" || fourcc == "vp08" || fourcc == "vp09";
}

bool isTrustedFile(const std::filesystem::path& path, const std::filesystem::path& spoolRoot,
                   std::uint64_t maxBytes) {
    std::error_code error;
    if (std::filesystem::symlink_status(path, error).type() != std::filesystem::file_type::regular) {
        return false;
    }
    const auto root = std::filesystem::weakly_canonical(spoolRoot, error);
    if (error) return false;
    const auto resolved = std::filesystem::weakly_canonical(path, error);
    if (error) return false;
    const auto relative = resolved.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
        return false;
    }
    const auto bytes = std::filesystem::file_size(resolved, error);
    return !error && bytes > 0 && bytes <= maxBytes;
}

bool hasValidMetadata(const cv::VideoCapture& capture, const MediaSource& source,
                      VideoLimits limits) {
    const auto width = capture.get(cv::CAP_PROP_FRAME_WIDTH);
    const auto height = capture.get(cv::CAP_PROP_FRAME_HEIGHT);
    const auto frameRate = capture.get(cv::CAP_PROP_FPS);
    const auto frameCount = capture.get(cv::CAP_PROP_FRAME_COUNT);
    if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(frameRate) ||
        !std::isfinite(frameCount) || width < 1 || height < 1 || frameRate < 1 ||
        frameCount < 1 || width > limits.maxWidth || height > limits.maxHeight ||
        frameRate > limits.maxFramesPerSecond || frameCount / frameRate > limits.maxDurationSeconds) {
        return false;
    }
    return static_cast<std::uint32_t>(width) == source.width &&
        static_cast<std::uint32_t>(height) == source.height &&
        source.durationMs && *source.durationMs <= limits.maxDurationSeconds * 1000.0;
}
} // namespace

OpenCvVideoClient::OpenCvVideoClient(cv::VideoCapture capture, MediaSource source)
    : capture_(std::move(capture)), source_(std::move(source)) {}

std::unique_ptr<OpenCvVideoClient> OpenCvVideoClient::open(
    const MediaSource& source, const std::filesystem::path& spoolRoot, VideoLimits limits) {
    if (source.kind != SourceKind::video || !source.ownedTemporaryFile ||
        !isTrustedFile(source.ownedTemporaryFile->path, spoolRoot, limits.maxBytes)) {
        return nullptr;
    }
    const auto container = containerOf(source.ownedTemporaryFile->path);
    if (container == Container::unsupported) return nullptr;
    cv::VideoCapture capture(source.ownedTemporaryFile->path.string(), cv::CAP_FFMPEG);
    if (!capture.isOpened() || !hasAllowedCodec(capture, container) ||
        !hasValidMetadata(capture, source, limits)) {
        return nullptr;
    }
    return std::unique_ptr<OpenCvVideoClient>(
        new OpenCvVideoClient(std::move(capture), source));
}

std::optional<DecodedVideoFrame> OpenCvVideoClient::nextSample(
    std::uint64_t minimumSourceTimeMs) {
    if (isFailed_ || !capture_.isOpened()) return std::nullopt;
    cv::Mat bgr;
    while (capture_.read(bgr)) {
        const double timestamp = capture_.get(cv::CAP_PROP_POS_MSEC);
        if (bgr.empty() || bgr.cols != static_cast<int>(source_.width) ||
            bgr.rows != static_cast<int>(source_.height) || !std::isfinite(timestamp) ||
            timestamp < 0 || timestamp > static_cast<double>(*source_.durationMs) + 1000.0) {
            isFailed_ = true;
            return std::nullopt;
        }
        const auto sourceTimeMs = static_cast<std::uint64_t>(timestamp);
        if (lastDecodedTimeMs_ && sourceTimeMs <= *lastDecodedTimeMs_) {
            continue;
        }
        lastDecodedTimeMs_ = sourceTimeMs;
        if (sourceTimeMs < minimumSourceTimeMs ||
            (lastSampleTimeMs_ && sourceTimeMs - *lastSampleTimeMs_ < SAMPLE_INTERVAL_MS)) {
            continue;
        }
        try {
            cv::Mat rgb;
            cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
            if (!rgb.isContinuous()) {
                isFailed_ = true;
                return std::nullopt;
            }
            lastSampleTimeMs_ = sourceTimeMs;
            const auto pixelCount = rgb.total() * rgb.elemSize();
            return DecodedVideoFrame{nextFrameId_++, sourceTimeMs,
                {{source_.width, source_.height}, {rgb.data, rgb.data + pixelCount}}};
        } catch (const cv::Exception&) {
            isFailed_ = true;
            return std::nullopt;
        }
    }
    return std::nullopt;
}

bool OpenCvVideoClient::seek(std::uint64_t positionMs) {
    if (!source_.durationMs || positionMs >= *source_.durationMs ||
        !capture_.set(cv::CAP_PROP_POS_MSEC, static_cast<double>(positionMs))) {
        return false;
    }
    lastDecodedTimeMs_.reset();
    lastSampleTimeMs_.reset();
    isFailed_ = false;
    return true;
}
} // namespace RoadSign::MediaProcessing
