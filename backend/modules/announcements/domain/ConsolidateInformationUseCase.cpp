#include "modules/announcements/public/ConsolidateInformationUseCase.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace RoadSign::Announcements {
namespace {
constexpr double CONSENSUS_WINDOW_MS = 2000.0;
constexpr double MIN_CATEGORY_SCORE = 0.60;
constexpr double MIN_OCR_CONFIDENCE = 0.70;
constexpr double OCR_INTERVAL_MS = 1000.0;
constexpr std::size_t MAX_CROPS_PER_FRAME = 4;
constexpr std::size_t REQUIRED_OBSERVATIONS = 3;
constexpr std::size_t DETECTION_HISTORY_LIMIT = 5;
constexpr std::size_t OCR_HISTORY_LIMIT = 3;
constexpr std::uint32_t STREET_NAME_CATEGORY_ID = 4;
constexpr std::uint32_t BUS_STOP_CATEGORY_ID = 6;
constexpr std::uint32_t OBSTACLE_CATEGORY_ID = 7;

bool hasCategoryConsensus(const SignRecognition::TrackEvidenceDto& evidence) {
    std::size_t count = 0;
    std::size_t inspected = 0;
    for (auto it = evidence.detections.rbegin();
         it != evidence.detections.rend() && inspected < DETECTION_HISTORY_LIMIT; ++it, ++inspected) {
        const auto& observation = *it;
        if (observation.categoryId == evidence.categoryId &&
            std::isfinite(observation.confidence) && observation.confidence >= MIN_CATEGORY_SCORE &&
            observation.sourceTimeMs <= evidence.lastSourceTimeMs &&
            evidence.lastSourceTimeMs - observation.sourceTimeMs <= CONSENSUS_WINDOW_MS)
            ++count;
    }
    return count >= REQUIRED_OBSERVATIONS;
}

std::optional<SignRecognition::TextReadingDto> acceptedReading(
    const SignRecognition::TrackEvidenceDto& evidence) {
    const auto oldest = evidence.ocrSamples.rbegin() +
        std::min(OCR_HISTORY_LIMIT, evidence.ocrSamples.size());
    for (auto latest = evidence.ocrSamples.rbegin(); latest != oldest; ++latest) {
        if (!std::isfinite(latest->confidence) || latest->confidence < MIN_OCR_CONFIDENCE ||
            latest->normalizedText.empty() || latest->displayText.empty() ||
            !std::isfinite(latest->sourceTimeMs) ||
            latest->sourceTimeMs > evidence.lastSourceTimeMs ||
            evidence.lastSourceTimeMs - latest->sourceTimeMs > CONSENSUS_WINDOW_MS)
            continue;
        for (auto earlier = latest + 1; earlier != oldest; ++earlier)
            if (std::isfinite(earlier->confidence) &&
                earlier->confidence >= MIN_OCR_CONFIDENCE &&
                !earlier->displayText.empty() &&
                earlier->normalizedText == latest->normalizedText &&
                std::isfinite(earlier->sourceTimeMs) &&
                earlier->sourceTimeMs <= latest->sourceTimeMs &&
                evidence.lastSourceTimeMs - earlier->sourceTimeMs <= CONSENSUS_WINDOW_MS &&
                latest->sourceTimeMs - earlier->sourceTimeMs <= CONSENSUS_WINDOW_MS)
                return *latest;
    }
    return std::nullopt;
}
} // namespace

ConsolidationDecisionDto ConsolidateInformationUseCase::evaluate(
    const SignRecognition::TrackEvidenceDto& evidence, ImageSize frameSize) {
    ConsolidationDecisionDto decision{};
    if (!evidence.isActive || evidence.trackId.empty() || evidence.detections.empty() ||
        evidence.categoryId >= 8 || !std::isfinite(evidence.lastSourceTimeMs) ||
        evidence.lastSourceTimeMs < 0.0) return decision;
    const auto frameId = evidence.detections.back().frameId;
    if (currentFrameId_ && frameId < *currentFrameId_) return decision;
    if (!currentFrameId_ || *currentFrameId_ != frameId) {
        currentFrameId_ = frameId;
        cropsThisFrame_ = 0;
    }
    decision.shouldConfirm = !evidence.isConfirmed && hasCategoryConsensus(evidence);
    if (!evidence.isConfirmed && !decision.shouldConfirm) return decision;
    if (evidence.categoryId == OBSTACLE_CATEGORY_ID) {
        for (auto it = obstacleLastSeenByTrack_.begin();
             it != obstacleLastSeenByTrack_.end();)
            if (evidence.lastSourceTimeMs - it->second > CONSENSUS_WINDOW_MS)
                it = obstacleLastSeenByTrack_.erase(it);
            else
                ++it;
        auto [stored, isNew] = obstacleLastSeenByTrack_.try_emplace(
            evidence.trackId, evidence.lastSourceTimeMs);
        if (isNew) decision.acceptedObstacle = AcceptImageInformationUseCase::describePosition(
            evidence.detections.back().box, frameSize);
        if (!decision.acceptedObstacle && isNew) obstacleLastSeenByTrack_.erase(stored);
        else stored->second = evidence.lastSourceTimeMs;
        return decision;
    }
    const bool isTextCategory = evidence.categoryId == STREET_NAME_CATEGORY_ID ||
                                evidence.categoryId == BUS_STOP_CATEGORY_ID;
    if (!isTextCategory) return decision;
    decision.acceptedText = acceptedReading(evidence);
    for (auto it = lastOcrTimeByTrack_.begin(); it != lastOcrTimeByTrack_.end();)
        if (evidence.lastSourceTimeMs - it->second > CONSENSUS_WINDOW_MS)
            it = lastOcrTimeByTrack_.erase(it);
        else
            ++it;
    const auto lastOcr = lastOcrTimeByTrack_.find(evidence.trackId);
    if (cropsThisFrame_ >= MAX_CROPS_PER_FRAME ||
        (lastOcr != lastOcrTimeByTrack_.end() &&
         evidence.lastSourceTimeMs - lastOcr->second < OCR_INTERVAL_MS)) return decision;
    lastOcrTimeByTrack_[evidence.trackId] = evidence.lastSourceTimeMs;
    ++cropsThisFrame_;
    decision.shouldScheduleOcr = true;
    return decision;
}

void ConsolidateInformationUseCase::forget(std::string_view trackId) {
    lastOcrTimeByTrack_.erase(std::string(trackId));
    obstacleLastSeenByTrack_.erase(std::string(trackId));
}

void ConsolidateInformationUseCase::reset() noexcept {
    currentFrameId_.reset();
    cropsThisFrame_ = 0;
    lastOcrTimeByTrack_.clear();
    obstacleLastSeenByTrack_.clear();
}
} // namespace RoadSign::Announcements
