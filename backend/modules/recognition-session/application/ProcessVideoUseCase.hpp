#pragma once

#include "SessionService.hpp"
#include "modules/media-processing/public/OpenCvVideoClient.hpp"
#include "shared/Frame.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace RoadSign::RecognitionSession {
struct VideoStepDto {
    enum class Status { frame, endOfFile, stale, failed } status;
    std::optional<Frame> frame;
};

class ProcessVideoUseCase {
public:
    ProcessVideoUseCase(SessionService& sessions, std::filesystem::path spoolRoot,
                        MediaProcessing::VideoLimits limits);

    [[nodiscard]] VideoStepDto next(std::string_view sessionId, std::uint32_t epoch,
                                    std::uint64_t minimumSourceTimeMs);
    void reset() noexcept;

private:
    [[nodiscard]] bool openFor(const Entity& session);

    SessionService& sessions_;
    std::filesystem::path spoolRoot_;
    MediaProcessing::VideoLimits limits_;
    std::string sessionId_;
    std::uint32_t epoch_{};
    std::unique_ptr<MediaProcessing::OpenCvVideoClient> decoder_;
};
} // namespace RoadSign::RecognitionSession
