#include "modules/sign-recognition/domain/TrackedSign.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace RoadSign::SignRecognition {
namespace {
constexpr ImageSize FRAME_SIZE{300, 100};

SignObservation observation(std::uint64_t frameId, std::uint32_t categoryId,
                            double sourceTimeMs, double x, double width = 20) {
    return {frameId, categoryId, 0.8, Box{x, 10, width, 20}, std::nullopt,
            std::nullopt, sourceTimeMs};
}
} // namespace

TEST(TrackingTest, SeparatesCategoriesAndPhysicalSignsWithTheSameText) {
    auto firstObservation = observation(1, 4, 0, 0);
    auto secondObservation = observation(1, 4, 0, 100);
    firstObservation.textCandidate = "High Street";
    secondObservation.textCandidate = "High Street";
    auto first = TrackedSign::start("session", 1, 1, firstObservation, FRAME_SIZE);
    auto second = TrackedSign::start("session", 1, 2, secondObservation, FRAME_SIZE);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_NE(first->trackId(), second->trackId());
    EXPECT_EQ(first->trackId(), "session/1/4/1");
    EXPECT_EQ(second->trackId(), "session/1/4/2");
    EXPECT_FALSE(first->canAssociate(observation(2, 5, 100, 0), 1, FRAME_SIZE));
    EXPECT_FALSE(first->canAssociate(observation(2, 4, 100, 100), 1, FRAME_SIZE));
    EXPECT_FALSE(first->canAssociate(observation(2, 4, 100, 1), 2, FRAME_SIZE));
    EXPECT_TRUE(first->canAssociate(observation(2, 4, 100, 1), 1, FRAME_SIZE));
}

TEST(TrackingTest, StillImageStartsConfirmedAndTextRevisionNeedsSubstantiveChange) {
    auto track = TrackedSign::start("session", 1, 1, observation(1, 4, 0, 0), FRAME_SIZE, true);
    ASSERT_TRUE(track);
    EXPECT_EQ(track->state(), TrackedSign::State::confirmed);
    EXPECT_EQ(track->revision(), 1U);
    ASSERT_TRUE(track->acceptedCategoryId());
    EXPECT_EQ(*track->acceptedCategoryId(), 4U);
    EXPECT_TRUE(track->acceptText("High Street", "high street"));
    EXPECT_EQ(track->revision(), 2U);
    EXPECT_FALSE(track->acceptText("HIGH STREET", "high street"));
    EXPECT_EQ(track->revision(), 2U);
    ASSERT_TRUE(track->acceptedText());
    EXPECT_EQ(*track->acceptedText(), "High Street");
    EXPECT_TRUE(track->acceptText("Broad Street", "broad street"));
    EXPECT_EQ(track->revision(), 3U);
    auto uncertain = observation(1, 4, 0, 0);
    uncertain.confidence = 0.74;
    EXPECT_FALSE(TrackedSign::start("session", 1, 2, uncertain, FRAME_SIZE, true));
}

TEST(TrackingTest, PredictsMotionUsingElapsedSourceTime) {
    auto track = TrackedSign::start("session", 1, 1, observation(1, 7, 0, 0), FRAME_SIZE);
    ASSERT_TRUE(track);
    ASSERT_TRUE(track->observe(observation(2, 7, 1000, 10), 1, FRAME_SIZE));
    EXPECT_TRUE(track->canAssociate(observation(3, 7, 1500, 15), 1, FRAME_SIZE));
    EXPECT_TRUE(track->canAssociate(observation(3, 7, 2000, 20), 1, FRAME_SIZE));
    EXPECT_FALSE(track->canAssociate(observation(3, 7, 3001, 30), 1, FRAME_SIZE));
}

TEST(TrackingTest, ReacquiresWithinTwoSecondsAndRemovesAfterExpiry) {
    auto track = TrackedSign::start("session", 2, 4, observation(1, 7, 1000, 0), FRAME_SIZE);
    ASSERT_TRUE(track);
    track->confirm();
    EXPECT_EQ(track->state(), TrackedSign::State::confirmed);
    track->markLost(1500);
    EXPECT_EQ(track->state(), TrackedSign::State::lost);
    ASSERT_TRUE(track->observe(observation(2, 7, 3000, 0), 2, FRAME_SIZE));
    EXPECT_EQ(track->state(), TrackedSign::State::confirmed);
    EXPECT_EQ(track->revision(), 1U);
    track->markLost(5001);
    EXPECT_EQ(track->state(), TrackedSign::State::removed);
    EXPECT_FALSE(track->canAssociate(observation(3, 7, 5002, 0), 2, FRAME_SIZE));
    EXPECT_TRUE(track->detections().empty());
    EXPECT_TRUE(track->ocrSamples().empty());
}

