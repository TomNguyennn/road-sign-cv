#pragma once

#include <json/json.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace RoadSign::RecognitionSession {
struct BufferedEvent {
    enum class Kind { frameResult, audioCancel } kind;
    std::string sessionId;
    std::uint32_t epoch;
    std::uint64_t stateRevision;
    Json::Value payload;
};

class SessionEventBuffer {
public:
    [[nodiscard]] bool publishFrameResult(BufferedEvent event);
    [[nodiscard]] bool publishAudioCancel(BufferedEvent event);
    [[nodiscard]] std::vector<BufferedEvent> drain(
        std::string_view sessionId, std::uint32_t epoch, std::uint64_t stateRevision);
    void clear(std::string_view sessionId);

private:
    [[nodiscard]] static bool isBounded(const BufferedEvent& event);

    std::mutex mutex_;
    std::optional<BufferedEvent> latestResult_;
    std::optional<BufferedEvent> cancellation_;
};
} // namespace RoadSign::RecognitionSession
