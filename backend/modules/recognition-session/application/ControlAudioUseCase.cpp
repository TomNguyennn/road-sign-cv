#include "ControlAudioUseCase.hpp"

#include <uuid/uuid.h>

#include <limits>
#include <utility>

namespace RoadSign::RecognitionSession {
std::string ControlAudioUseCase::randomUuid() {
    uuid_t bytes;
    uuid_generate_random(bytes);
    char output[37]{};
    uuid_unparse_lower(bytes, output);
    return output;
}

void ControlAudioUseCase::acceptDescription(std::string descriptionId, std::string text,
                                            std::chrono::steady_clock::time_point now) {
    static_cast<void>(now);
    if (descriptionId.empty() || text.empty() || text.size() > 500) return;
    latestDescription_ = Description{std::move(descriptionId), std::move(text)};
}

std::optional<AudioOffer> ControlAudioUseCase::offerAutomatic(
    std::string_view descriptionId, std::string trackId, std::uint32_t revision,
    std::uint64_t frameId, std::chrono::steady_clock::time_point now) {
    static_cast<void>(now);
    if (!latestDescription_ || latestDescription_->id != descriptionId ||
        lastAutomaticDescriptionId_ == descriptionId ||
        session_.state == State::ending || session_.state == State::ended || session_.isMuted ||
        session_.isAudioBlocked || pendingCancellation_ || activeOffer_ ||
        trackId.empty() || revision == 0 || frameId == 0) return std::nullopt;
    activeOffer_ = AudioOffer{randomUuid(), latestDescription_->id, latestDescription_->text,
                              "automatic", false, std::move(trackId), revision, frameId, std::nullopt};
    lastAutomaticDescriptionId_ = latestDescription_->id;
    return activeOffer_;
}

AudioControlResult ControlAudioUseCase::mute(std::chrono::steady_clock::time_point now) {
    if (session_.audioGeneration == std::numeric_limits<std::uint32_t>::max())
        return {AudioControlStatus::staleDescription, std::nullopt, std::nullopt};
    const auto cutoff = session_.audioGeneration++;
    session_.isMuted = true;
    ++session_.stateRevision;
    if (latestDescription_) lastAutomaticDescriptionId_ = latestDescription_->id;
    activeOffer_.reset();
    pendingCancellation_ = AudioCancellation{randomUuid(), cutoff, now + std::chrono::seconds{5}};
    return {AudioControlStatus::accepted, std::nullopt, pendingCancellation_};
}

AudioControlResult ControlAudioUseCase::unmute(std::chrono::steady_clock::time_point now) {
    static_cast<void>(now);
    if (session_.isMuted) {
        session_.isMuted = false;
        ++session_.stateRevision;
    }
    return {AudioControlStatus::accepted, std::nullopt, std::nullopt};
}

AudioControlResult ControlAudioUseCase::enable(std::chrono::steady_clock::time_point now) {
    static_cast<void>(now);
    if (session_.isMuted) return {AudioControlStatus::muted, std::nullopt, std::nullopt};
    isActivated_ = true;
    if (session_.isAudioBlocked) {
        session_.isAudioBlocked = false;
        ++session_.stateRevision;
    }
    return {AudioControlStatus::accepted, std::nullopt, std::nullopt};
}

AudioOffer ControlAudioUseCase::manualOffer(const Description& description, std::string origin) {
    return {randomUuid(), description.id, description.text, std::move(origin), true,
            std::nullopt, std::nullopt, std::nullopt, std::nullopt};
}

AudioControlResult ControlAudioUseCase::repeat(std::chrono::steady_clock::time_point now) {
    static_cast<void>(now);
    if (session_.isMuted) return {AudioControlStatus::muted, std::nullopt, std::nullopt};
    if (!latestStarted_) return {AudioControlStatus::empty, std::nullopt, std::nullopt};
    if (activeOffer_ && activeOffer_->origin == "repeat")
        return {AudioControlStatus::accepted, activeOffer_, std::nullopt};
    if (pendingCancellation_ || activeOffer_) return {AudioControlStatus::accepted, std::nullopt, std::nullopt};
    activeOffer_ = manualOffer(*latestStarted_, "repeat");
    return {AudioControlStatus::accepted, activeOffer_, std::nullopt};
}

AudioControlResult ControlAudioUseCase::recover(
    std::string_view descriptionId, bool activated, std::chrono::steady_clock::time_point now) {
    static_cast<void>(now);
    if (session_.isMuted) return {AudioControlStatus::muted, std::nullopt, std::nullopt};
    if (!latestDescription_ || latestDescription_->id != descriptionId)
        return {AudioControlStatus::staleDescription, std::nullopt, std::nullopt};
    if (!activated) return {AudioControlStatus::activationRequired, std::nullopt, std::nullopt};
    if (session_.isAudioBlocked) {
        session_.isAudioBlocked = false;
        ++session_.stateRevision;
    }
    if (activeOffer_ && activeOffer_->origin == "recovery" &&
        activeOffer_->descriptionId == descriptionId)
        return {AudioControlStatus::accepted, activeOffer_, std::nullopt};
    if (pendingCancellation_ || activeOffer_) return {AudioControlStatus::accepted, std::nullopt, std::nullopt};
    activeOffer_ = manualOffer(*latestDescription_, "recovery");
    return {AudioControlStatus::accepted, activeOffer_, std::nullopt};
}

bool ControlAudioUseCase::recordPlaybackStarted(std::string_view announcementId,
                                                 std::string_view descriptionId,
                                                 std::uint32_t generation) {
    if (!activeOffer_ || activeOffer_->announcementId != announcementId ||
        activeOffer_->descriptionId != descriptionId || generation != session_.audioGeneration ||
        pendingCancellation_) return false;
    latestStarted_ = Description{activeOffer_->descriptionId, activeOffer_->text};
    return true;
}

bool ControlAudioUseCase::recordPlaybackEnded(std::string_view announcementId,
                                               std::uint32_t generation) {
    if (!activeOffer_ || activeOffer_->announcementId != announcementId ||
        generation != session_.audioGeneration) return false;
    activeOffer_.reset();
    return true;
}

void ControlAudioUseCase::recordPlaybackBlocked(std::string_view announcementId,
                                                 std::uint32_t generation) {
    if (!activeOffer_ || activeOffer_->announcementId != announcementId ||
        generation != session_.audioGeneration) return;
    activeOffer_.reset();
    session_.isAudioBlocked = true;
    isActivated_ = false;
    ++session_.stateRevision;
}

bool ControlAudioUseCase::acknowledgeCancellation(std::string_view cancellationId,
                                                   std::uint32_t cutoff) {
    if (pendingCancellation_) {
        if (pendingCancellation_->cancellationId != cancellationId ||
            pendingCancellation_->cancelThroughAudioGeneration != cutoff) return false;
        completedCancellation_ = pendingCancellation_;
        pendingCancellation_.reset();
        return true;
    }
    return completedCancellation_ && completedCancellation_->cancellationId == cancellationId &&
           completedCancellation_->cancelThroughAudioGeneration == cutoff;
}

void ControlAudioUseCase::expireCancellation(std::chrono::steady_clock::time_point now) {
    if (pendingCancellation_ && now >= pendingCancellation_->deadline) {
        session_.state = State::ending;
        ++session_.stateRevision;
        activeOffer_.reset();
    }
}

void ControlAudioUseCase::clearHistory() {
    latestDescription_.reset();
    latestStarted_.reset();
    activeOffer_.reset();
    lastAutomaticDescriptionId_.clear();
}
} // namespace RoadSign::RecognitionSession
