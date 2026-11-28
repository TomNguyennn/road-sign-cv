#include "../application/ControlAudioUseCase.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace RoadSign::RecognitionSession {
namespace {
constexpr auto now = std::chrono::steady_clock::time_point{};

Entity imageSession() {
    Entity session{};
    session.sessionId = "session-1";
    session.sourceKind = SourceKind::image;
    session.state = State::reviewing;
    return session;
}
} // namespace

TEST(AudioControlsTest, MuteFencesCurrentAudioAndUnmuteDoesNotDrainBacklog) {
    auto session = imageSession();
    ControlAudioUseCase controls{session};
    controls.acceptDescription("description-1", "Crossing ahead", now);
    ASSERT_TRUE(controls.offerAutomatic("description-1", "track-1", 1, 1, now));

    const auto muted = controls.mute(now);
    EXPECT_TRUE(session.isMuted);
    EXPECT_EQ(session.audioGeneration, 2);
    EXPECT_GT(session.stateRevision, 1);
    ASSERT_TRUE(muted.cancellation);
    EXPECT_EQ(muted.cancellation->cancelThroughAudioGeneration, 1);
    EXPECT_FALSE(controls.offerAutomatic("description-1", "track-1", 1, 1, now));

    EXPECT_TRUE(controls.acknowledgeCancellation(muted.cancellation->cancellationId, 1));
    EXPECT_EQ(controls.unmute(now).status, AudioControlStatus::accepted);
    EXPECT_FALSE(session.isMuted);
    EXPECT_FALSE(controls.offerAutomatic("description-1", "track-1", 1, 1, now));
}

TEST(AudioControlsTest, RepeatRequiresStartedSpeechAndNeverBypassesMute) {
    auto session = imageSession();
    ControlAudioUseCase controls{session};
    EXPECT_EQ(controls.repeat(now).status, AudioControlStatus::empty);
    controls.acceptDescription("description-1", "Crossing ahead", now);
    EXPECT_EQ(controls.repeat(now).status, AudioControlStatus::empty);
    const auto offer = controls.offerAutomatic("description-1", "track-1", 1, 1, now);
    ASSERT_TRUE(offer);
    ASSERT_TRUE(controls.recordPlaybackStarted(offer->announcementId, "description-1", 1));
    EXPECT_EQ(controls.mute(now).status, AudioControlStatus::accepted);
    EXPECT_EQ(controls.repeat(now).status, AudioControlStatus::muted);
}

TEST(AudioControlsTest, FirstImageCanRecoverAfterBlockedPlaybackWithoutStartedHistory) {
    auto session = imageSession();
    ControlAudioUseCase controls{session};
    controls.acceptDescription("description-1", "Crossing ahead", now);
    const auto offer = controls.offerAutomatic("description-1", "track-1", 1, 1, now);
    ASSERT_TRUE(offer);
    controls.recordPlaybackBlocked(offer->announcementId, 1);
    EXPECT_EQ(controls.repeat(now).status, AudioControlStatus::empty);
    EXPECT_EQ(controls.recover("description-1", false, now).status,
              AudioControlStatus::activationRequired);
    const auto recovered = controls.recover("description-1", true, now);
    ASSERT_EQ(recovered.status, AudioControlStatus::accepted);
    ASSERT_TRUE(recovered.offer);
    EXPECT_TRUE(recovered.offer->isReplay);
    EXPECT_EQ(recovered.offer->origin, "recovery");
    EXPECT_FALSE(recovered.offer->trackId);
    EXPECT_FALSE(recovered.offer->revision);
    EXPECT_FALSE(recovered.offer->frameId);
    EXPECT_FALSE(recovered.offer->clientCaptureTimeMs);
    EXPECT_EQ(controls.recover("old-description", true, now).status,
              AudioControlStatus::staleDescription);
}

TEST(AudioControlsTest, CancellationBarrierRejectsLateStartAndReleasesSlotOnlyAfterAck) {
    auto session = imageSession();
    ControlAudioUseCase controls{session};
    controls.acceptDescription("description-1", "Crossing ahead", now);
    const auto offer = controls.offerAutomatic("description-1", "track-1", 1, 1, now);
    ASSERT_TRUE(offer);
    const auto muted = controls.mute(now);
    ASSERT_TRUE(muted.cancellation);
    EXPECT_EQ(controls.unmute(now).status, AudioControlStatus::accepted);
    EXPECT_FALSE(controls.offerAutomatic("description-1", "track-1", 1, 1, now));
    EXPECT_FALSE(controls.recordPlaybackStarted(offer->announcementId, "description-1", 1));
    EXPECT_FALSE(controls.acknowledgeCancellation("unknown", 1));
    EXPECT_TRUE(controls.acknowledgeCancellation(muted.cancellation->cancellationId, 1));
    EXPECT_TRUE(controls.acknowledgeCancellation(muted.cancellation->cancellationId, 1));
    EXPECT_FALSE(controls.acknowledgeCancellation(muted.cancellation->cancellationId, 2));
}

