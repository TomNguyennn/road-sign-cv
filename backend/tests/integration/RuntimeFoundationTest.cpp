#include "shared/Frame.hpp"
#include "shared/Identity.hpp"
#include "modules/sign-recognition/public/SignObservation.hpp"
#include "modules/media-processing/public/MediaSource.hpp"
#include "modules/text-extraction/public/ExtractTextRequest.hpp"
#include "modules/announcements/public/AcceptedInformation.hpp"
#include "modules/speech/public/AudioArtifact.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <memory>
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
} // namespace RoadSign
