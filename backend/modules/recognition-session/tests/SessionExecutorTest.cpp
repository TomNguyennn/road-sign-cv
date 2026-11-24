#include "../infrastructure/SessionExecutor.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace RoadSign::RecognitionSession {
TEST(SessionExecutorTest, HoldsOneActiveAndOnlyLatestPendingOwnedFrame) {
    std::mutex mutex;
    std::condition_variable condition;
    bool releaseFirst = false;
    std::vector<std::uint64_t> processed;
    std::vector<std::uint64_t> dropped;
    const WorkIdentity identity{1, 1, 1};
    SessionExecutor executor(
        [&](const Frame& frame, WorkIdentity, std::stop_token, const auto&) {
            std::unique_lock lock(mutex);
            processed.push_back(frame.frameId());
            condition.notify_all();
            if (frame.frameId() == 1)
                condition.wait(lock, [&] { return releaseFirst; });
        },
        [&](std::uint64_t frameId) { dropped.push_back(frameId); });
    executor.setIdentity(identity);
    const std::vector<std::uint8_t> bytes(3, 1);
    ASSERT_TRUE(Frame::copyOf(1, 0.0, std::nullopt, {1, 1}, bytes, 3));
    executor.submit(*Frame::copyOf(1, 0.0, std::nullopt, {1, 1}, bytes, 3), identity);
    {
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] { return !processed.empty(); });
    }
    executor.submit(*Frame::copyOf(2, 1.0, std::nullopt, {1, 1}, bytes, 3), identity);
    executor.submit(*Frame::copyOf(3, 2.0, std::nullopt, {1, 1}, bytes, 3), identity);
    {
        std::lock_guard lock(mutex);
        releaseFirst = true;
    }
    condition.notify_all();
    {
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] { return processed.size() == 2; });
    }
    EXPECT_EQ(processed, (std::vector<std::uint64_t>{1, 3}));
    EXPECT_EQ(dropped, (std::vector<std::uint64_t>{2}));
}

TEST(SessionExecutorTest, RejectsPublicationAfterIdentityChanges) {
    std::mutex mutex;
    std::condition_variable condition;
    bool attempted = false;
    std::atomic<bool> published = false;
    const WorkIdentity oldIdentity{1, 1, 1};
    SessionExecutor executor(
        [&](const Frame&, WorkIdentity, std::stop_token, const SessionExecutor::Publish& publish) {
            {
                std::unique_lock lock(mutex);
                condition.wait(lock, [&] { return attempted; });
            }
            published = publish([&] { published = true; });
        },
        [](std::uint64_t) {});
    executor.setIdentity(oldIdentity);
    const std::vector<std::uint8_t> bytes(3, 0);
    executor.submit(*Frame::copyOf(1, 0.0, std::nullopt, {1, 1}, bytes), oldIdentity);
    executor.setIdentity({1, 2, 1});
    {
        std::lock_guard lock(mutex);
        attempted = true;
    }
    condition.notify_all();
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    EXPECT_FALSE(published.load());
}

TEST(SessionExecutorTest, SpeechHandoffUsesTheSameStaleGenerationFence) {
    std::mutex mutex;
    std::condition_variable condition;
    bool ready = false;
    bool release = false;
    std::atomic<bool> published = false;
    std::atomic<bool> cancelled = false;
    bool done = false;
    const WorkIdentity identity{9, 3, 4};
    SessionExecutor executor(
        [](const Frame&, WorkIdentity, std::stop_token, const auto&) {},
        [](std::uint64_t) {},
        [&](std::string, WorkIdentity, std::stop_token stopToken,
            const SessionExecutor::Publish& publish) {
            std::unique_lock lock(mutex);
            ready = true;
            condition.notify_all();
            condition.wait(lock, [&] { return release; });
            lock.unlock();
            cancelled = stopToken.stop_requested();
            published = publish([&] { published = true; });
            lock.lock();
            done = true;
            condition.notify_all();
        });
    executor.setIdentity(identity);
    executor.submitSpeech("test phrase", identity);
    {
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] { return ready; });
    }
    executor.setIdentity({9, 4, 4});
    {
        std::lock_guard lock(mutex);
        release = true;
    }
    condition.notify_all();
    {
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] { return done; });
    }
    EXPECT_TRUE(cancelled.load());
    EXPECT_FALSE(published.load());
}
} // namespace RoadSign::RecognitionSession
