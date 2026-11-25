#include "../infrastructure/SessionEventBuffer.hpp"

#include <gtest/gtest.h>

#include <string>

namespace RoadSign::RecognitionSession {
namespace {
BufferedEvent frame(std::uint64_t frameId, std::uint64_t revision = 2) {
    Json::Value payload;
    payload["frameId"] = Json::UInt64{frameId};
    payload["observations"] = Json::arrayValue;
    return {BufferedEvent::Kind::frameResult, "session-1", 1, revision, std::move(payload)};
}

BufferedEvent cancel(std::string sessionId = "session-1") {
    Json::Value payload;
    payload["cancellationId"] = "cancel-1";
    payload["cancelThroughAudioGeneration"] = 1;
    payload["audioGeneration"] = 2;
    payload["reason"] = "mute";
    return {BufferedEvent::Kind::audioCancel, std::move(sessionId), 1, 2,
            std::move(payload)};
}
} // namespace

TEST(SessionEventBufferTest, SendsCancellationBeforeOnlyTheLatestFrame) {
    SessionEventBuffer buffer;
    ASSERT_TRUE(buffer.publishFrameResult(frame(1)));
    ASSERT_TRUE(buffer.publishFrameResult(frame(2)));
    ASSERT_TRUE(buffer.publishAudioCancel(cancel()));
    const auto events = buffer.drain("session-1", 1, 2);
    ASSERT_EQ(events.size(), 2);
    EXPECT_EQ(events[0].kind, BufferedEvent::Kind::audioCancel);
    EXPECT_EQ(events[1].kind, BufferedEvent::Kind::frameResult);
    EXPECT_EQ(events[1].payload["frameId"].asUInt64(), 2);
    EXPECT_TRUE(buffer.drain("session-1", 1, 2).empty());
}

TEST(SessionEventBufferTest, DropsStaleFramesButRetainsCurrentCancellation) {
    SessionEventBuffer buffer;
    ASSERT_TRUE(buffer.publishFrameResult(frame(1, 1)));
    ASSERT_TRUE(buffer.publishAudioCancel(cancel()));
    const auto events = buffer.drain("session-1", 2, 2);
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].kind, BufferedEvent::Kind::audioCancel);
    ASSERT_TRUE(buffer.publishAudioCancel(cancel("old-session")));
    EXPECT_TRUE(buffer.drain("new-session", 1, 2).empty());
}

TEST(SessionEventBufferTest, RejectsOversizedOrUnboundedResults) {
    SessionEventBuffer buffer;
    auto oversized = frame(1);
    oversized.payload["text"] = std::string(61U * 1024U, 'x');
    EXPECT_FALSE(buffer.publishFrameResult(std::move(oversized)));
    auto observations = frame(2);
    for (int index = 0; index < 101; ++index)
        observations.payload["observations"].append(Json::objectValue);
    EXPECT_FALSE(buffer.publishFrameResult(std::move(observations)));
    EXPECT_TRUE(buffer.drain("session-1", 1, 2).empty());
}
} // namespace RoadSign::RecognitionSession
