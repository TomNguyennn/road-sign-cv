#pragma once

#include "../domain/TrackedSign.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace RoadSign::SignRecognition {
struct TrackAssignment {
    std::size_t observationIndex;
    std::string trackId;
};

class ByteTrackClient {
public:
    ByteTrackClient(std::string sessionId, std::uint32_t epoch);
    ~ByteTrackClient();
    ByteTrackClient(const ByteTrackClient&) = delete;
    ByteTrackClient& operator=(const ByteTrackClient&) = delete;

    [[nodiscard]] std::optional<std::vector<TrackAssignment>> update(
        const std::vector<SignObservation>& observations, double sourceTimeMs,
        ImageSize frameSize);
    [[nodiscard]] const TrackedSign* find(std::string_view trackId) const noexcept;
    [[nodiscard]] std::size_t trackCount() const noexcept;
    void reset(std::string sessionId, std::uint32_t epoch);

private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace RoadSign::SignRecognition
