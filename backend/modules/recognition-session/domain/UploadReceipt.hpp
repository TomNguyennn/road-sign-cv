#pragma once

#include <optional>
#include <string>

namespace RoadSign::RecognitionSession {
enum class UploadStatus { idle, receiving, validating, accepted, rejected, aborted };

struct UploadReceipt {
    std::optional<std::string> uploadAttemptId;
    UploadStatus status{UploadStatus::idle};
    std::optional<std::string> errorCode;
};
} // namespace RoadSign::RecognitionSession
