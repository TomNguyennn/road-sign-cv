#pragma once

#include "shared/Frame.hpp"
#include "shared/Identity.hpp"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

namespace RoadSign::RecognitionSession {
class SessionExecutor {
public:
    using Publish = std::function<bool(const std::function<void()>&)>;
    using ProcessFrame = std::function<void(const Frame&, WorkIdentity, std::stop_token,
                                            const Publish&)>;
    using DroppedFrame = std::function<void(std::uint64_t)>;

    SessionExecutor(ProcessFrame processFrame, DroppedFrame droppedFrame);
    ~SessionExecutor();
    SessionExecutor(const SessionExecutor&) = delete;
    SessionExecutor& operator=(const SessionExecutor&) = delete;

    void setIdentity(WorkIdentity identity);
    void submit(Frame frame, WorkIdentity identity);
    void shutdown();

private:
    struct PendingFrame { Frame frame; WorkIdentity identity; };
    void run(std::stop_token stopToken);

    ProcessFrame processFrame_;
    DroppedFrame droppedFrame_;
    std::mutex mutex_;
    std::condition_variable_any condition_;
    std::optional<WorkIdentity> identity_;
    std::optional<PendingFrame> pending_;
    std::jthread worker_;
};
} // namespace RoadSign::RecognitionSession
