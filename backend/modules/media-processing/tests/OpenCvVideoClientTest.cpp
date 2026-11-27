#include "modules/media-processing/public/OpenCvVideoClient.hpp"

#include <gtest/gtest.h>

#include <opencv2/videoio.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include <unistd.h>

namespace RoadSign::MediaProcessing {
namespace {
constexpr VideoLimits LIMITS{100U * 1024U * 1024U, 1920, 1080, 300, 60};

struct VideoFixture {
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("road-sign-video-test-" + std::to_string(getpid()));
    std::shared_ptr<OwnedTemporaryFile> owner = std::make_shared<OwnedTemporaryFile>();

    VideoFixture() {
        std::filesystem::create_directories(root);
        owner->path = root / "clip.mp4";
        const auto fixture = std::filesystem::path{__FILE__}.parent_path() /
                             "fixtures/box-h264.mp4";
        std::filesystem::copy_file(fixture, owner->path,
                                   std::filesystem::copy_options::overwrite_existing);
    }

    ~VideoFixture() {
        owner.reset();
        std::filesystem::remove_all(root);
    }

    MediaSource source() const {
        cv::VideoCapture capture(owner->path.string(), cv::CAP_FFMPEG);
        const auto durationMs = static_cast<std::uint64_t>(
            capture.get(cv::CAP_PROP_FRAME_COUNT) / capture.get(cv::CAP_PROP_FPS) * 1000.0);
        return {SourceKind::video, 640, 480, durationMs, 0, owner};
    }
};
} // namespace

TEST(OpenCvVideoClientTest, SamplesAtMostFiveFramesPerSourceSecondAndSeeks) {
    VideoFixture fixture;
    ASSERT_TRUE(std::filesystem::exists(fixture.owner->path));
    auto decoder = OpenCvVideoClient::open(fixture.source(), fixture.root, LIMITS);
    ASSERT_TRUE(decoder);
    auto first = decoder->nextSample(0);
    ASSERT_TRUE(first);
    EXPECT_EQ(first->image.rgbPixels.size(), 640U * 480U * 3U);
    auto second = decoder->nextSample(0);
    ASSERT_TRUE(second);
    EXPECT_GE(second->sourceTimeMs - first->sourceTimeMs, 200U);
    auto later = decoder->nextSample(700);
    ASSERT_TRUE(later);
    EXPECT_GE(later->sourceTimeMs, 700U);
    EXPECT_FALSE(decoder->seek(*fixture.source().durationMs));
    ASSERT_TRUE(decoder->seek(0));
    auto replay = decoder->nextSample(0);
    ASSERT_TRUE(replay);
    EXPECT_LE(replay->sourceTimeMs, second->sourceTimeMs);
}

TEST(OpenCvVideoClientTest, RejectsUnsafeOrOutOfBoundsSources) {
    VideoFixture fixture;
    auto source = fixture.source();
    source.width = 641;
    EXPECT_FALSE(OpenCvVideoClient::open(source, fixture.root, LIMITS));
    source = fixture.source();
    EXPECT_FALSE(OpenCvVideoClient::open(source, fixture.root / "other", LIMITS));
    EXPECT_FALSE(OpenCvVideoClient::open(source, fixture.root,
        {LIMITS.maxBytes, 639, LIMITS.maxHeight, 300, 60}));
    EXPECT_FALSE(OpenCvVideoClient::open(source, fixture.root,
        {1, LIMITS.maxWidth, LIMITS.maxHeight, 300, 60}));
}
} // namespace RoadSign::MediaProcessing
