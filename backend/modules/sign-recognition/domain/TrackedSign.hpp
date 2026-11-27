#pragma once

#include "../public/SignObservation.hpp"
#include "../public/TrackEvidenceDto.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace RoadSign::SignRecognition {
struct OcrSample {
    std::string displayText;
    std::string normalizedText;
    double confidence;
    double sourceTimeMs;
};

class TrackedSign {
public:
    enum class State { tentative, confirmed, lost, removed };

    [[nodiscard]] static std::optional<TrackedSign> start(
        std::string sessionId, std::uint32_t epoch, std::uint64_t localId,
        const SignObservation& observation, ImageSize frameSize, bool isStillImage = false);

    [[nodiscard]] bool canAssociate(const SignObservation& observation, std::uint32_t epoch,
                                    ImageSize frameSize) const noexcept;
    [[nodiscard]] bool observe(const SignObservation& observation, std::uint32_t epoch,
                               ImageSize frameSize);
    void confirm() noexcept;
    void markLost(double sourceTimeMs) noexcept;
    [[nodiscard]] bool recordOcr(OcrSample sample);
    [[nodiscard]] bool acceptText(std::string displayText, std::string normalizedText);
    [[nodiscard]] TrackEvidenceDto evidence() const;

    [[nodiscard]] const std::string& trackId() const noexcept { return trackId_; }
    [[nodiscard]] State state() const noexcept { return state_; }
    [[nodiscard]] std::uint32_t categoryId() const noexcept { return categoryId_; }
    [[nodiscard]] std::uint32_t revision() const noexcept { return revision_; }
    [[nodiscard]] double firstSourceTimeMs() const noexcept { return firstSourceTimeMs_; }
    [[nodiscard]] double lastSourceTimeMs() const noexcept { return lastSourceTimeMs_; }
    [[nodiscard]] const std::deque<SignObservation>& detections() const noexcept {
        return detections_;
    }
    [[nodiscard]] const std::deque<OcrSample>& ocrSamples() const noexcept {
        return ocrSamples_;
    }
    [[nodiscard]] const std::optional<std::string>& acceptedText() const noexcept {
        return acceptedText_;
    }
    [[nodiscard]] const std::optional<std::uint32_t>& acceptedCategoryId() const noexcept {
        return acceptedCategoryId_;
    }

private:
    TrackedSign(std::string trackId, std::uint32_t epoch, const SignObservation& observation);
    void remove() noexcept;
    void pruneOcrSamples() noexcept;
    [[nodiscard]] Box predictedBox(double sourceTimeMs) const noexcept;

    std::string trackId_;
    std::uint32_t epoch_;
    std::uint32_t categoryId_;
    State state_{State::tentative};
    double firstSourceTimeMs_;
    double lastSourceTimeMs_;
    std::deque<SignObservation> detections_;
    std::deque<OcrSample> ocrSamples_;
    std::optional<std::uint32_t> acceptedCategoryId_;
    std::optional<std::string> acceptedText_;
    std::string acceptedNormalizedText_;
    std::uint32_t revision_{};
};
} // namespace RoadSign::SignRecognition
