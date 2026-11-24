#include "RuntimeConfig.hpp"

#include <json/json.h>

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

namespace RoadSign::Infrastructure {
namespace {
bool readBounded(const Json::Value& parent, const char* key, std::uint32_t hardLimit,
                 std::uint32_t& result) {
    if (!parent.isObject() || !parent.isMember(key) || !parent[key].isUInt()) return false;
    const auto value = parent[key].asUInt();
    if (value == 0 || value > hardLimit) return false;
    result = value;
    return true;
}
}

std::optional<std::uint32_t> RuntimeConfig::parseUploadAttempts(std::string_view value) {
    std::uint32_t result{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size() || result > 100)
        return std::nullopt;
    return result;
}

std::optional<bool> RuntimeConfig::parseBoolean(std::string_view value) {
    if (value == "true") return true;
    if (value == "false") return false;
    return std::nullopt;
}

std::optional<RuntimeConfig> RuntimeConfig::fromEnvironment() {
    RuntimeConfig config;
    const char* policyPath = std::getenv("POLICY_CONFIG_PATH");
    if (!policyPath) policyPath = "config/policy.json";
    std::ifstream input(policyPath);
    if (!input) return std::nullopt;
    const std::string contents{std::istreambuf_iterator<char>{input}, {}};
    const auto parsedPolicy = fromPolicyJson(contents);
    if (!parsedPolicy) return std::nullopt;
    config = *parsedPolicy;
    if (const char* raw = std::getenv("MAX_UPLOAD_ATTEMPTS")) {
        auto parsed = parseUploadAttempts(raw);
        if (!parsed) return std::nullopt;
        config.maxUploadAttempts = *parsed;
    }
    if (const char* raw = std::getenv("RUNTIME_METRICS_ENABLED")) {
        auto parsed = parseBoolean(raw);
        if (!parsed) return std::nullopt;
        config.metricsEnabled = *parsed;
    }
    if (const char* raw = std::getenv("TRUSTED_ORIGIN")) {
        config.trustedOrigin = raw;
        if (config.trustedOrigin.empty()) return std::nullopt;
    }
    return config;
}

std::optional<RuntimeConfig> RuntimeConfig::fromPolicyJson(std::string_view json) {
    Json::Value document;
    Json::CharReaderBuilder builder;
    std::string errors;
    const auto reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());
    if (!reader->parse(json.data(), json.data() + json.size(), &document, &errors) ||
        !document.isObject() || !document.isMember("policyVersion") ||
        document["policyVersion"].asString() != "1") return std::nullopt;

    RuntimeConfig config;
    const auto& camera = document["camera"];
    const auto& image = document["image"];
    const auto& video = document["video"];
    const auto& runtime = document["runtime"];
    if (!readBounded(camera, "maxFrameBytes", 1'048'576, config.maxFrameBytes) ||
        !readBounded(camera, "maxWidth", 1280, config.maxCameraWidth) ||
        !readBounded(camera, "maxHeight", 720, config.maxCameraHeight) ||
        !readBounded(camera, "maxFramesPerSecond", 5, config.maxFramesPerSecond) ||
        !readBounded(image, "maxBytes", 10'485'760, config.maxImageBytes) ||
        !readBounded(image, "maxDecodedPixels", 20'000'000, config.maxDecodedPixels) ||
        !readBounded(video, "maxBytes", 104'857'600, config.maxVideoBytes) ||
        !readBounded(video, "maxDurationSeconds", 300, config.maxVideoDurationSeconds) ||
        !readBounded(video, "maxWidth", 1920, config.maxVideoWidth) ||
        !readBounded(video, "maxHeight", 1080, config.maxVideoHeight) ||
        !readBounded(video, "maxFramesPerSecond", 60, config.maxVideoFramesPerSecond) ||
        !readBounded(runtime, "maxRequestBodyBytes", 105'906'176, config.maxRequestBodyBytes) ||
        !readBounded(runtime, "sessionTmpfsBytes", 268'435'456, config.sessionTmpfsBytes) ||
        !document.isMember("upload") || !document["upload"].isObject() ||
        !document["upload"].isMember("maxAttempts") ||
        !document["upload"]["maxAttempts"].isUInt() ||
        document["upload"]["maxAttempts"].asUInt() > 100 ||
        !runtime.isMember("metricsEnabled") || !runtime["metricsEnabled"].isBool())
        return std::nullopt;
    config.maxUploadAttempts = document["upload"]["maxAttempts"].asUInt();
    config.metricsEnabled = runtime["metricsEnabled"].asBool();
    return config;
}
} // namespace RoadSign::Infrastructure
