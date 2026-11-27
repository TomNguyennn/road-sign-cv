#include "modules/sign-recognition/infrastructure/ByteTrackClient.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace RoadSign::SignRecognition {
namespace {
constexpr ImageSize FRAME_SIZE{3000, 100};

SignObservation sign(std::uint64_t frame, std::uint32_t category, double time,
                     double x, double confidence = 0.8) {
    return {frame, category, confidence, Box{x, 10, 20, 20}, std::nullopt,
            std::nullopt, time};
}
} // namespace

TEST(ByteTrackClientTest, SeparatesOverlappingCategoriesAndDistinctSameCategorySigns) {
    ByteTrackClient client("session", 1);
    const auto first = client.update({sign(1, 0, 0, 10), sign(1, 1, 0, 10),
                                      sign(1, 0, 0, 100)}, 0, FRAME_SIZE);
    ASSERT_TRUE(first);
    ASSERT_EQ(first->size(), std::size_t{3});
    EXPECT_NE((*first)[0].trackId, (*first)[1].trackId);
    EXPECT_NE((*first)[0].trackId, (*first)[2].trackId);
    const auto second = client.update({sign(2, 0, 200, 12), sign(2, 1, 200, 12),
                                       sign(2, 0, 200, 102)}, 200, FRAME_SIZE);
    ASSERT_TRUE(second);
    ASSERT_EQ(second->size(), std::size_t{3});
    for (std::size_t index = 0; index < first->size(); ++index)
        EXPECT_EQ((*first)[index].trackId, (*second)[index].trackId);
    EXPECT_EQ(client.trackCount(), std::size_t{3});
}

TEST(ByteTrackClientTest, UsesSourceTimeForLossReacquisitionAndNewEncounter) {
    ByteTrackClient client("session", 1);
    const auto first = client.update({sign(1, 4, 0, 10)}, 0, FRAME_SIZE);
    ASSERT_TRUE(first);
    ASSERT_EQ(first->size(), std::size_t{1});
    const auto second = client.update({sign(2, 4, 500, 12)}, 500, FRAME_SIZE);
    ASSERT_TRUE(second);
    ASSERT_EQ(second->size(), std::size_t{1});
    EXPECT_EQ((*first)[0].trackId, (*second)[0].trackId);
    ASSERT_TRUE(client.update({}, 1500, FRAME_SIZE));
    ASSERT_NE(client.find((*first)[0].trackId), nullptr);
    EXPECT_EQ(client.find((*first)[0].trackId)->state(), TrackedSign::State::lost);
    const auto reacquired = client.update({sign(3, 4, 2000, 14)}, 2000, FRAME_SIZE);
    ASSERT_TRUE(reacquired);
    ASSERT_EQ(reacquired->size(), std::size_t{1});
    EXPECT_EQ((*first)[0].trackId, (*reacquired)[0].trackId);
    ASSERT_TRUE(client.update({}, 4001, FRAME_SIZE));
    EXPECT_EQ(client.find((*first)[0].trackId), nullptr);
    const auto newEncounter = client.update({sign(4, 4, 4100, 14)}, 4100, FRAME_SIZE);
    ASSERT_TRUE(newEncounter);
    ASSERT_EQ(newEncounter->size(), std::size_t{1});
    EXPECT_NE((*first)[0].trackId, (*newEncounter)[0].trackId);
}

TEST(ByteTrackClientTest, DoesNotExpireFromHighFrameCountWithinTwoSeconds) {
    ByteTrackClient client("session", 1);
    const auto first = client.update({sign(1, 1, 0, 10)}, 0, FRAME_SIZE);
    ASSERT_TRUE(first);
    ASSERT_EQ(first->size(), std::size_t{1});
    for (std::uint64_t frame = 1; frame <= 100; ++frame)
        ASSERT_TRUE(client.update({}, static_cast<double>(frame * 10), FRAME_SIZE));
    const auto reacquired = client.update({sign(102, 1, 1001, 10)}, 1001, FRAME_SIZE);
    ASSERT_TRUE(reacquired);
    ASSERT_EQ(reacquired->size(), std::size_t{1});
    EXPECT_EQ((*first)[0].trackId, (*reacquired)[0].trackId);
}

