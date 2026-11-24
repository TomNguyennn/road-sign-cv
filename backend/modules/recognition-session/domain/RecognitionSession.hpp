#pragma once

#include "UploadReceipt.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace RoadSign::RecognitionSession {
enum class SourceKind { camera, image, video };
enum class State { created, awaitingInput, processing, reviewing, ending, ended };

struct Entity {
    std::string sessionId;
    SourceKind sourceKind;
    std::uint32_t epoch{1};
    State state{State::created};
    std::uint64_t stateRevision{1};
    std::string csrfToken;
    std::uint32_t audioGeneration{1};
    bool isMuted{};
    std::chrono::steady_clock::time_point eventAttachDeadline;
    std::chrono::steady_clock::time_point heartbeatDeadline;
    std::uint32_t uploadAttemptsUsed{};
    std::uint32_t maxUploadAttempts{};
    bool hasAcceptedUpload{};
    UploadReceipt uploadReceipt;
    std::optional<std::string> uploadAdmissionNonce;
    std::optional<std::chrono::steady_clock::time_point> uploadAdmissionDeadline;
    bool eventsAttached{};
    std::optional<std::chrono::steady_clock::time_point> endingSince;

    [[nodiscard]] bool canAdmitUpload() const noexcept {
        return state == State::awaitingInput && !hasAcceptedUpload &&
               uploadReceipt.status != UploadStatus::receiving &&
               uploadReceipt.status != UploadStatus::validating &&
               (maxUploadAttempts == 0 || uploadAttemptsUsed < maxUploadAttempts);
    }
};
} // namespace RoadSign::RecognitionSession
