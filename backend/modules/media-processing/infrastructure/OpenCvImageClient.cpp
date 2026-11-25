#include "../public/OpenCvImageClient.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <array>
#include <climits>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>

namespace RoadSign::MediaProcessing {
namespace {
std::uint32_t readBigEndian(const unsigned char* value) {
    return (static_cast<std::uint32_t>(value[0]) << 24U) |
           (static_cast<std::uint32_t>(value[1]) << 16U) |
           (static_cast<std::uint32_t>(value[2]) << 8U) |
           static_cast<std::uint32_t>(value[3]);
}

std::optional<ImageSize> pngSize(std::string_view bytes) {
    constexpr std::array<unsigned char, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
    if (bytes.size() < 24 || std::memcmp(bytes.data(), signature.data(), signature.size()) != 0 ||
        bytes.substr(12, 4) != "IHDR") return std::nullopt;
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.data() + 16);
    const auto width = readBigEndian(data);
    const auto height = readBigEndian(data + 4);
    if (width == 0 || height == 0 || width > static_cast<std::uint32_t>(INT_MAX) ||
        height > static_cast<std::uint32_t>(INT_MAX)) return std::nullopt;
    return ImageSize{width, height};
}

bool isJpegStart(std::string_view bytes) {
    return bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
           static_cast<unsigned char>(bytes[1]) == 0xD8 &&
           static_cast<unsigned char>(bytes[2]) == 0xFF;
}

bool isJpegStartOfFrame(unsigned char marker) {
    switch (marker) {
    case 0xC0: case 0xC1: case 0xC2: case 0xC3:
    case 0xC5: case 0xC6: case 0xC7:
    case 0xC9: case 0xCA: case 0xCB:
    case 0xCD: case 0xCE: case 0xCF: return true;
    default: return false;
    }
}

std::optional<ImageSize> jpegSize(std::string_view bytes) {
    if (!isJpegStart(bytes)) return std::nullopt;
    std::size_t offset = 2;
    while (offset + 4 <= bytes.size()) {
        if (static_cast<unsigned char>(bytes[offset]) != 0xFF) return std::nullopt;
        while (offset < bytes.size() && static_cast<unsigned char>(bytes[offset]) == 0xFF) ++offset;
        if (offset >= bytes.size()) return std::nullopt;
        const auto marker = static_cast<unsigned char>(bytes[offset++]);
        if (marker == 0xD8 || marker == 0xD9 || marker == 0x01 ||
            (marker >= 0xD0 && marker <= 0xD7)) continue;
        if (offset + 2 > bytes.size()) return std::nullopt;
        const auto segmentLength = static_cast<std::size_t>(
            (static_cast<unsigned char>(bytes[offset]) << 8U) |
            static_cast<unsigned char>(bytes[offset + 1]));
        if (segmentLength < 2 || segmentLength > bytes.size() - offset) return std::nullopt;
        if (isJpegStartOfFrame(marker)) {
            if (segmentLength < 7) return std::nullopt;
            const auto* data = reinterpret_cast<const unsigned char*>(bytes.data() + offset + 2);
            const auto height = static_cast<std::uint32_t>((data[1] << 8U) | data[2]);
            const auto width = static_cast<std::uint32_t>((data[3] << 8U) | data[4]);
            if (width == 0 || height == 0) return std::nullopt;
            return ImageSize{width, height};
        }
        offset += segmentLength;
    }
    return std::nullopt;
}

bool withinLimits(ImageSize size, ImageLimits limits) {
    return size.width > 0 && size.height > 0 && size.width <= limits.maxWidth &&
           size.height <= limits.maxHeight &&
           static_cast<std::uint64_t>(size.width) * size.height <= limits.maxPixels;
}
} // namespace

bool OpenCvImageClient::isPng(std::string_view encoded) noexcept {
    return pngSize(encoded).has_value();
}

std::optional<DecodedImage> OpenCvImageClient::decode(
    std::string_view encoded, SourceKind kind, ImageLimits limits) {
    if (kind == SourceKind::video || encoded.empty() || encoded.size() > limits.maxBytes)
        return std::nullopt;
    auto declared = kind == SourceKind::image ? pngSize(encoded) : std::nullopt;
    if (!declared) declared = jpegSize(encoded);
    if (!declared || !withinLimits(*declared, limits)) return std::nullopt;
    const auto* begin = reinterpret_cast<const unsigned char*>(encoded.data());
    try {
        const std::vector<unsigned char> buffer(begin, begin + encoded.size());
        const cv::Mat bgr = cv::imdecode(buffer, cv::IMREAD_COLOR);
        if (bgr.empty() || !withinLimits({static_cast<std::uint32_t>(bgr.cols),
                                          static_cast<std::uint32_t>(bgr.rows)}, limits))
            return std::nullopt;
        cv::Mat rgb;
        cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
        if (!rgb.isContinuous()) return std::nullopt;
        return DecodedImage{{static_cast<std::uint32_t>(rgb.cols),
                             static_cast<std::uint32_t>(rgb.rows)},
                            {rgb.data, rgb.data + rgb.total() * rgb.elemSize()}};
    } catch (const cv::Exception&) {
        return std::nullopt;
    }
}

std::optional<DecodedImage> OpenCvImageClient::decodeOwned(
    const MediaSource& source, const std::filesystem::path& spoolRoot, ImageLimits limits) {
    if (source.kind != SourceKind::image || !source.ownedTemporaryFile) return std::nullopt;
    std::error_code error;
    const auto root = std::filesystem::weakly_canonical(spoolRoot, error);
    if (error) return std::nullopt;
    const auto path = source.ownedTemporaryFile->path;
    if (std::filesystem::symlink_status(path, error).type() != std::filesystem::file_type::regular)
        return std::nullopt;
    const auto resolved = std::filesystem::weakly_canonical(path, error);
    if (error) return std::nullopt;
    const auto relative = resolved.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
        return std::nullopt;
    const auto size = std::filesystem::file_size(resolved, error);
    if (error || size == 0 || size > limits.maxBytes ||
        size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
        return std::nullopt;
    std::ifstream file(resolved, std::ios::binary);
    if (!file) return std::nullopt;
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) return std::nullopt;
    auto decoded = decode(bytes, source.kind, limits);
    if (!decoded || decoded->size.width != source.width || decoded->size.height != source.height)
        return std::nullopt;
    return decoded;
}
} // namespace RoadSign::MediaProcessing
