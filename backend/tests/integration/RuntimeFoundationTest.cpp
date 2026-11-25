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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
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

TEST(RuntimeFoundationTest, RuntimeMetricsFlagAcceptsOnlyExplicitBooleans) {
    EXPECT_EQ(Infrastructure::RuntimeConfig::parseBoolean("true"), true);
    EXPECT_EQ(Infrastructure::RuntimeConfig::parseBoolean("false"), false);
    EXPECT_FALSE(Infrastructure::RuntimeConfig::parseBoolean("1"));
    EXPECT_FALSE(Infrastructure::RuntimeConfig::parseBoolean("TRUE"));
}

TEST(RuntimeFoundationTest, PolicyAcceptsConfiguredDefaultsAndRejectsEnlargedHardLimits) {
    constexpr std::string_view validPolicy = R"({
        "policyVersion":"1",
        "camera":{"maxWidth":1280,"maxHeight":720,"maxFramesPerSecond":5,
                   "maxFrameBytes":1048576,"jpegQuality":0.8},
        "image":{"maxBytes":10485760,"maxDecodedPixels":20000000},
        "video":{"maxBytes":104857600,"maxDurationSeconds":300,"maxWidth":1920,
                  "maxHeight":1080,"maxFramesPerSecond":60},
        "recognition":{"inputSize":640,"trackingScoreFloor":0.10,"nmsIou":0.45,
                       "acceptedScore":0.60,"temporalRequired":3,"temporalWindow":5,
                       "temporalWindowMs":2000,"stillScore":0.75},
        "tracking":{"highScore":0.50,"lowScore":0.10,"newTrackScore":0.60,
                    "maxAssociationCost":0.80,"expiryMs":2000,"maxTracks":100,"perClass":true},
        "ocr":{"maxReadsPerSecond":1,"maxCropsPerFrame":4,"minWordConfidence":70,
               "consensusSamples":3,"consensusMatches":2,"consensusWindowMs":2000,
               "stillMaxCrops":1,"minCropSize":16,"maxCropEdge":1600,
               "unicodeNfkc":true,"caseFoldConsensus":true},
        "speech":{"maxPending":3,"maxPendingAgeMs":5000,"maxTextCharacters":500,
                  "maxWavDurationMs":45000,"maxHistory":200,
                  "queueOverflow":"dropLowestPriorityNewestTie",
                  "categoryPriority":[0,0,0,0,2,1,2,0]},
        "lifecycle":{"heartbeatMs":5000,"leaseMs":15000,"eventAttachMs":10000,
                     "resourceFailure":"endSessionNoDiskSpill"},
        "upload":{"maxAttempts":3},
        "runtime":{"metricsEnabled":false,"maxRequestBodyBytes":105906176,
                    "spoolMemoryBytes":10485760,
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
    auto oversizedMemorySpool = std::string{validPolicy};
    const auto memorySpool = oversizedMemorySpool.find("\"spoolMemoryBytes\":10485760");
    ASSERT_NE(memorySpool, std::string::npos);
    oversizedMemorySpool.replace(memorySpool, std::string_view{"\"spoolMemoryBytes\":10485760"}.size(),
                                 "\"spoolMemoryBytes\":10485761");
    EXPECT_FALSE(Infrastructure::RuntimeConfig::fromPolicyJson(oversizedMemorySpool));
    auto invalidAttemptLimit = std::string{validPolicy};
    const auto attempts = invalidAttemptLimit.find("\"maxAttempts\":3");
    ASSERT_NE(attempts, std::string::npos);
    invalidAttemptLimit.replace(attempts, std::string_view{"\"maxAttempts\":3"}.size(),
                                "\"maxAttempts\":101");
    EXPECT_FALSE(Infrastructure::RuntimeConfig::fromPolicyJson(invalidAttemptLimit));
    auto alteredRecognition = std::string{validPolicy};
    const auto score = alteredRecognition.find("\"acceptedScore\":0.60");
    ASSERT_NE(score, std::string::npos);
    alteredRecognition.replace(score, std::string_view{"\"acceptedScore\":0.60"}.size(),
                               "\"acceptedScore\":0.10");
    EXPECT_FALSE(Infrastructure::RuntimeConfig::fromPolicyJson(alteredRecognition));
    auto alteredPriority = std::string{validPolicy};
    const auto priority = alteredPriority.find("[0,0,0,0,2,1,2,0]");
    ASSERT_NE(priority, std::string::npos);
    alteredPriority.replace(priority, std::string_view{"[0,0,0,0,2,1,2,0]"}.size(),
                            "[2,2,2,2,2,1,2,0]");
    EXPECT_FALSE(Infrastructure::RuntimeConfig::fromPolicyJson(alteredPriority));
}

TEST(RuntimeFoundationTest, LoggerOnlyExposesFixedEventNames) {
    EXPECT_EQ(Infrastructure::eventName(Infrastructure::Event::requestRejected), "request.rejected");
    EXPECT_EQ(Infrastructure::eventName(Infrastructure::Event::sessionCreated), "session.created");
    std::ostringstream output;
    auto* previous = std::clog.rdbuf(output.rdbuf());
    Infrastructure::logEvent(Infrastructure::Event::requestRejected);
    std::clog.rdbuf(previous);
    EXPECT_EQ(output.str(), "{\"event\":\"request.rejected\"}\n");
}

TEST(RuntimeFoundationTest, TemporaryMediaOwnerUnlinksItsSpool) {
    const auto path = std::filesystem::temp_directory_path() / "road-sign-runtime-spool-test";
    {
        std::ofstream file(path);
        ASSERT_TRUE(file);
        file << "test media";
    }
    {
        MediaProcessing::OwnedTemporaryFile owner{path};
        ASSERT_TRUE(std::filesystem::exists(path));
    }
    EXPECT_FALSE(std::filesystem::exists(path));
}
} // namespace RoadSign
