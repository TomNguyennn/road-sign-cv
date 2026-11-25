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

bool fixedUInt(const Json::Value& parent, const char* key, Json::UInt expected) {
    return parent.isObject() && parent.isMember(key) && parent[key].isUInt() &&
           parent[key].asUInt() == expected;
}

bool fixedDouble(const Json::Value& parent, const char* key, double expected) {
    return parent.isObject() && parent.isMember(key) && parent[key].isDouble() &&
           parent[key].asDouble() == expected;
}

bool fixedText(const Json::Value& parent, const char* key, std::string_view expected) {
    return parent.isObject() && parent.isMember(key) && parent[key].isString() &&
           parent[key].asString() == expected;
}

bool fixedTrue(const Json::Value& parent, const char* key) {
    return parent.isObject() && parent.isMember(key) && parent[key].isBool() &&
           parent[key].asBool();
}

bool fixedRecognitionPolicy(const Json::Value& document) {
    const auto& camera = document["camera"];
    const auto& recognition = document["recognition"];
    const auto& tracking = document["tracking"];
    return fixedDouble(camera, "jpegQuality", 0.8) &&
        fixedUInt(recognition, "inputSize", 640) &&
        fixedDouble(recognition, "trackingScoreFloor", 0.10) &&
        fixedDouble(recognition, "nmsIou", 0.45) &&
        fixedDouble(recognition, "acceptedScore", 0.60) &&
        fixedUInt(recognition, "temporalRequired", 3) &&
        fixedUInt(recognition, "temporalWindow", 5) &&
        fixedUInt(recognition, "temporalWindowMs", 2000) &&
        fixedDouble(recognition, "stillScore", 0.75) &&
        fixedDouble(tracking, "highScore", 0.50) &&
        fixedDouble(tracking, "lowScore", 0.10) &&
        fixedDouble(tracking, "newTrackScore", 0.60) &&
        fixedDouble(tracking, "maxAssociationCost", 0.80) &&
        fixedUInt(tracking, "expiryMs", 2000) &&
        fixedUInt(tracking, "maxTracks", 100) && fixedTrue(tracking, "perClass");
}

bool fixedOcrPolicy(const Json::Value& document) {
    const auto& ocr = document["ocr"];
    return fixedUInt(ocr, "maxReadsPerSecond", 1) &&
        fixedUInt(ocr, "maxCropsPerFrame", 4) &&
        fixedUInt(ocr, "minWordConfidence", 70) &&
        fixedUInt(ocr, "consensusSamples", 3) &&
        fixedUInt(ocr, "consensusMatches", 2) &&
        fixedUInt(ocr, "consensusWindowMs", 2000) &&
        fixedUInt(ocr, "stillMaxCrops", 1) &&
        fixedUInt(ocr, "minCropSize", 16) &&
        fixedUInt(ocr, "maxCropEdge", 1600) &&
        fixedTrue(ocr, "unicodeNfkc") && fixedTrue(ocr, "caseFoldConsensus");
}

bool fixedSpeechAndLifecyclePolicy(const Json::Value& document) {
    const auto& speech = document["speech"];
    const auto& lifecycle = document["lifecycle"];
    constexpr Json::UInt priorities[] = {0, 0, 0, 0, 2, 1, 2, 0};
    if (!speech["categoryPriority"].isArray() ||
        speech["categoryPriority"].size() != std::size(priorities)) return false;
    for (Json::ArrayIndex index = 0; index < std::size(priorities); ++index)
        if (!speech["categoryPriority"][index].isUInt() ||
            speech["categoryPriority"][index].asUInt() != priorities[index]) return false;
    return fixedUInt(speech, "maxPending", 3) &&
        fixedUInt(speech, "maxPendingAgeMs", 5000) &&
        fixedUInt(speech, "maxTextCharacters", 500) &&
        fixedUInt(speech, "maxWavDurationMs", 45000) &&
        fixedUInt(speech, "maxHistory", 200) &&
        fixedText(speech, "queueOverflow", "dropLowestPriorityNewestTie") &&
        fixedUInt(lifecycle, "heartbeatMs", 5000) &&
        fixedUInt(lifecycle, "leaseMs", 15000) &&
        fixedUInt(lifecycle, "eventAttachMs", 10000) &&
        fixedText(lifecycle, "resourceFailure", "endSessionNoDiskSpill");
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
        !readBounded(runtime, "spoolMemoryBytes", 10'485'760, config.spoolMemoryBytes) ||
        !readBounded(runtime, "sessionTmpfsBytes", 268'435'456, config.sessionTmpfsBytes) ||
        !document.isMember("upload") || !document["upload"].isObject() ||
        !document["upload"].isMember("maxAttempts") ||
        !document["upload"]["maxAttempts"].isUInt() ||
        document["upload"]["maxAttempts"].asUInt() > 100 ||
        !runtime.isMember("metricsEnabled") || !runtime["metricsEnabled"].isBool())
        return std::nullopt;
    if (!fixedRecognitionPolicy(document) || !fixedOcrPolicy(document) ||
        !fixedSpeechAndLifecyclePolicy(document)) return std::nullopt;
    config.maxUploadAttempts = document["upload"]["maxAttempts"].asUInt();
    config.metricsEnabled = runtime["metricsEnabled"].asBool();
    return config;
}
} // namespace RoadSign::Infrastructure
