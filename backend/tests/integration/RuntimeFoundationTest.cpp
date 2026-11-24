#include "shared/Frame.hpp"
#include "shared/Identity.hpp"
#include "modules/sign-recognition/public/SignObservation.hpp"
#include "modules/media-processing/public/MediaSource.hpp"
#include "modules/text-extraction/public/ExtractTextRequest.hpp"
#include "modules/announcements/public/AcceptedInformation.hpp"
#include "modules/speech/public/AudioArtifact.hpp"
#include "infrastructure/RuntimeConfig.hpp"
#include "infrastructure/Logger.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace RoadSign {
TEST(RuntimeFoundationTest, RejectsInvalidAndOverflowingImageDimensions) {
    EXPECT_FALSE((ImageSize{0, 4}.pixelBytes(3)));
    EXPECT_FALSE((ImageSize{4, 4}.pixelBytes(0)));
    EXPECT_FALSE((ImageSize{std::numeric_limits<std::uint32_t>::max(),
                            std::numeric_limits<std::uint32_t>::max()}.pixelBytes(3)));
    EXPECT_EQ((ImageSize{2, 3}.pixelBytes(3)), 18);
}

TEST(RuntimeFoundationTest, ClipsFinitePositiveBoxesToOriginalImage) {
    const auto box = Box{-2.0, 1.0, 6.0, 4.0}.clipped({4, 4});
    ASSERT_TRUE(box);
    EXPECT_DOUBLE_EQ(box->x, 0.0);
    EXPECT_DOUBLE_EQ(box->y, 1.0);
    EXPECT_DOUBLE_EQ(box->width, 4.0);
    EXPECT_DOUBLE_EQ(box->height, 3.0);
    EXPECT_FALSE((Box{0.0, 0.0, std::numeric_limits<double>::infinity(), 1.0}.clipped({4, 4})));
}

TEST(RuntimeFoundationTest, RetainedFrameKeepsOwnedPixelsAlive) {
    std::vector<std::uint8_t> pixels(12, 7);
    auto frame = Frame::copyOf(1, 0.0, std::nullopt, {2, 2}, pixels);
    ASSERT_TRUE(frame);
    pixels.front() = 99;
    Frame copied = *frame;
    Frame retained = std::move(copied);
    EXPECT_EQ(retained.pixels()->front(), 7);
}

TEST(RuntimeFoundationTest, RejectsNegativeTimeAndWrongPixelLength) {
    std::vector<std::uint8_t> pixels(12, 0);
    EXPECT_FALSE(Frame::copyOf(1, -1.0, std::nullopt, {2, 2}, pixels));
    EXPECT_FALSE(Frame::copyOf(1, 0.0, std::numeric_limits<double>::infinity(), {2, 2}, pixels));
    EXPECT_FALSE(Frame::copyOf(1, 0.0, std::nullopt, {2, 2},
                               std::span<const std::uint8_t>(pixels.data(), 11)));
}

TEST(RuntimeFoundationTest, FrameIdsIncreaseWithinEpochAndResetAcrossEpochs) {
    FrameSequence sequence;
    EXPECT_TRUE(sequence.accept(1, 1));
    EXPECT_TRUE(sequence.accept(1, 2));
    EXPECT_FALSE(sequence.accept(1, 2));
    EXPECT_FALSE(sequence.accept(0, 3));
    EXPECT_TRUE(sequence.accept(2, 1));
    EXPECT_FALSE(sequence.accept(1, 3));
}

TEST(RuntimeFoundationTest, SignObservationRejectsInvalidConfidenceEpochAndGeometry) {
    using SignRecognition::SignObservation;
    const SignObservation valid{1, 2, 0.8, {1, 1, 2, 2}, std::nullopt, 0.7, 10.0};
    EXPECT_TRUE(valid.isValid(1, 1, {4, 4}));
    EXPECT_FALSE(valid.isValid(1, 2, {4, 4}));
    const SignObservation invalid{1, 2, 1.1, {1, 1, 2, 2}, std::nullopt, std::nullopt, 10.0};
    EXPECT_FALSE(invalid.isValid(1, 1, {4, 4}));
}

TEST(RuntimeFoundationTest, UploadAttemptPolicyAcceptsOnlyIntegerZeroThroughOneHundred) {
    EXPECT_EQ(Infrastructure::RuntimeConfig::parseUploadAttempts("0"), 0);
    EXPECT_EQ(Infrastructure::RuntimeConfig::parseUploadAttempts("100"), 100);
    EXPECT_FALSE(Infrastructure::RuntimeConfig::parseUploadAttempts("101"));
    EXPECT_FALSE(Infrastructure::RuntimeConfig::parseUploadAttempts("-1"));
    EXPECT_FALSE(Infrastructure::RuntimeConfig::parseUploadAttempts("3x"));
}

TEST(RuntimeFoundationTest, PolicyAcceptsConfiguredDefaultsAndRejectsEnlargedHardLimits) {
    constexpr std::string_view validPolicy = R"({
        "policyVersion":"1",
        "camera":{"maxWidth":1280,"maxHeight":720,"maxFramesPerSecond":5,
                   "maxFrameBytes":1048576},
        "image":{"maxBytes":10485760,"maxDecodedPixels":20000000},
        "video":{"maxBytes":104857600,"maxDurationSeconds":300,"maxWidth":1920,
                  "maxHeight":1080,"maxFramesPerSecond":60},
        "upload":{"maxAttempts":3},
        "runtime":{"metricsEnabled":false,"maxRequestBodyBytes":105906176,
                    "sessionTmpfsBytes":268435456}
    })";
    const auto config = Infrastructure::RuntimeConfig::fromPolicyJson(validPolicy);
    ASSERT_TRUE(config);
    EXPECT_EQ(config->maxUploadAttempts, 3);
    EXPECT_FALSE(config->metricsEnabled);
    EXPECT_EQ(config->maxDecodedPixels, 20'000'000);

    const std::vector<std::pair<std::string, std::string>> enlargedLimits{
        {"1280", "1281"}, {"720", "721"}, {"5", "6"},
        {"1048576", "1048577"},
        {"10485760", "10485761"}, {"20000000", "20000001"},
        {"104857600", "104857601"}, {"300", "301"}, {"1920", "1921"},
        {"1080", "1081"}, {"60", "61"}, {"105906176", "105906177"},
        {"268435456", "268435457"},
    };
    for (const auto& [limitValue, enlargedValue] : enlargedLimits) {
        auto oversized = std::string{validPolicy};
        const auto limit = oversized.find(limitValue);
        ASSERT_NE(limit, std::string::npos);
        oversized.replace(limit, limitValue.size(), enlargedValue);
        EXPECT_FALSE(Infrastructure::RuntimeConfig::fromPolicyJson(oversized))
            << "accepted an enlarged hard limit " << limitValue;
    }
    auto invalidAttemptLimit = std::string{validPolicy};
    const auto attempts = invalidAttemptLimit.find("\"maxAttempts\":3");
    ASSERT_NE(attempts, std::string::npos);
    invalidAttemptLimit.replace(attempts, std::string_view{"\"maxAttempts\":3"}.size(),
                                "\"maxAttempts\":101");
    EXPECT_FALSE(Infrastructure::RuntimeConfig::fromPolicyJson(invalidAttemptLimit));
}

TEST(RuntimeFoundationTest, LoggerOnlyExposesFixedEventNames) {
    EXPECT_EQ(Infrastructure::eventName(Infrastructure::Event::requestRejected), "request.rejected");
    EXPECT_EQ(Infrastructure::eventName(Infrastructure::Event::sessionCreated), "session.created");
}
} // namespace RoadSign
