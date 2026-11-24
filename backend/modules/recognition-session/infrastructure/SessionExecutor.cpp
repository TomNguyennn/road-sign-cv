#include "SessionExecutor.hpp"

#include <utility>

namespace RoadSign::RecognitionSession {
SessionExecutor::SessionExecutor(ProcessFrame processFrame, DroppedFrame droppedFrame,
                                 ProcessSpeech processSpeech)
    : processFrame_(std::move(processFrame)), droppedFrame_(std::move(droppedFrame)),
      processSpeech_(std::move(processSpeech)),
      worker_([this](std::stop_token token) { run(token); }),
      speechWorker_([this](std::stop_token token) { runSpeech(token); }) {}

SessionExecutor::~SessionExecutor() { shutdown(); }

void SessionExecutor::setIdentity(WorkIdentity identity) {
    std::lock_guard lock(mutex_);
    if (identity_ && *identity_ != identity) {
        activeFrameCancellation_.request_stop();
        activeSpeechCancellation_.request_stop();
    }
    identity_ = identity;
    if (pending_ && pending_->identity != identity) pending_.reset();
    if (pendingSpeech_ && pendingSpeech_->identity != identity) pendingSpeech_.reset();
}

void SessionExecutor::submitSpeech(std::string text, WorkIdentity identity) {
    if (!processSpeech_ || text.empty()) return;
    {
        std::lock_guard lock(mutex_);
        if (!identity_ || *identity_ != identity) return;
        pendingSpeech_ = PendingSpeech{std::move(text), identity};
    }
    condition_.notify_all();
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
    speechWorker_.request_stop();
    activeFrameCancellation_.request_stop();
    activeSpeechCancellation_.request_stop();
    condition_.notify_all();
    worker_.join();
    speechWorker_.join();
    std::lock_guard lock(mutex_);
    pending_.reset();
    pendingSpeech_.reset();
    identity_.reset();
}

void SessionExecutor::runSpeech(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        std::optional<PendingSpeech> work;
        std::stop_token workToken;
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, stopToken, [this] { return pendingSpeech_.has_value(); });
            if (stopToken.stop_requested()) return;
            work.emplace(std::move(*pendingSpeech_));
            pendingSpeech_.reset();
            activeSpeechCancellation_ = std::stop_source{};
            workToken = activeSpeechCancellation_.get_token();
        }
        const Publish publishIfCurrent = [this, identity = work->identity](const auto& publish) {
            std::lock_guard lock(mutex_);
            if (!identity_ || *identity_ != identity) return false;
            publish();
            return true;
        };
        processSpeech_(std::move(work->text), work->identity, workToken, publishIfCurrent);
    }
}

void SessionExecutor::run(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        std::optional<PendingFrame> work;
        std::stop_token workToken;
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, stopToken, [this] { return pending_.has_value(); });
            if (stopToken.stop_requested()) return;
            work.emplace(std::move(*pending_));
            pending_.reset();
            activeFrameCancellation_ = std::stop_source{};
            workToken = activeFrameCancellation_.get_token();
        }
        const Publish publishIfCurrent = [this, identity = work->identity](const auto& publish) {
            std::lock_guard lock(mutex_);
            if (identity_ && *identity_ == identity) {
                publish();
                return true;
            }
            return false;
        };
        processFrame_(work->frame, work->identity, workToken, publishIfCurrent);
    }
}
} // namespace RoadSign::RecognitionSession
