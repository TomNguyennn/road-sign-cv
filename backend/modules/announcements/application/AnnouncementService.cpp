#include "modules/announcements/public/AnnouncementService.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace RoadSign::Announcements {
namespace {
constexpr std::size_t MAX_PENDING = 3;
constexpr auto FRESHNESS = std::chrono::seconds(5);
constexpr auto CANCELLATION_ACK_TIMEOUT = std::chrono::seconds(5);

bool isExpired(const AnnouncementDto& item, AnnouncementTime now) {
    return now >= item.createdAt + FRESHNESS;
}

bool precedes(const AnnouncementDto& left, const AnnouncementDto& right) {
    if (left.priority != right.priority) {
        return left.priority < right.priority;
    }
    if (left.createdAt != right.createdAt) {
        return left.createdAt < right.createdAt;
    }
    return left.announcementId < right.announcementId;
}

bool hasValidSource(const AcceptedInformation& information) {
    if (information.trackId.empty() || information.revision == 0 || information.frameId == 0 ||
        information.displayText.empty() || information.categoryId > 7) {
        return false;
    }
    if (information.clientCaptureTimeMs &&
        (!std::isfinite(*information.clientCaptureTimeMs) || *information.clientCaptureTimeMs < 0)) {
        return false;
    }
    const auto expectedPriority = information.categoryId == 5 ? 1 :
        (information.categoryId == 4 || information.categoryId == 6 ? 2 : 0);
    return information.priority == expectedPriority;
}
} // namespace

void AnnouncementService::expirePending(AnnouncementTime now) {
    std::erase_if(pending_, [now](const AnnouncementDto& item) {
        return isExpired(item, now);
    });
}

bool AnnouncementService::hasSignature(std::string_view trackId, std::uint32_t revision) const {
    const auto started = startedRevisionByTrack_.find(std::string(trackId));
    if (started != startedRevisionByTrack_.end() && revision <= started->second) {
        return true;
    }
    if (active_ && active_->trackId == trackId && active_->revision == revision) {
        return true;
    }
    return std::any_of(pending_.begin(), pending_.end(), [&](const AnnouncementDto& item) {
        return item.trackId == trackId && item.revision == revision;
    });
}

bool AnnouncementService::barrierTimedOut(AnnouncementTime now) noexcept {
    if (!barrier_ || now < barrier_->deadline) {
        return false;
    }
    reset();
    isEnded_ = true;
    return true;
}

EnqueueDecisionDto AnnouncementService::enqueue(const AcceptedInformation& information,
                                                 std::string announcementId,
                                                 AnnouncementTime now) {
    if (isEnded_ || barrierTimedOut(now) || announcementId.empty() ||
        !hasValidSource(information)) {
        return {false, std::nullopt};
    }
    expirePending(now);
    if (hasSignature(information.trackId, information.revision) ||
        (active_ && active_->announcementId == announcementId) ||
        std::any_of(pending_.begin(), pending_.end(), [&](const AnnouncementDto& item) {
            return item.announcementId == announcementId;
        })) {
        return {false, std::nullopt};
    }
    AnnouncementDto candidate{std::move(announcementId), information.trackId,
                              information.revision, information.frameId,
                              information.clientCaptureTimeMs, information.displayText,
                              information.priority, now, AnnouncementState::pending};
    if (pending_.size() < MAX_PENDING) {
        pending_.push_back(std::move(candidate));
        return {true, std::nullopt};
    }
    const auto worst = std::max_element(pending_.begin(), pending_.end(), precedes);
    if (candidate.priority >= worst->priority) {
        return {false, candidate.announcementId};
    }
    std::string droppedId = worst->announcementId;
    *worst = std::move(candidate);
    return {true, std::move(droppedId)};
}

std::optional<AnnouncementDto> AnnouncementService::beginSynthesis(AnnouncementTime now) {
    if (isEnded_ || barrierTimedOut(now) || barrier_ || active_) {
        return std::nullopt;
    }
    expirePending(now);
    if (pending_.empty()) {
        return std::nullopt;
    }
    const auto next = std::min_element(pending_.begin(), pending_.end(), precedes);
    active_ = std::move(*next);
    pending_.erase(next);
    active_->state = AnnouncementState::synthesizing;
    return active_;
}

