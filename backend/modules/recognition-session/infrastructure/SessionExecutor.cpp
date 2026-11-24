#include "SessionExecutor.hpp"

#include <utility>

namespace RoadSign::RecognitionSession {
SessionExecutor::SessionExecutor(ProcessFrame processFrame, DroppedFrame droppedFrame)
    : processFrame_(std::move(processFrame)), droppedFrame_(std::move(droppedFrame)),
      worker_([this](std::stop_token token) { run(token); }) {}

SessionExecutor::~SessionExecutor() { shutdown(); }

void SessionExecutor::setIdentity(WorkIdentity identity) {
    std::lock_guard lock(mutex_);
    identity_ = identity;
    if (pending_ && pending_->identity != identity) pending_.reset();
}

void SessionExecutor::submit(Frame frame, WorkIdentity identity) {
    std::optional<std::uint64_t> dropped;
    {
        std::lock_guard lock(mutex_);
        if (!identity_ || *identity_ != identity) return;
        if (pending_) dropped = pending_->frame.frameId();
        pending_ = PendingFrame{std::move(frame), identity};
    }
    if (dropped) droppedFrame_(*dropped);
    condition_.notify_one();
}

void SessionExecutor::shutdown() {
    if (!worker_.joinable()) return;
    worker_.request_stop();
    condition_.notify_all();
    worker_.join();
    std::lock_guard lock(mutex_);
    pending_.reset();
    identity_.reset();
}

void SessionExecutor::run(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        std::optional<PendingFrame> work;
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, stopToken, [this] { return pending_.has_value(); });
            if (stopToken.stop_requested()) return;
            work.emplace(std::move(*pending_));
            pending_.reset();
        }
        const Publish publishIfCurrent = [this, identity = work->identity](const auto& publish) {
            std::lock_guard lock(mutex_);
            if (identity_ && *identity_ == identity) {
                publish();
                return true;
            }
            return false;
        };
        processFrame_(work->frame, work->identity, stopToken, publishIfCurrent);
    }
}
} // namespace RoadSign::RecognitionSession
