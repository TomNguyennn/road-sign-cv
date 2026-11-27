#include "modules/announcements/public/ConsolidateInformationUseCase.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace RoadSign::Announcements {
namespace {
SignRecognition::TrackEvidenceDto evidence(std::string trackId, std::uint32_t category,
                                            std::uint64_t frameId, double sourceTimeMs,
                                            bool isConfirmed = false) {
    SignRecognition::TrackEvidenceDto result{std::move(trackId), category, 0, true,
                                              isConfirmed, sourceTimeMs, {}, {}};
    result.detections.push_back({frameId, category, 0.8, Box{10, 10, 20, 20},
                                 std::nullopt, std::nullopt, sourceTimeMs});
    return result;
}

void addDetection(SignRecognition::TrackEvidenceDto& track, std::uint64_t frameId,
                  double time, double confidence = 0.8) {
    track.detections.push_back({frameId, track.categoryId, confidence, Box{10, 10, 20, 20},
                                std::nullopt, std::nullopt, time});
    track.lastSourceTimeMs = time;
}
} // namespace

TEST(ConsolidateInformationUseCaseTest, ConfirmsOnlyThreeStrongRecentObservations) {
    ConsolidateInformationUseCase policy;
    auto track = evidence("track", 1, 1, 0);
    EXPECT_FALSE(policy.evaluate(track).shouldConfirm);
    addDetection(track, 2, 1000, 0.59);
    addDetection(track, 3, 1500);
    EXPECT_FALSE(policy.evaluate(track).shouldConfirm);
    addDetection(track, 4, 2000);
    EXPECT_TRUE(policy.evaluate(track).shouldConfirm);
    addDetection(track, 5, 2501, 0.1);
    EXPECT_FALSE(policy.evaluate(track).shouldConfirm);
    track.isActive = false;
    EXPECT_FALSE(policy.evaluate(track).shouldConfirm);
}

TEST(ConsolidateInformationUseCaseTest, UsesOnlyTheLastFiveDetections) {
    ConsolidateInformationUseCase policy;
    auto track = evidence("track", 1, 1, 0);
    addDetection(track, 2, 100);
    addDetection(track, 3, 200);
    for (std::uint64_t frame = 4; frame <= 8; ++frame)
        addDetection(track, frame, static_cast<double>(frame * 100), 0.59);
    EXPECT_FALSE(policy.evaluate(track).shouldConfirm);
}

TEST(ConsolidateInformationUseCaseTest, LimitsOcrToOnePerTrackPerSecond) {
    ConsolidateInformationUseCase policy;
    auto track = evidence("street", 4, 1, 0, true);
    EXPECT_TRUE(policy.evaluate(track).shouldScheduleOcr);
    EXPECT_FALSE(policy.evaluate(track).shouldScheduleOcr);
    addDetection(track, 2, 999);
    EXPECT_FALSE(policy.evaluate(track).shouldScheduleOcr);
    addDetection(track, 3, 1000);
    EXPECT_TRUE(policy.evaluate(track).shouldScheduleOcr);
    track.isActive = false;
    EXPECT_FALSE(policy.evaluate(track).shouldScheduleOcr);
    policy.forget("street");
}

TEST(ConsolidateInformationUseCaseTest, RejectsStaleFramesAfterScheduling) {
    ConsolidateInformationUseCase policy;
    auto current = evidence("street", 4, 10, 1000, true);
    EXPECT_TRUE(policy.evaluate(current).shouldScheduleOcr);
    auto stale = evidence("other", 4, 9, 900, true);
    EXPECT_FALSE(policy.evaluate(stale).shouldScheduleOcr);
}

TEST(ConsolidateInformationUseCaseTest, LimitsOcrCropsAcrossTracksInOneFrame) {
    ConsolidateInformationUseCase policy;
    for (int index = 0; index < 5; ++index) {
        auto track = evidence("street-" + std::to_string(index), 4, 10, 1000, true);
        EXPECT_EQ(policy.evaluate(track).shouldScheduleOcr, index < 4);
    }
    auto nextFrame = evidence("street-4", 4, 11, 2000, true);
    EXPECT_TRUE(policy.evaluate(nextFrame).shouldScheduleOcr);
    policy.reset();
    EXPECT_TRUE(policy.evaluate(nextFrame).shouldScheduleOcr);
}

TEST(ConsolidateInformationUseCaseTest, AcceptsTwoMatchingReadingsAndIgnoresTransientConflict) {
    ConsolidateInformationUseCase policy;
    auto track = evidence("street", 4, 3, 1500, true);
    track.ocrSamples.push_back({"High Street", "high street", 0.8, 0});
    EXPECT_FALSE(policy.evaluate(track).acceptedText);
    track.ocrSamples.push_back({"HIGH STREET", "high street", 0.9, 1000});
    auto accepted = policy.evaluate(track).acceptedText;
    ASSERT_TRUE(accepted);
    EXPECT_EQ(accepted->normalizedText, "high street");
    track.ocrSamples.push_back({"Broad Street", "broad street", 0.9, 1400});
    accepted = policy.evaluate(track).acceptedText;
    ASSERT_TRUE(accepted);
    EXPECT_EQ(accepted->normalizedText, "high street");
    track.ocrSamples.erase(track.ocrSamples.begin());
    track.ocrSamples.push_back({"Broad Street", "broad street", 0.9, 1500});
    accepted = policy.evaluate(track).acceptedText;
    ASSERT_TRUE(accepted);
    EXPECT_EQ(accepted->normalizedText, "broad street");
}

TEST(ConsolidateInformationUseCaseTest, RejectsOldAndLowConfidenceReadings) {
    ConsolidateInformationUseCase policy;
    auto track = evidence("bus", 6, 3, 3000, true);
    track.ocrSamples = {{"Bus Stop", "bus stop", 0.9, 999},
                        {"Bus Stop", "bus stop", 0.69, 2500},
                        {"Bus Stop", "bus stop", 0.9, 3000}};
    EXPECT_FALSE(policy.evaluate(track).acceptedText);
    track.ocrSamples[1].confidence = 0.70;
    EXPECT_TRUE(policy.evaluate(track).acceptedText);
    auto nonText = evidence("warning", 0, 3, 3000, true);
    nonText.ocrSamples = track.ocrSamples;
    EXPECT_FALSE(policy.evaluate(nonText).acceptedText);
    EXPECT_FALSE(policy.evaluate(nonText).shouldScheduleOcr);
    track.ocrSamples = {{"Bus Stop", "bus stop", 0.9, 999},
                        {"Bus Stop", "bus stop", 0.9, 2500}};
    EXPECT_FALSE(policy.evaluate(track).acceptedText);
}

TEST(ConsolidateInformationUseCaseTest, UsesOnlyTheLastThreeOcrSamples) {
    ConsolidateInformationUseCase policy;
    auto track = evidence("street", 4, 4, 1200, true);
    track.ocrSamples = {{"High Street", "high street", 0.9, 0},
                        {"High Street", "high street", 0.9, 100},
                        {"Broad Street", "broad street", 0.9, 200},
                        {"Church Road", "church road", 0.9, 300}};
    EXPECT_FALSE(policy.evaluate(track).acceptedText);
}
} // namespace RoadSign::Announcements
