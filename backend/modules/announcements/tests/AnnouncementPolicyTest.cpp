#include "modules/announcements/public/AnnouncementService.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace RoadSign::Announcements {
namespace {
const AnnouncementTime START{};

AnnouncementTime at(std::int64_t milliseconds) {
    return START + std::chrono::milliseconds(milliseconds);
}

AcceptedInformation information(std::string trackId, std::uint32_t revision,
                                std::uint32_t category = 4, std::string text = "High Street") {
    const std::uint8_t priority = category == 5 ? 1 :
        (category == 4 || category == 6 ? 2 : 0);
    return {std::move(trackId), revision, category, std::move(text), "high street",
            priority, 1, std::nullopt};
}
} // namespace

TEST(AnnouncementPolicyTest, SuppressesPendingSynthesisOfferedAndStartedSignatures) {
    AnnouncementService queue;
    auto sign = information("track", 1);
    EXPECT_TRUE(queue.enqueue(sign, "a", at(0)).isQueued);
    sign.displayText = " HIGH   STREET ";
    EXPECT_FALSE(queue.enqueue(sign, "duplicate", at(1)).isQueued);
    const auto synthesis = queue.beginSynthesis(at(2));
    ASSERT_TRUE(synthesis);
    EXPECT_EQ(synthesis->announcementId, "a");
    EXPECT_FALSE(queue.enqueue(sign, "duplicate-2", at(3)).isQueued);
    EXPECT_TRUE(queue.offer("a", at(4)));
    EXPECT_FALSE(queue.enqueue(sign, "duplicate-3", at(5)).isQueued);
    EXPECT_TRUE(queue.started("a", at(6)));
    EXPECT_TRUE(queue.completed("a"));
    EXPECT_FALSE(queue.enqueue(sign, "duplicate-4", at(7)).isQueued);
    sign.revision = 2;
    sign.displayText = "Broad Street";
    EXPECT_TRUE(queue.enqueue(sign, "changed", at(8)).isQueued);
}

TEST(AnnouncementPolicyTest, KeepsThreePendingByPriorityAndOldestTie) {
    AnnouncementService queue;
    EXPECT_TRUE(queue.enqueue(information("a", 1), "a", at(0)).isQueued);
    EXPECT_TRUE(queue.enqueue(information("b", 1), "b", at(1)).isQueued);
    EXPECT_TRUE(queue.enqueue(information("c", 1), "c", at(2)).isQueued);
    const auto rejected = queue.enqueue(information("d", 1), "d", at(3));
    EXPECT_FALSE(rejected.isQueued);
    EXPECT_EQ(rejected.droppedAnnouncementId, "d");
    auto high = information("warning", 1, 0, "Warning");
    const auto inserted = queue.enqueue(high, "warning", at(4));
    EXPECT_TRUE(inserted.isQueued);
    EXPECT_EQ(inserted.droppedAnnouncementId, "c");
    EXPECT_EQ(queue.pendingCount(), std::size_t{3});
    const auto first = queue.beginSynthesis(at(5));
    ASSERT_TRUE(first);
    EXPECT_EQ(first->announcementId, "warning");
    EXPECT_TRUE(queue.failedSynthesis("warning"));
    const auto second = queue.beginSynthesis(at(6));
    ASSERT_TRUE(second);
    EXPECT_EQ(second->announcementId, "a");
}

TEST(AnnouncementPolicyTest, NewPriorityDoesNotPreemptPlayingSpeech) {
    AnnouncementService queue;
    EXPECT_TRUE(queue.enqueue(information("street", 1), "street", at(0)).isQueued);
    ASSERT_TRUE(queue.beginSynthesis(at(0)));
    EXPECT_TRUE(queue.offer("street", at(1)));
    EXPECT_TRUE(queue.started("street", at(2)));
    EXPECT_TRUE(queue.enqueue(information("warning", 1, 0, "Warning"),
                              "warning", at(3)).isQueued);
    ASSERT_TRUE(queue.active());
    EXPECT_EQ(queue.active()->announcementId, "street");
    EXPECT_EQ(queue.active()->state, AnnouncementState::playing);
    EXPECT_FALSE(queue.beginSynthesis(at(4)));
    EXPECT_TRUE(queue.completed("street"));
    const auto next = queue.beginSynthesis(at(5));
    ASSERT_TRUE(next);
    EXPECT_EQ(next->announcementId, "warning");
}