bool AnnouncementService::offer(std::string_view announcementId, AnnouncementTime now) {
    if (isEnded_ || barrierTimedOut(now) || !active_ ||
        active_->announcementId != announcementId ||
        active_->state != AnnouncementState::synthesizing) {
        return false;
    }
    if (isExpired(*active_, now)) {
        active_.reset();
        return false;
    }
    active_->state = AnnouncementState::offered;
    return true;
}

bool AnnouncementService::started(std::string_view announcementId, AnnouncementTime now) {
    if (isEnded_ || barrierTimedOut(now) || !active_ ||
        active_->announcementId != announcementId ||
        active_->state != AnnouncementState::offered || isExpired(*active_, now)) {
        return false;
    }
    auto& revision = startedRevisionByTrack_[active_->trackId];
    revision = std::max(revision, active_->revision);
    active_->state = AnnouncementState::playing;
    return true;
}

bool AnnouncementService::completed(std::string_view announcementId) {
    if (isEnded_ || !active_ || active_->announcementId != announcementId ||
        active_->state != AnnouncementState::playing) {
        return false;
    }
    active_.reset();
    return true;
}

bool AnnouncementService::failedSynthesis(std::string_view announcementId) {
    if (isEnded_ || !active_ || active_->announcementId != announcementId ||
        active_->state != AnnouncementState::synthesizing) {
        return false;
    }
    active_.reset();
    return true;
}

void AnnouncementService::startCancellation(std::string cancellationId,
                                            std::uint32_t cancelThroughAudioGeneration,
                                            AnnouncementTime now) {
    barrier_ = CancellationBarrier{std::move(cancellationId),
                                   cancelThroughAudioGeneration,
                                   now + CANCELLATION_ACK_TIMEOUT};
}

bool AnnouncementService::invalidateTrack(std::string_view trackId,
                                          std::optional<std::uint32_t> currentRevision,
                                          std::string cancellationId,
                                          std::uint32_t cancelThroughAudioGeneration,
                                          AnnouncementTime now) {
    if (isEnded_ || barrierTimedOut(now)) {
        return false;
    }
    const auto obsolete = [&](const AnnouncementDto& item) {
        return item.trackId == trackId &&
            (!currentRevision || item.revision < *currentRevision);
    };
    std::erase_if(pending_, obsolete);
    if (!active_ || !obsolete(*active_) || active_->state == AnnouncementState::playing) {
        return false;
    }
    if (active_->state == AnnouncementState::synthesizing) {
        active_.reset();
        return false;
    }
    if (cancellationId.empty() || cancelThroughAudioGeneration == 0) {
        return false;
    }
    active_.reset();
    startCancellation(std::move(cancellationId), cancelThroughAudioGeneration, now);
    return true;
}

bool AnnouncementService::expire(AnnouncementTime now, std::string cancellationId,
                                  std::uint32_t cancelThroughAudioGeneration) {
    if (isEnded_ || barrierTimedOut(now)) {
        return false;
    }
    expirePending(now);
    if (!active_ || !isExpired(*active_, now) ||
        active_->state == AnnouncementState::playing) {
        return false;
    }
    if (active_->state == AnnouncementState::synthesizing) {
        active_.reset();
        return false;
    }
    if (cancellationId.empty() || cancelThroughAudioGeneration == 0) {
        return false;
    }
    active_.reset();
    startCancellation(std::move(cancellationId), cancelThroughAudioGeneration, now);
    return true;
}

bool AnnouncementService::acknowledgeCancellation(
    std::string_view cancellationId, std::uint32_t cancelThroughAudioGeneration,
    AnnouncementTime now) {
    if (isEnded_ || barrierTimedOut(now) || !barrier_ ||
        barrier_->cancellationId != cancellationId ||
        barrier_->generationCutoff != cancelThroughAudioGeneration) {
        return false;
    }
    barrier_.reset();
    return true;
}

void AnnouncementService::forgetTrack(std::string_view trackId) {
    startedRevisionByTrack_.erase(std::string(trackId));
}

void AnnouncementService::reset() noexcept {
    pending_.clear();
    active_.reset();
    startedRevisionByTrack_.clear();
    barrier_.reset();
    isEnded_ = false;
}
} // namespace RoadSign::Announcements
