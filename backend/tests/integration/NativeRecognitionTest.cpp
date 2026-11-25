#include "modules/media-processing/public/OpenCvImageClient.hpp"

#include <gtest/gtest.h>
#include <opencv2/imgcodecs.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

namespace RoadSign::MediaProcessing {
namespace {
std::string encode(const char* extension, cv::Size size) {
    cv::Mat bgr(size, CV_8UC3, cv::Scalar{0, 0, 255});
    std::vector<unsigned char> buffer;
    EXPECT_TRUE(cv::imencode(extension, bgr, buffer));
    return {reinterpret_cast<const char*>(buffer.data()), buffer.size()};
}

constexpr ImageLimits kImageLimits{10'485'760, 20'000'000, 20'000'000, 20'000'000};
constexpr ImageLimits kCameraLimits{1'048'576, 1280 * 720, 1280, 720};
} // namespace

TEST(NativeRecognitionTest, DecodesNonSquareImageToOwnedRgbPixels) {
    const auto encoded = encode(".png", {5, 3});
    const auto decoded = OpenCvImageClient::decode(encoded, SourceKind::image, kImageLimits);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->size.width, 5);
    EXPECT_EQ(decoded->size.height, 3);
    ASSERT_EQ(decoded->rgbPixels.size(), 45);
    EXPECT_EQ(decoded->rgbPixels[0], 255);
    EXPECT_EQ(decoded->rgbPixels[1], 0);
    EXPECT_EQ(decoded->rgbPixels[2], 0);
}

TEST(NativeRecognitionTest, CameraRequiresBoundedJpeg) {
    const auto jpeg = encode(".jpg", {5, 3});
    const auto png = encode(".png", {5, 3});
    EXPECT_TRUE(OpenCvImageClient::decode(jpeg, SourceKind::camera, kCameraLimits));
    EXPECT_FALSE(OpenCvImageClient::decode(png, SourceKind::camera, kCameraLimits));
    EXPECT_FALSE(OpenCvImageClient::decode(jpeg, SourceKind::camera, {1, 1280 * 720, 1280, 720}));
    EXPECT_FALSE(OpenCvImageClient::decode(jpeg, SourceKind::camera, {1'048'576, 4, 1280, 720}));
    EXPECT_FALSE(OpenCvImageClient::decode(jpeg, SourceKind::camera, {1'048'576, 1280 * 720, 4, 720}));
}

TEST(NativeRecognitionTest, AppliesExifOrientationBeforeCheckingFinalBounds) {
    auto jpeg = encode(".jpg", {5, 3});
    constexpr std::array<unsigned char, 36> orientationSix{
        0xFF, 0xE1, 0x00, 0x22, 'E', 'x', 'i', 'f', 0, 0, 'I', 'I', 0x2A, 0,
        8, 0, 0, 0, 1, 0, 0x12, 0x01, 3, 0, 1, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0};
    jpeg.insert(2, reinterpret_cast<const char*>(orientationSix.data()), orientationSix.size());
    const auto decoded = OpenCvImageClient::decode(jpeg, SourceKind::image, kImageLimits);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->size.width, 3);
    EXPECT_EQ(decoded->size.height, 5);
    EXPECT_FALSE(OpenCvImageClient::decode(jpeg, SourceKind::camera,
                                           {1'048'576, 1280 * 720, 5, 3}));
}

TEST(NativeRecognitionTest, RejectsInvalidAndOversizedImageBeforeDecode) {
    EXPECT_FALSE(OpenCvImageClient::decode("not an image", SourceKind::image, kImageLimits));
    auto png = encode(".png", {5, 3});
    png[16] = 0x7f;
    EXPECT_FALSE(OpenCvImageClient::decode(png, SourceKind::image, kImageLimits));
    EXPECT_FALSE(OpenCvImageClient::decode(encode(".png", {5, 3}), SourceKind::video,
                                           kImageLimits));
}

TEST(NativeRecognitionTest, OwnedMediaMustStayWithinSpoolAndMatchAcceptedDimensions) {
    const auto root = std::filesystem::temp_directory_path() /
                      ("road-sign-image-test-" + std::to_string(getpid()));
    std::filesystem::create_directories(root);
    const auto path = root / "accepted.png";
    const auto encoded = encode(".png", {5, 3});
    { std::ofstream output(path, std::ios::binary); output.write(encoded.data(), encoded.size()); }
    auto owner = std::make_shared<OwnedTemporaryFile>();
    owner->path = path;
    MediaSource source{SourceKind::image, 5, 3, std::nullopt, std::nullopt, owner};
    EXPECT_TRUE(OpenCvImageClient::decodeOwned(source, root, kImageLimits));
    source.width = 4;
    EXPECT_FALSE(OpenCvImageClient::decodeOwned(source, root, kImageLimits));
    source.width = 5;
    EXPECT_FALSE(OpenCvImageClient::decodeOwned(source, root / "other", kImageLimits));
    owner.reset();
    source.ownedTemporaryFile.reset();
    std::filesystem::remove_all(root);
}
} // namespace RoadSign::MediaProcessing
