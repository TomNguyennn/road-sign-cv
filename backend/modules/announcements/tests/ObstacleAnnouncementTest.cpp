#include "modules/announcements/public/AcceptImageInformationUseCase.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>

namespace RoadSign::Announcements {
namespace {
SignRecognition::SignObservation obstacle(Box box, double confidence = 0.75) {
    return {1, 7, confidence, box, std::nullopt, std::nullopt, 0.0};
}
} // namespace

TEST(ObstacleAnnouncementTest, DescribesClippedBoxCentreAtExactThirdBoundaries) {
    constexpr ImageSize size{300, 100};
    const auto left = AcceptImageInformationUseCase::describeObstacle(obstacle({-20, 0, 40, 20}), size);
    const auto centre = AcceptImageInformationUseCase::describeObstacle(obstacle({90, 0, 20, 20}), size);
    const auto right = AcceptImageInformationUseCase::describeObstacle(obstacle({190, 0, 20, 20}), size);
    const auto farRight = AcceptImageInformationUseCase::describeObstacle(obstacle({290, 0, 30, 20}), size);
    ASSERT_TRUE(left);
    ASSERT_TRUE(centre);
    ASSERT_TRUE(right);
    ASSERT_TRUE(farRight);
    EXPECT_EQ(left->text, "Possible obstacle on the left of the camera view");
    EXPECT_EQ(centre->text, "Possible obstacle on the centre of the camera view");
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
} // namespace RoadSign::Announcements