TEST(AnnouncementPolicyTest, ExpiresPendingAndRejectsLateSynthesisResult) {
    AnnouncementService queue;
    EXPECT_TRUE(queue.enqueue(information("old", 1), "old", at(0)).isQueued);
    EXPECT_FALSE(queue.beginSynthesis(at(5000)));
    EXPECT_TRUE(queue.enqueue(information("new", 1), "new", at(6000)).isQueued);
    ASSERT_TRUE(queue.beginSynthesis(at(6000)));
    EXPECT_FALSE(queue.offer("new", at(11000)));
    EXPECT_FALSE(queue.active());
    EXPECT_TRUE(queue.enqueue(information("new", 1), "fresh-observation", at(11001)).isQueued);
}

TEST(AnnouncementPolicyTest, InvalidatesObsoletePendingAndOfferedResults) {
    AnnouncementService queue;
    EXPECT_TRUE(queue.enqueue(information("track", 1), "old", at(0)).isQueued);
    EXPECT_FALSE(queue.invalidateTrack("track", 2, "unused", 1, at(1)));
    EXPECT_EQ(queue.pendingCount(), std::size_t{0});
    EXPECT_TRUE(queue.enqueue(information("track", 2), "new", at(2)).isQueued);
    ASSERT_TRUE(queue.beginSynthesis(at(2)));
    EXPECT_TRUE(queue.offer("new", at(3)));
    EXPECT_TRUE(queue.invalidateTrack("track", std::nullopt, "cancel-new", 1, at(4)));
    EXPECT_FALSE(queue.active());
    EXPECT_TRUE(queue.hasCancellationBarrier());
    EXPECT_FALSE(queue.offer("new", at(5)));
    EXPECT_TRUE(queue.enqueue(information("other", 1), "other", at(6)).isQueued);
    EXPECT_FALSE(queue.beginSynthesis(at(7)));
    EXPECT_FALSE(queue.acknowledgeCancellation("wrong", 1, at(8)));
    EXPECT_FALSE(queue.acknowledgeCancellation("cancel-new", 2, at(8)));
    EXPECT_TRUE(queue.acknowledgeCancellation("cancel-new", 1, at(8)));
    EXPECT_TRUE(queue.beginSynthesis(at(9)));
}

TEST(AnnouncementPolicyTest, InvalidatesSynthesisBeforeAResultCanBeOffered) {
    AnnouncementService queue;
    EXPECT_TRUE(queue.enqueue(information("track", 1), "old", at(0)).isQueued);
    ASSERT_TRUE(queue.beginSynthesis(at(1)));
    EXPECT_FALSE(queue.invalidateTrack("track", std::nullopt, "unused", 1, at(2)));
    EXPECT_FALSE(queue.active());
    EXPECT_FALSE(queue.offer("old", at(3)));
    EXPECT_FALSE(queue.hasCancellationBarrier());
    EXPECT_TRUE(queue.enqueue(information("track", 1), "reacquired", at(4)).isQueued);
}

TEST(AnnouncementPolicyTest, CopiesScalarSourceMetadataAndRejectsInvalidCaptureTime) {
    AnnouncementService queue;
    auto sign = information("track", 1);
    sign.frameId = 42;
    sign.clientCaptureTimeMs = 123.5;
    ASSERT_TRUE(queue.enqueue(sign, "a", at(0)).isQueued);
    const auto active = queue.beginSynthesis(at(1));
    ASSERT_TRUE(active);
    EXPECT_EQ(active->frameId, 42U);
    EXPECT_EQ(active->clientCaptureTimeMs, 123.5);
    sign.trackId = "invalid";
    sign.clientCaptureTimeMs = -1;
    EXPECT_FALSE(queue.enqueue(sign, "invalid", at(2)).isQueued);
}

TEST(AnnouncementPolicyTest, CancellationAckTimeoutEndsScheduling) {
    AnnouncementService queue;
    EXPECT_TRUE(queue.enqueue(information("track", 1), "a", at(0)).isQueued);
    ASSERT_TRUE(queue.beginSynthesis(at(0)));
    EXPECT_TRUE(queue.offer("a", at(1)));
    EXPECT_TRUE(queue.expire(at(5000), "cancel-expired", 1));
    EXPECT_TRUE(queue.hasCancellationBarrier());
    EXPECT_TRUE(queue.enqueue(information("other", 1), "b", at(5001)).isQueued);
    EXPECT_FALSE(queue.beginSynthesis(at(10000)));
    EXPECT_TRUE(queue.isEnded());
    EXPECT_FALSE(queue.acknowledgeCancellation("cancel-expired", 1, at(10001)));
}
} // namespace RoadSign::Announcements
