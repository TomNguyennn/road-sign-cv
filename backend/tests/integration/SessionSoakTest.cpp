#include "modules/recognition-session/infrastructure/SessionExecutor.hpp"
#include "modules/recognition-session/application/SessionService.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace RoadSign::RecognitionSession {
namespace {
constexpr std::uint64_t TEN_MINUTE_FRAMES = 600;
}

TEST(SessionSoakTest, FrameWorkerKeepsOnlyOneActiveAndLatestPending) {
    std::mutex mutex;
    std::condition_variable condition;
    bool isFirstActive = false;
    bool canComplete = false;
    std::vector<std::uint64_t> processed;
    std::size_t dropped = 0;
    const WorkIdentity identity{1, 1, 1};
    SessionExecutor executor(
        [&](const Frame& frame, WorkIdentity, std::stop_token,
            const SessionExecutor::Publish&) {
            std::unique_lock lock(mutex);
            processed.push_back(frame.frameId());
            isFirstActive = true;
            condition.notify_all();
            if (frame.frameId() == 1)
                condition.wait(lock, [&] { return canComplete; });
            condition.notify_all();
        },
        [&](std::uint64_t) { ++dropped; });
    executor.setIdentity(identity);
    const std::vector<std::uint8_t> bytes(3, 0);
    executor.submit(*Frame::copyOf(1, 0, std::nullopt, {1, 1}, bytes), identity);
    {
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] { return isFirstActive; });
    }
    for (std::uint64_t frameId = 2; frameId <= TEN_MINUTE_FRAMES; ++frameId) {
        executor.submit(*Frame::copyOf(frameId, static_cast<double>((frameId - 1) * 1000),
                                       std::nullopt, {1, 1}, bytes), identity);
    }
    {
        std::lock_guard lock(mutex);
        canComplete = true;
    }
    condition.notify_all();
    {
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] { return processed.size() == 2; });
    }
    EXPECT_EQ(processed, (std::vector<std::uint64_t>{1, TEN_MINUTE_FRAMES}));
    EXPECT_EQ(dropped, TEN_MINUTE_FRAMES - 2);
    executor.shutdown();
}

TEST(SessionSoakTest, EndReleasesSessionOwnedMedia) {
    SessionService sessions{3, "https://demo.example"};
    const auto now = std::chrono::steady_clock::time_point{};
    const auto session = sessions.create(SourceKind::video,
        "https://demo.example", now).session.value();
    ASSERT_TRUE(sessions.attachEvents(session.sessionId, now));
    constexpr std::string_view attemptId = "edc6258a-a013-467e-ace1-865af6048911";
    const auto admission = sessions.admitUpload(session.sessionId, session.csrfToken,
        "https://demo.example", 1, std::string{attemptId}, now);
    ASSERT_TRUE(admission.nonce);
    ASSERT_TRUE(sessions.setUploadValidating(session.sessionId, attemptId,
                                              *admission.nonce, now));
    const auto path = std::filesystem::temp_directory_path() /
        ("road-sign-soak-" + session.sessionId + ".mp4");
    {
        std::ofstream output(path);
        output << "owned fixture";
    }
    auto owner = std::make_shared<MediaProcessing::OwnedTemporaryFile>();
    owner->path = path;
    const std::weak_ptr<MediaProcessing::OwnedTemporaryFile> weakOwner = owner;
    MediaProcessing::MediaSource media{MediaProcessing::SourceKind::video,
        640, 480, 3000, 0, std::move(owner)};
    ASSERT_TRUE(sessions.finishUpload(session.sessionId, attemptId, true,
                                      std::nullopt, std::move(media)));
    EXPECT_FALSE(weakOwner.expired());
    sessions.shutdown();
    EXPECT_TRUE(weakOwner.expired());
    EXPECT_FALSE(std::filesystem::exists(path));
}
} // namespace RoadSign::RecognitionSession