TEST(ByteTrackClientTest, RequiresNewTrackScoreButRetainsLowScoreMatches) {
    ByteTrackClient client("session", 1);
    const auto uncertain = client.update({sign(1, 2, 0, 10, 0.59)}, 0, FRAME_SIZE);
    ASSERT_TRUE(uncertain);
    EXPECT_TRUE(uncertain->empty());
    const auto accepted = client.update({sign(2, 2, 200, 10, 0.60)}, 200, FRAME_SIZE);
    ASSERT_TRUE(accepted);
    ASSERT_EQ(accepted->size(), std::size_t{1});
    const auto low = client.update({sign(3, 2, 400, 11, 0.10)}, 400, FRAME_SIZE);
    ASSERT_TRUE(low);
    ASSERT_EQ(low->size(), std::size_t{1});
    EXPECT_EQ((*accepted)[0].trackId, (*low)[0].trackId);
    const auto belowFloor = client.update({sign(4, 2, 600, 12, 0.09)}, 600, FRAME_SIZE);
    ASSERT_TRUE(belowFloor);
    EXPECT_TRUE(belowFloor->empty());
}

TEST(ByteTrackClientTest, UsesOneObservationPerExistingTrack) {
    ByteTrackClient client("session", 1);
    const auto first = client.update({sign(1, 2, 0, 10)}, 0, FRAME_SIZE);
    ASSERT_TRUE(first);
    ASSERT_EQ(first->size(), std::size_t{1});
    const auto duplicate = client.update({sign(2, 2, 200, 11, 0.70),
                                          sign(2, 2, 200, 12, 0.90)}, 200, FRAME_SIZE);
    ASSERT_TRUE(duplicate);
    ASSERT_EQ(duplicate->size(), std::size_t{1});
    EXPECT_EQ((*duplicate)[0].observationIndex, std::size_t{1});
    EXPECT_EQ((*duplicate)[0].trackId, (*first)[0].trackId);
    EXPECT_EQ(client.trackCount(), std::size_t{1});
}

TEST(ByteTrackClientTest, BoundsActiveAndLostTracksAndRejectsExcessNewCandidates) {
    ByteTrackClient client("session", 1);
    std::vector<SignObservation> observations;
    for (std::size_t index = 0; index < 101; ++index)
        observations.push_back(sign(1, 3, 0, static_cast<double>(index * 25),
                                    index == 100 ? 0.99 : 0.61));
    const auto assigned = client.update(observations, 0, FRAME_SIZE);
    ASSERT_TRUE(assigned);
    EXPECT_EQ(assigned->size(), std::size_t{100});
    EXPECT_EQ(client.trackCount(), std::size_t{100});
    EXPECT_EQ(assigned->back().observationIndex, std::size_t{100});
    ASSERT_TRUE(client.update({}, 1000, FRAME_SIZE));
    EXPECT_EQ(client.trackCount(), std::size_t{100});
    ASSERT_TRUE(client.update({}, 2001, FRAME_SIZE));
    EXPECT_EQ(client.trackCount(), std::size_t{0});
}

TEST(ByteTrackClientTest, RejectsInvalidTimeAndFencesEpochReset) {
    ByteTrackClient client("session", 1);
    EXPECT_FALSE(client.update({}, -1, FRAME_SIZE));
    EXPECT_FALSE(client.update({}, 0, {0, 100}));
    const auto first = client.update({sign(1, 0, 0, 10)}, 0, FRAME_SIZE);
    ASSERT_TRUE(first);
    ASSERT_EQ(first->size(), std::size_t{1});
    EXPECT_FALSE(client.update({}, 0, FRAME_SIZE));
    EXPECT_THROW(client.reset("session", 1), std::invalid_argument);
    client.reset("session", 2);
    EXPECT_EQ(client.trackCount(), std::size_t{0});
    const auto next = client.update({sign(2, 0, 0, 10)}, 0, FRAME_SIZE);
    ASSERT_TRUE(next);
    ASSERT_EQ(next->size(), std::size_t{1});
    EXPECT_NE((*first)[0].trackId, (*next)[0].trackId);
    EXPECT_EQ(client.find((*first)[0].trackId), nullptr);
}
} // namespace RoadSign::SignRecognition
