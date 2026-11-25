#include "SessionEventBuffer.hpp"

#include <cstddef>
#include <utility>

namespace RoadSign::RecognitionSession {
namespace {
constexpr std::size_t kMaxPayloadBytes = 60U * 1024U;
constexpr std::size_t kMaxObservations = 100;
}

bool SessionEventBuffer::isBounded(const BufferedEvent& event) {
    if (event.sessionId.empty() || event.epoch == 0 || event.stateRevision == 0 ||
        !event.payload.isObject()) return false;
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    return Json::writeString(writer, event.payload).size() <= kMaxPayloadBytes;
}

bool SessionEventBuffer::publishFrameResult(BufferedEvent event) {
    if (event.kind != BufferedEvent::Kind::frameResult || !isBounded(event) ||
        !event.payload.isMember("observations") ||
        !event.payload["observations"].isArray() ||
        event.payload["observations"].size() > kMaxObservations)
        return false;
    std::lock_guard lock(mutex_);
    latestResult_ = std::move(event);
    return true;
}

bool SessionEventBuffer::publishAudioCancel(BufferedEvent event) {
    if (event.kind != BufferedEvent::Kind::audioCancel || !isBounded(event) ||
        !event.payload.isMember("cancellationId") ||
        !event.payload["cancellationId"].isString())
        return false;
    std::lock_guard lock(mutex_);
    cancellation_ = std::move(event);
    return true;
}

std::vector<BufferedEvent> SessionEventBuffer::drain(
    std::string_view sessionId, std::uint32_t epoch, std::uint64_t stateRevision) {
    std::lock_guard lock(mutex_);
    std::vector<BufferedEvent> events;
    if (cancellation_ && cancellation_->sessionId == sessionId)
        events.push_back(std::move(*cancellation_));
    cancellation_.reset();
    if (latestResult_ && latestResult_->sessionId == sessionId &&
        latestResult_->epoch == epoch && latestResult_->stateRevision == stateRevision)
        events.push_back(std::move(*latestResult_));
    latestResult_.reset();
    return events;
}

void SessionEventBuffer::clear(std::string_view sessionId) {
    std::lock_guard lock(mutex_);
    if (latestResult_ && latestResult_->sessionId == sessionId) latestResult_.reset();
    if (cancellation_ && cancellation_->sessionId == sessionId) cancellation_.reset();
}
} // namespace RoadSign::RecognitionSession
