#include "modules/sign-recognition/infrastructure/ByteTrackClient.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace RoadSign::SignRecognition {
namespace {
constexpr ImageSize FRAME_SIZE{1280, 720};
constexpr std::uint64_t TEN_MINUTE_FRAMES = 600;

SignObservation sign(std::uint64_t frameId, double sourceTimeMs) {
    return {frameId, 4, 0.85, Box{200, 150, 50, 100}, std::nullopt,
            std::nullopt, sourceTimeMs};
}
} // namespace

TEST(TrackingSoakTest, KeepsTenMinutesOfTrackingHistoryBounded) {
    ByteTrackClient tracker("soak-session", 1);
    auto history = TrackedSign::start("soak-session", 1, 1, sign(1, 0), FRAME_SIZE);
    ASSERT_TRUE(history);
    std::string firstId;
    for (std::uint64_t frameId = 1; frameId <= TEN_MINUTE_FRAMES; ++frameId) {
        const auto timeMs = static_cast<double>((frameId - 1) * 1000);
        const auto assignments = tracker.update({sign(frameId, timeMs)}, timeMs, FRAME_SIZE);
        ASSERT_TRUE(assignments);
        ASSERT_EQ(assignments->size(), std::size_t{1});
        if (firstId.empty()) firstId = assignments->front().trackId;
        EXPECT_EQ(assignments->front().trackId, firstId);
        ASSERT_LE(tracker.trackCount(), std::size_t{100});
        const auto* track = tracker.find(firstId);
        ASSERT_NE(track, nullptr);
        EXPECT_LE(track->detections().size(), std::size_t{5});
        EXPECT_LE(track->ocrSamples().size(), std::size_t{3});
        if (frameId > 1) {
            ASSERT_TRUE(history->observe(sign(frameId, timeMs), 1, FRAME_SIZE));
        }
        ASSERT_TRUE(history->recordOcr({"High Street", "high street", 0.9, timeMs}));
        EXPECT_LE(history->detections().size(), std::size_t{5});
        EXPECT_LE(history->ocrSamples().size(), std::size_t{3});
    }
    history->markLost(602000);
    EXPECT_EQ(history->state(), TrackedSign::State::removed);
    EXPECT_TRUE(history->detections().empty());
    EXPECT_TRUE(history->ocrSamples().empty());
    tracker.reset("soak-session", 2);
    EXPECT_EQ(tracker.find(firstId), nullptr);
    EXPECT_EQ(tracker.trackCount(), std::size_t{0});
}

} // namespace RoadSign::SignRecognition
