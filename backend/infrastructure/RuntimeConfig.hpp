#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace RoadSign::Infrastructure {
struct RuntimeConfig {
    std::uint32_t maxUploadAttempts{3};
    bool metricsEnabled{};
    std::string trustedOrigin{"https://localhost"};
    std::uint32_t maxFrameBytes{1'048'576};
    std::uint32_t maxCameraWidth{1280};
    std::uint32_t maxCameraHeight{720};
    std::uint32_t maxFramesPerSecond{5};
    std::uint32_t maxImageBytes{10'485'760};
    std::uint32_t maxDecodedPixels{20'000'000};
    std::uint32_t maxVideoBytes{104'857'600};
    std::uint32_t maxVideoDurationSeconds{300};
    std::uint32_t maxVideoWidth{1920};
    std::uint32_t maxVideoHeight{1080};
    std::uint32_t maxVideoFramesPerSecond{60};
    std::uint32_t maxRequestBodyBytes{105'906'176};
    std::uint32_t spoolMemoryBytes{10'485'760};
    std::uint32_t sessionTmpfsBytes{268'435'456};

    [[nodiscard]] static std::optional<RuntimeConfig> fromEnvironment();
    [[nodiscard]] static std::optional<RuntimeConfig> fromPolicyJson(std::string_view json);
    [[nodiscard]] static std::optional<std::uint32_t> parseUploadAttempts(std::string_view value);
    [[nodiscard]] static std::optional<bool> parseBoolean(std::string_view value);
};
} // namespace RoadSign::Infrastructure