TEST(TrackingTest, DetectionAndOcrHistoriesAreIndependentAndBounded) {
    auto track = TrackedSign::start("session", 1, 1, observation(1, 4, 0, 0), FRAME_SIZE);
    ASSERT_TRUE(track);
    for (int index = 1; index <= 6; ++index)
        ASSERT_TRUE(track->observe(observation(index + 1, 4, index * 500, 0), 1, FRAME_SIZE));
    EXPECT_EQ(track->detections().size(), std::size_t{5});
    EXPECT_EQ(track->detections().front().frameId, std::uint64_t{3});
    EXPECT_TRUE(track->recordOcr({"High Street", "high street", 0.9, 1000}));
    EXPECT_TRUE(track->recordOcr({"High Street", "high street", 0.9, 2000}));
    EXPECT_TRUE(track->recordOcr({"High Street", "high street", 0.9, 2500}));
    EXPECT_TRUE(track->recordOcr({"High Street", "high street", 0.9, 3000}));
    EXPECT_EQ(track->ocrSamples().size(), std::size_t{3});
    EXPECT_EQ(track->ocrSamples().front().sourceTimeMs, 2000);
    EXPECT_EQ(track->detections().size(), std::size_t{5});
}

TEST(TrackingTest, NewDetectionsExpireOldOcrWithoutChangingDetectionHistory) {
    auto track = TrackedSign::start("session", 1, 1, observation(1, 4, 0, 0), FRAME_SIZE);
    ASSERT_TRUE(track);
    ASSERT_TRUE(track->recordOcr({"High Street", "high street", 0.9, 0}));
    ASSERT_TRUE(track->observe(observation(2, 4, 2000, 0), 1, FRAME_SIZE));
    EXPECT_EQ(track->ocrSamples().size(), std::size_t{1});
    ASSERT_TRUE(track->observe(observation(3, 4, 2001, 0), 1, FRAME_SIZE));
    EXPECT_TRUE(track->ocrSamples().empty());
    EXPECT_EQ(track->detections().size(), std::size_t{3});
}

TEST(TrackingTest, EvidenceCopiesBoundedHistoriesAcrossModuleBoundary) {
    auto track = TrackedSign::start("session", 1, 1, observation(1, 4, 0, 0), FRAME_SIZE);
    ASSERT_TRUE(track);
    ASSERT_TRUE(track->observe(observation(2, 4, 100, 1), 1, FRAME_SIZE));
    ASSERT_TRUE(track->recordOcr({"High Street", "high street", 0.9, 100}));
    auto evidence = track->evidence();
    ASSERT_EQ(evidence.detections.size(), std::size_t{2});
    ASSERT_EQ(evidence.ocrSamples.size(), std::size_t{1});
    evidence.detections.clear();
    evidence.ocrSamples.front().displayText = "Changed";
    EXPECT_EQ(track->detections().size(), std::size_t{2});
    EXPECT_EQ(track->ocrSamples().front().displayText, "High Street");
}

TEST(TrackingTest, RejectsInvalidTrackCreationAndLateOcr) {
    EXPECT_FALSE(TrackedSign::start("", 1, 1, observation(1, 4, 0, 0), FRAME_SIZE));
    EXPECT_FALSE(TrackedSign::start("session", 0, 1, observation(1, 4, 0, 0), FRAME_SIZE));
    EXPECT_FALSE(TrackedSign::start("session", 1, 0, observation(1, 4, 0, 0), FRAME_SIZE));
    auto track = TrackedSign::start("session", 1, 1, observation(1, 4, 3000, 0), FRAME_SIZE);
    ASSERT_TRUE(track);
    EXPECT_FALSE(track->recordOcr({"High Street", "high street", 0.9, 999}));
    EXPECT_FALSE(track->recordOcr({"", "", 0.9, 3000}));
    EXPECT_FALSE(track->observe(observation(2, 5, 3100, 0), 1, FRAME_SIZE));
    EXPECT_FALSE(track->observe(observation(2, 4, 3100, 0), 2, FRAME_SIZE));
    EXPECT_FALSE(track->observe(observation(2, 4, 2999, 0), 1, FRAME_SIZE));
    auto obstacle = TrackedSign::start("session", 1, 2, observation(1, 7, 3000, 0),
                                       FRAME_SIZE, true);
    ASSERT_TRUE(obstacle);
    EXPECT_FALSE(obstacle->recordOcr({"Unrelated text", "unrelated text", 0.9, 3000}));
    EXPECT_FALSE(obstacle->acceptText("Unrelated text", "unrelated text"));
}
} // namespace RoadSign::SignRecognition
