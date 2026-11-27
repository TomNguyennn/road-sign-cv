#include "ProcessVideoUseCase.hpp"

#include <algorithm>
#include <utility>

namespace RoadSign::RecognitionSession {
ProcessVideoUseCase::ProcessVideoUseCase(SessionService& sessions,
                                         std::filesystem::path spoolRoot,
                                         MediaProcessing::VideoLimits limits)
    : sessions_(sessions), spoolRoot_(std::move(spoolRoot)), limits_(limits) {}

bool ProcessVideoUseCase::openFor(const Entity& session) {
    if (!session.acceptedMedia || !session.acceptedMedia->positionMs) return false;
    auto decoder = MediaProcessing::OpenCvVideoClient::open(
        *session.acceptedMedia, spoolRoot_, limits_);
    if (!decoder) return false;
    if (*session.acceptedMedia->positionMs > 0 &&
        !decoder->seek(*session.acceptedMedia->positionMs)) {
        return false;
    }
    decoder_ = std::move(decoder);
    sessionId_ = session.sessionId;
    epoch_ = session.epoch;
    return true;
}

VideoStepDto ProcessVideoUseCase::next(std::string_view sessionId, std::uint32_t epoch,
                                       std::uint64_t minimumSourceTimeMs) {
    const auto session = sessions_.snapshot();
    if (!session || session->sessionId != sessionId || session->epoch != epoch ||
        session->sourceKind != SourceKind::video || session->state != State::processing) {
        reset();
        return {VideoStepDto::Status::stale, std::nullopt};
    }
    if ((!decoder_ || sessionId_ != sessionId || epoch_ != epoch) && !openFor(*session)) {
        reset();
        return {VideoStepDto::Status::failed, std::nullopt};
    }
    const auto sourcePositionMs = session->acceptedMedia->positionMs.value_or(0);
    const auto decoded = decoder_->nextSample(std::max(minimumSourceTimeMs, sourcePositionMs));
    if (!decoded) {
        if (decoder_->hasFailed()) {
            reset();
            return {VideoStepDto::Status::failed, std::nullopt};
        }
        if (!sessions_.finishVideo(sessionId, epoch)) {
            reset();
            return {VideoStepDto::Status::stale, std::nullopt};
        }
        reset();
        return {VideoStepDto::Status::endOfFile, std::nullopt};
    }
    auto frame = Frame::copyOf(decoded->frameId,
        static_cast<double>(decoded->sourceTimeMs), std::nullopt,
        decoded->image.size, decoded->image.rgbPixels);
    if (!frame) return {VideoStepDto::Status::failed, std::nullopt};
    if (!sessions_.advanceVideoPosition(sessionId, epoch, decoded->sourceTimeMs)) {
        reset();
        return {VideoStepDto::Status::stale, std::nullopt};
    }
    return {VideoStepDto::Status::frame, std::move(frame)};
}

void ProcessVideoUseCase::reset() noexcept {
    decoder_.reset();
    sessionId_.clear();
    epoch_ = 0;
}
} // namespace RoadSign::RecognitionSession
