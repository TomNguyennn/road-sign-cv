#include "modules/recognition-session/application/SessionService.hpp"
#include "modules/recognition-session/application/ProcessVideoUseCase.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace RoadSign::RecognitionSession {
namespace {
constexpr std::string_view ORIGIN = "https://demo.example";
constexpr std::string_view ATTEMPT = "edc6258a-a013-467e-ace1-865af6048911";

struct VideoSessionFixture {
    SessionService sessions{3, std::string{ORIGIN}};
    std::chrono::steady_clock::time_point now{};
    Entity created = sessions.create(SourceKind::video, ORIGIN, now).session.value();
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("road-sign-contract-" + created.sessionId + ".mp4");

    explicit VideoSessionFixture(bool hasPlayableVideo = false) {
        if (hasPlayableVideo) {
            const auto fixturePath = std::filesystem::path{__FILE__}.parent_path() /
                "../../modules/media-processing/tests/fixtures/box-h264.mp4";
            std::filesystem::copy_file(fixturePath, path);
        } else {
            std::ofstream output(path);
            output << "owned fixture";
        }
        if (!sessions.attachEvents(created.sessionId, now))
            throw std::runtime_error("event attachment failed");
        auto admitted = sessions.admitUpload(created.sessionId, created.csrfToken, ORIGIN,
                                              1, std::string{ATTEMPT}, now);
        if (!admitted.nonce || !sessions.setUploadValidating(created.sessionId, ATTEMPT,
                                                             *admitted.nonce, now))
            throw std::runtime_error("upload admission failed");
        auto owner = std::make_shared<MediaProcessing::OwnedTemporaryFile>();
        owner->path = path;
        MediaProcessing::MediaSource media{MediaProcessing::SourceKind::video, 640, 480,
            hasPlayableVideo ? 16000U : 3000U, 0, owner};
        if (!sessions.finishUpload(created.sessionId, ATTEMPT, true,
                                   std::nullopt, std::move(media)))
            throw std::runtime_error("video acceptance failed");
    }

    ~VideoSessionFixture() { sessions.shutdown(); }
};
} // namespace

TEST(VideoRecognitionContractTest, UploadedVideoProgressesToReviewAtEof) {
    VideoSessionFixture fixture;
    auto& sessions = fixture.sessions;
    ASSERT_TRUE(sessions.beginProcessing(fixture.created.sessionId));
    EXPECT_TRUE(sessions.advanceVideoPosition(fixture.created.sessionId, 1, 200));
    EXPECT_TRUE(sessions.advanceVideoPosition(fixture.created.sessionId, 1, 1200));
    EXPECT_FALSE(sessions.advanceVideoPosition(fixture.created.sessionId, 1, 1000));
    EXPECT_TRUE(sessions.finishVideo(fixture.created.sessionId, 1));
    const auto snapshot = sessions.snapshot();
    ASSERT_TRUE(snapshot);
    EXPECT_EQ(snapshot->state, State::reviewing);
    ASSERT_TRUE(snapshot->acceptedMedia);
    EXPECT_EQ(snapshot->acceptedMedia->positionMs, 3000U);
    EXPECT_TRUE(std::filesystem::exists(fixture.path));
}

TEST(VideoRecognitionContractTest, SeekFencesEpochAndRetainsValidatedUpload) {
    VideoSessionFixture fixture;
    auto& sessions = fixture.sessions;
    ASSERT_TRUE(sessions.beginProcessing(fixture.created.sessionId));
    const auto before = sessions.snapshot().value();
    const auto seeked = sessions.seekVideo(fixture.created.sessionId,
        fixture.created.csrfToken, ORIGIN, 1, 1000);
    ASSERT_TRUE(seeked);
    EXPECT_EQ(seeked->epoch, 2U);
    EXPECT_EQ(seeked->audioGeneration, 2U);
    EXPECT_GT(seeked->stateRevision, before.stateRevision);
    EXPECT_EQ(seeked->state, State::processing);
    ASSERT_TRUE(seeked->acceptedMedia);
    EXPECT_EQ(seeked->acceptedMedia->positionMs, 1000U);
    EXPECT_TRUE(std::filesystem::exists(fixture.path));
    EXPECT_FALSE(sessions.acceptsCallback(fixture.created.sessionId, 1, 1));
    EXPECT_FALSE(sessions.advanceVideoPosition(fixture.created.sessionId, 1, 1200));
    EXPECT_FALSE(sessions.finishVideo(fixture.created.sessionId, 1));
    EXPECT_FALSE(sessions.seekVideo(fixture.created.sessionId,
        fixture.created.csrfToken, ORIGIN, 1, 1500));
    EXPECT_TRUE(sessions.advanceVideoPosition(fixture.created.sessionId, 2, 1200));
}

TEST(VideoRecognitionContractTest, RejectsOutOfRangeAndUnauthorisedSeek) {
    VideoSessionFixture fixture;
    auto& sessions = fixture.sessions;
    ASSERT_TRUE(sessions.beginProcessing(fixture.created.sessionId));
    EXPECT_FALSE(sessions.seekVideo(fixture.created.sessionId, "wrong", ORIGIN, 1, 100));
    EXPECT_FALSE(sessions.seekVideo(fixture.created.sessionId,
                                    fixture.created.csrfToken, ORIGIN, 1, 3000));
    EXPECT_FALSE(sessions.seekVideo(fixture.created.sessionId,
                                    fixture.created.csrfToken, ORIGIN, 1, 3001));
    EXPECT_EQ(sessions.snapshot()->epoch, 1U);
}

TEST(VideoRecognitionContractTest, DecodesPacedFramesAndReviewsAfterEof) {
    VideoSessionFixture fixture{true};
    auto& sessions = fixture.sessions;
    ASSERT_TRUE(sessions.beginProcessing(fixture.created.sessionId));
    ProcessVideoUseCase processor(sessions, fixture.path.parent_path(),
        {100U * 1024U * 1024U, 1920, 1080, 300, 60});
    auto first = processor.next(fixture.created.sessionId, 1, 0);
    ASSERT_EQ(first.status, VideoStepDto::Status::frame);
    ASSERT_TRUE(first.frame);
    auto second = processor.next(fixture.created.sessionId, 1, 0);
    ASSERT_EQ(second.status, VideoStepDto::Status::frame);
    ASSERT_TRUE(second.frame);
    EXPECT_GE(second.frame->sourceTimeMs() - first.frame->sourceTimeMs(), 200.0);
    const auto sought = sessions.seekVideo(fixture.created.sessionId,
        fixture.created.csrfToken, ORIGIN, 1, 1000);
    ASSERT_TRUE(sought);
    EXPECT_EQ(processor.next(fixture.created.sessionId, 1, 0).status,
              VideoStepDto::Status::stale);
    auto replay = processor.next(fixture.created.sessionId, 2, 0);
    ASSERT_EQ(replay.status, VideoStepDto::Status::frame);
    ASSERT_TRUE(replay.frame);
    EXPECT_GE(replay.frame->sourceTimeMs(), 1000.0);
    VideoStepDto step{VideoStepDto::Status::frame, std::nullopt};
    for (int attempt = 0; attempt < 100 && step.status == VideoStepDto::Status::frame;
         ++attempt) {
        step = processor.next(fixture.created.sessionId, 2, 0);
    }
    EXPECT_EQ(step.status, VideoStepDto::Status::endOfFile);
    EXPECT_EQ(sessions.snapshot()->state, State::reviewing);
}
} // namespace RoadSign::RecognitionSession