TEST(AudioControlsTest, SupersededAcknowledgmentCanArriveAfterNextGeneration) {
    auto session = imageSession();
    ControlAudioUseCase controls{session};
    const auto first = controls.mute(now);
    ASSERT_TRUE(first.cancellation);
    EXPECT_EQ(controls.unmute(now).status, AudioControlStatus::accepted);
    const auto second = controls.mute(now);
    ASSERT_TRUE(second.cancellation);
    EXPECT_GT(session.audioGeneration, first.cancellation->cancelThroughAudioGeneration);
    EXPECT_FALSE(controls.acknowledgeCancellation(first.cancellation->cancellationId, 1));
    EXPECT_FALSE(controls.offerAutomatic("description-1", "track-1", 1, 1, now));
    EXPECT_TRUE(controls.acknowledgeCancellation(second.cancellation->cancellationId, 2));
    EXPECT_TRUE(controls.acknowledgeCancellation(second.cancellation->cancellationId, 2));
}

TEST(AudioControlsTest, CompletedReceiptCannotClearAYoungerBarrier) {
    auto session = imageSession();
    ControlAudioUseCase controls{session};
    const auto first = controls.mute(now);
    ASSERT_TRUE(first.cancellation);
    ASSERT_TRUE(controls.acknowledgeCancellation(first.cancellation->cancellationId, 1));
    EXPECT_EQ(controls.unmute(now).status, AudioControlStatus::accepted);
    const auto second = controls.mute(now);
    ASSERT_TRUE(second.cancellation);
    EXPECT_FALSE(controls.acknowledgeCancellation(first.cancellation->cancellationId, 1));
    EXPECT_TRUE(controls.acknowledgeCancellation(second.cancellation->cancellationId, 2));
}

TEST(AudioControlsTest, MissingCancellationAcknowledgmentEndsSession) {
    auto session = imageSession();
    ControlAudioUseCase controls{session};
    EXPECT_EQ(controls.mute(now).status, AudioControlStatus::accepted);
    controls.expireCancellation(now + std::chrono::seconds{5});
    EXPECT_EQ(session.state, State::ending);
    EXPECT_FALSE(controls.offerAutomatic("description-1", "track-1", 1, 1, now));
}

TEST(AudioControlsTest, LatestCompletedSpeechRemainsReplayableAfterSourceRemoval) {
    auto session = imageSession();
    ControlAudioUseCase controls{session};
    controls.acceptDescription("description-1", "Crossing ahead", now);
    const auto offer = controls.offerAutomatic("description-1", "track-1", 1, 1, now);
    ASSERT_TRUE(offer);
    ASSERT_TRUE(controls.recordPlaybackStarted(offer->announcementId, "description-1", 1));
    ASSERT_TRUE(controls.recordPlaybackEnded(offer->announcementId, 1));
    controls.removeSourceHistory();
    const auto replay = controls.repeat(now);
    ASSERT_TRUE(replay.offer);
    EXPECT_TRUE(replay.offer->isReplay);
    EXPECT_FALSE(replay.offer->trackId);
    EXPECT_FALSE(replay.offer->frameId);
    EXPECT_FALSE(controls.recordPlaybackStarted("superseded", "description-1", 1));
}

TEST(AudioControlsTest, RecoveryCoalescesAndNeverRestoresAutomaticBacklog) {
    auto session = imageSession();
    ControlAudioUseCase controls{session};
    controls.acceptDescription("description-1", "Crossing ahead", now);
    const auto offer = controls.offerAutomatic("description-1", "track-1", 1, 1, now);
    ASSERT_TRUE(offer);
    controls.recordPlaybackBlocked(offer->announcementId, 1);
    const auto first = controls.recover("description-1", true, now);
    const auto second = controls.recover("description-1", true, now);
    ASSERT_TRUE(first.offer);
    ASSERT_TRUE(second.offer);
    EXPECT_EQ(first.offer->announcementId, second.offer->announcementId);
    EXPECT_EQ(controls.recover("missing", true, now).status,
              AudioControlStatus::staleDescription);
    EXPECT_FALSE(controls.offerAutomatic("description-1", "track-1", 1, 1, now));
}

TEST(AudioControlsTest, MuteCancelsEveryOldGenerationArtifact) {
    auto session = imageSession();
    ControlAudioUseCase controls{session};
    controls.acceptDescription("description-1", "Crossing ahead", now);
    ASSERT_TRUE(controls.offerAutomatic("description-1", "track-1", 1, 1, now));
    controls.acceptDescription("description-2", "Bus stop", now);
    EXPECT_FALSE(controls.offerAutomatic("description-2", "track-2", 1, 2, now));
    const auto muted = controls.mute(now);
    ASSERT_TRUE(muted.cancellation);
    EXPECT_FALSE(controls.recordPlaybackStarted("announcement-1", "description-1", 1));
    EXPECT_FALSE(controls.recordPlaybackStarted("announcement-2", "description-2", 1));
    EXPECT_TRUE(controls.acknowledgeCancellation(muted.cancellation->cancellationId, 1));
    EXPECT_EQ(controls.unmute(now).status, AudioControlStatus::accepted);
    EXPECT_FALSE(controls.offerAutomatic("description-2", "track-2", 1, 2, now));
}
} // namespace RoadSign::RecognitionSession
