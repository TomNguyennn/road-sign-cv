#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>

namespace RoadSignTest {
class FakeClock {
public:
    using TimePoint = std::chrono::steady_clock::time_point;
    [[nodiscard]] TimePoint now() const {
        std::lock_guard lock(mutex_);
        return now_;
    }
    void advance(std::chrono::milliseconds duration) {
        std::lock_guard lock(mutex_);
        now_ += duration;
    }
private:
    mutable std::mutex mutex_;
    TimePoint now_{};
};

class WorkerBarrier {
public:
    void arriveAndWait() {
        std::unique_lock lock(mutex_);
        arrived_ = true;
        condition_.notify_all();
        condition_.wait(lock, [this] { return released_; });
    }
    void waitUntilArrived() {
        std::unique_lock lock(mutex_);
        condition_.wait(lock, [this] { return arrived_; });
    }
    void release() {
        std::lock_guard lock(mutex_);
        released_ = true;
        condition_.notify_all();
    }
private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool arrived_{};
    bool released_{};
};
struct TestObservation { std::string category; double confidence{}; };
struct TestAudio { std::string text; std::chrono::milliseconds duration{}; };
}
