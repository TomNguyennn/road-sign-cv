#include "TrackedSign.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace RoadSign::SignRecognition {
namespace {
constexpr double REACQUISITION_WINDOW_MS = 2000.0;
constexpr double MAX_ASSOCIATION_COST = 0.80;
constexpr std::size_t MAX_DETECTION_HISTORY = 5;
constexpr std::size_t MAX_OCR_HISTORY = 3;
constexpr double MIN_STILL_SCORE = 0.75;
constexpr std::uint32_t STREET_NAME_CATEGORY_ID = 4;
constexpr std::uint32_t BUS_STOP_CATEGORY_ID = 6;

double intersectionOverUnion(Box first, Box second) noexcept {
    const double left = std::max(first.x, second.x);
    const double top = std::max(first.y, second.y);
    const double right = std::min(first.x + first.width, second.x + second.width);
    const double bottom = std::min(first.y + first.height, second.y + second.height);
    const double intersection = std::max(0.0, right - left) * std::max(0.0, bottom - top);
    const double total = first.width * first.height + second.width * second.height - intersection;
    if (!std::isfinite(total) || total <= 0.0) return 0.0;
    return intersection / total;
}
} // namespace

TrackedSign::TrackedSign(std::string trackId, std::uint32_t epoch,
                         const SignObservation& observation)
    : trackId_(std::move(trackId)), epoch_(epoch), categoryId_(observation.categoryId),
      firstSourceTimeMs_(observation.sourceTimeMs), lastSourceTimeMs_(observation.sourceTimeMs) {
    detections_.push_back(observation);
}

std::optional<TrackedSign> TrackedSign::start(
    std::string sessionId, std::uint32_t epoch, std::uint64_t localId,
    const SignObservation& observation, ImageSize frameSize, bool isStillImage) {
    if (sessionId.empty() || epoch == 0 || localId == 0 ||
        (isStillImage && observation.confidence < MIN_STILL_SCORE) ||
        !observation.isValid(epoch, epoch, frameSize)) return std::nullopt;
    auto trackId = sessionId + "/" + std::to_string(epoch) + "/" +
        std::to_string(observation.categoryId) + "/" + std::to_string(localId);
    TrackedSign track{std::move(trackId), epoch, observation};
    if (isStillImage) track.confirm();
    return track;
}

Box TrackedSign::predictedBox(double sourceTimeMs) const noexcept {
    auto predicted = detections_.back().box;
    if (detections_.size() < 2) return predicted;
    const auto& previous = detections_[detections_.size() - 2];
    const auto& latest = detections_.back();
    const double elapsed = latest.sourceTimeMs - previous.sourceTimeMs;
    if (elapsed <= 0.0) return predicted;
    const double ratio = (sourceTimeMs - latest.sourceTimeMs) / elapsed;
    predicted.x += (latest.box.x - previous.box.x) * ratio;
    predicted.y += (latest.box.y - previous.box.y) * ratio;
    return predicted;
}

bool TrackedSign::canAssociate(const SignObservation& observation, std::uint32_t epoch,
                               ImageSize frameSize) const noexcept {
    if (state_ == State::removed || observation.categoryId != categoryId_ ||
        !observation.isValid(epoch, epoch_, frameSize) ||
        observation.sourceTimeMs <= lastSourceTimeMs_ ||
        observation.sourceTimeMs - lastSourceTimeMs_ > REACQUISITION_WINDOW_MS)
        return false;
    const auto predicted = predictedBox(observation.sourceTimeMs).clipped(frameSize);
    const auto candidate = observation.box.clipped(frameSize);
    return predicted && candidate &&
        1.0 - intersectionOverUnion(*predicted, *candidate) <= MAX_ASSOCIATION_COST;
}

bool TrackedSign::observe(const SignObservation& observation, std::uint32_t epoch,
                          ImageSize frameSize) {
    if (!canAssociate(observation, epoch, frameSize)) return false;
    detections_.push_back(observation);
    if (detections_.size() > MAX_DETECTION_HISTORY) detections_.pop_front();
    lastSourceTimeMs_ = observation.sourceTimeMs;
    pruneOcrSamples();
    if (state_ == State::lost)
        state_ = acceptedCategoryId_ ? State::confirmed : State::tentative;
    return true;
}

void TrackedSign::confirm() noexcept {
    if (state_ == State::removed) return;
    state_ = State::confirmed;
    if (!acceptedCategoryId_) {
        acceptedCategoryId_ = categoryId_;
        revision_ = 1;
    }
}

void TrackedSign::remove() noexcept {
    state_ = State::removed;
    detections_.clear();
    ocrSamples_.clear();
}

void TrackedSign::pruneOcrSamples() noexcept {
    while (!ocrSamples_.empty() &&
           lastSourceTimeMs_ - ocrSamples_.front().sourceTimeMs > REACQUISITION_WINDOW_MS)
        ocrSamples_.pop_front();
    if (ocrSamples_.size() > MAX_OCR_HISTORY) ocrSamples_.pop_front();
}

void TrackedSign::markLost(double sourceTimeMs) noexcept {
    if (state_ == State::removed || !std::isfinite(sourceTimeMs) ||
        sourceTimeMs < lastSourceTimeMs_) return;
    if (sourceTimeMs - lastSourceTimeMs_ > REACQUISITION_WINDOW_MS) {
        remove();
        return;
    }
    state_ = State::lost;
}

bool TrackedSign::recordOcr(OcrSample sample) {
    if (state_ == State::removed ||
        (categoryId_ != STREET_NAME_CATEGORY_ID && categoryId_ != BUS_STOP_CATEGORY_ID) ||
        sample.displayText.empty() ||
        sample.normalizedText.empty() || !std::isfinite(sample.confidence) ||
        sample.confidence < 0.0 || sample.confidence > 1.0 ||
        !std::isfinite(sample.sourceTimeMs) || sample.sourceTimeMs > lastSourceTimeMs_ ||
        lastSourceTimeMs_ - sample.sourceTimeMs > REACQUISITION_WINDOW_MS ||
        (!ocrSamples_.empty() && sample.sourceTimeMs < ocrSamples_.back().sourceTimeMs))
        return false;
    ocrSamples_.push_back(std::move(sample));
    pruneOcrSamples();
    return true;
}

bool TrackedSign::acceptText(std::string displayText, std::string normalizedText) {
    if (state_ != State::confirmed ||
        (categoryId_ != STREET_NAME_CATEGORY_ID && categoryId_ != BUS_STOP_CATEGORY_ID) ||
        displayText.empty() || normalizedText.empty() ||
        revision_ == std::numeric_limits<std::uint32_t>::max() ||
        normalizedText == acceptedNormalizedText_)
        return false;
    acceptedText_ = std::move(displayText);
    acceptedNormalizedText_ = std::move(normalizedText);
    ++revision_;
    return true;
}
} // namespace RoadSign::SignRecognition
