#include "modules/announcements/public/AcceptImageInformationUseCase.hpp"
#include "modules/announcements/public/ConsolidateInformationUseCase.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <tuple>

namespace RoadSign::Announcements {
namespace {
SignRecognition::SignObservation obstacle(Box box, double confidence = 0.75) {
    return {1, 7, confidence, box, std::nullopt, std::nullopt, 0.0};
}
} // namespace

TEST(ObstacleAnnouncementTest, DescribesClippedBoxCentreAtExactThirdBoundaries) {
    constexpr ImageSize size{300, 100};
    const auto left = AcceptImageInformationUseCase::describeObstacle(obstacle({-20, 0, 40, 20}), size);
    const auto beforeCentre = AcceptImageInformationUseCase::describeObstacle(obstacle({89.999, 0, 20, 20}), size);
    const auto centre = AcceptImageInformationUseCase::describeObstacle(obstacle({90, 0, 20, 20}), size);
    const auto beforeRight = AcceptImageInformationUseCase::describeObstacle(obstacle({189.999, 0, 20, 20}), size);
    const auto right = AcceptImageInformationUseCase::describeObstacle(obstacle({190, 0, 20, 20}), size);
    const auto farRight = AcceptImageInformationUseCase::describeObstacle(obstacle({290, 0, 30, 20}), size);
    ASSERT_TRUE(left);
    ASSERT_TRUE(beforeCentre);
    ASSERT_TRUE(centre);
    ASSERT_TRUE(beforeRight);
    ASSERT_TRUE(right);
    ASSERT_TRUE(farRight);
    EXPECT_EQ(left->text, "Possible obstacle on the left of the camera view");
    EXPECT_EQ(beforeCentre->text, left->text);
    EXPECT_EQ(centre->text, "Possible obstacle in the centre of the camera view");
    EXPECT_EQ(beforeRight->text, centre->text);
    EXPECT_EQ(right->text, "Possible obstacle on the right of the camera view");
    EXPECT_EQ(farRight->text, right->text);
    EXPECT_EQ(left->priority, 0);
    EXPECT_EQ(centre->priority, 0);
    EXPECT_EQ(right->priority, 0);
}

TEST(ObstacleAnnouncementTest, RejectsUncertainOrInvalidGeometry) {
    constexpr ImageSize size{300, 100};
    EXPECT_FALSE(AcceptImageInformationUseCase::describeObstacle(obstacle({0, 0, 20, 20}, 0.749), size));
    EXPECT_FALSE(AcceptImageInformationUseCase::describeObstacle(obstacle({0, 0, 0, 20}), size));
    EXPECT_FALSE(AcceptImageInformationUseCase::describeObstacle(obstacle({300, 0, 20, 20}), size));
    EXPECT_FALSE(AcceptImageInformationUseCase::describeObstacle(
        obstacle({std::numeric_limits<double>::quiet_NaN(), 0, 20, 20}), size));
    EXPECT_FALSE(AcceptImageInformationUseCase::describeObstacle(obstacle({0, 0, 20, 20}), {0, 100}));
    auto differentCategory = obstacle({0, 0, 20, 20});
    differentCategory.categoryId = 6;
    EXPECT_FALSE(AcceptImageInformationUseCase::describeObstacle(differentCategory, size));
}

TEST(ObstacleAnnouncementTest, AnnouncesAcceptedTrackOnceDespiteCameraMovement) {
    constexpr ImageSize size{300, 100};
    ConsolidateInformationUseCase policy;
    SignRecognition::TrackEvidenceDto track{"session/1/7/1", 7, 0, true, false, 1000,
        {obstacle({10, 0, 20, 20}), obstacle({11, 0, 20, 20}),
         obstacle({12, 0, 20, 20})}, {}};
    track.detections[0].sourceTimeMs = 0;
    track.detections[1].sourceTimeMs = 500;
    track.detections[2].sourceTimeMs = 1000;
    const auto first = policy.evaluate(track, size);
    EXPECT_TRUE(first.shouldConfirm);
    ASSERT_TRUE(first.acceptedObstacle);
    EXPECT_EQ(first.acceptedObstacle->text,
              "Possible obstacle on the left of the camera view");
    EXPECT_EQ(first.acceptedObstacle->priority, 0);

    track.isConfirmed = true;
    track.revision = 1;
    for (const auto& [frameId, time, x] :
         {std::tuple<std::uint64_t, double, double>{2, 1200, 130},
          std::tuple<std::uint64_t, double, double>{3, 1400, 250}}) {
        auto moved = obstacle({x, 0, 20, 20});
        moved.frameId = frameId;
        moved.sourceTimeMs = time;
        track.detections.push_back(moved);
        track.lastSourceTimeMs = time;
        const auto repeated = policy.evaluate(track, size);
        EXPECT_FALSE(repeated.shouldConfirm);
        EXPECT_FALSE(repeated.acceptedObstacle);
        EXPECT_EQ(track.revision, 1U);
    }

    track.trackId = "session/1/7/2";
    const auto newTrack = policy.evaluate(track, size);
    ASSERT_TRUE(newTrack.acceptedObstacle);
    EXPECT_EQ(newTrack.acceptedObstacle->text,
              "Possible obstacle on the right of the camera view");
}

TEST(ObstacleAnnouncementTest, InvalidFrameDoesNotConsumeFirstAcceptedAlert) {
    ConsolidateInformationUseCase policy;
    SignRecognition::TrackEvidenceDto track{"session/1/7/1", 7, 1, true, true, 0,
        {obstacle({90, 0, 20, 20})}, {}};
    EXPECT_FALSE(policy.evaluate(track, {0, 100}).acceptedObstacle);
    const auto accepted = policy.evaluate(track, {300, 100}).acceptedObstacle;
    ASSERT_TRUE(accepted);
    EXPECT_EQ(accepted->text, "Possible obstacle in the centre of the camera view");
}
} // namespace RoadSign::Announcements
