#pragma once

#include <cstdint>

namespace RoadSign {
struct WorkIdentity {
    std::uint64_t sessionId{};
    std::uint32_t epoch{};
    std::uint32_t audioGeneration{};

    [[nodiscard]] bool isValid() const noexcept {
        return sessionId != 0 && epoch != 0 && audioGeneration != 0;
    }
    friend bool operator==(const WorkIdentity&, const WorkIdentity&) = default;
};

class FrameSequence {
public:
    [[nodiscard]] bool accept(std::uint32_t epoch, std::uint64_t frameId) noexcept {
        if (epoch == 0 || epoch < epoch_ ||
            (epoch_ == epoch && hasFrame_ && frameId <= lastFrameId_))
            return false;
        if (epoch != epoch_) {
            epoch_ = epoch;
            hasFrame_ = false;
        }
        lastFrameId_ = frameId;
        hasFrame_ = true;
        return true;
    }

private:
    std::uint32_t epoch_{};
    std::uint64_t lastFrameId_{};
    bool hasFrame_{};
};
}
