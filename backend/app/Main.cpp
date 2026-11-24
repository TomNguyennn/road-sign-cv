#include "infrastructure/Logger.hpp"
#include "infrastructure/RuntimeConfig.hpp"
#include "modules/recognition-session/application/SessionService.hpp"
#include "modules/recognition-session/infrastructure/MediaAdmissionController.hpp"
#include "modules/recognition-session/infrastructure/SessionController.hpp"

#include <drogon/drogon.h>
#include <json/json.h>

#include <charconv>
#include <cstdlib>
#include <optional>
#include <string_view>
#include <utility>

namespace {
constexpr std::uint16_t kDefaultPort = 8080;
constexpr std::string_view kMaxRequestBodySize = "101M";
constexpr std::string_view kMaxInMemoryBodySize = "10M";

struct SessionShutdown {
    RoadSign::RecognitionSession::SessionService& sessions;
    ~SessionShutdown() { sessions.shutdown(); }
};

std::optional<std::uint16_t> parsePort(const char* value) {
    if (!value) return kDefaultPort;
    unsigned int port{};
    const std::string_view text{value};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), port);
    if (error != std::errc{} || end != text.data() + text.size() || port == 0 || port > 65535)
        return std::nullopt;
    return static_cast<std::uint16_t>(port);
}
}

int main() {
    using namespace RoadSign;
    const auto config = Infrastructure::RuntimeConfig::fromEnvironment();
    if (!config) {
        Infrastructure::logEvent(Infrastructure::Event::runtimeUnavailable);
        return EXIT_FAILURE;
    }
    RecognitionSession::SessionService sessions{config->maxUploadAttempts, config->trustedOrigin};
    const SessionShutdown sessionShutdown{sessions};
    Json::Value drogonConfig;
    drogonConfig["app"]["upload_path"] = "/run/road-sign";
    drogonConfig["app"]["client_max_body_size"] = kMaxRequestBodySize.data();
    drogonConfig["app"]["client_max_memory_body_size"] = kMaxInMemoryBodySize.data();
    drogon::app().loadConfigJson(std::move(drogonConfig));
    RecognitionSession::registerSessionRoutes(drogon::app(), sessions, *config);
    RecognitionSession::registerMediaAdmissionRoutes(drogon::app(), sessions, *config);
    const auto port = parsePort(std::getenv("PORT"));
    if (!port) {
        Infrastructure::logEvent(Infrastructure::Event::runtimeUnavailable);
        return EXIT_FAILURE;
    }
    drogon::app().addListener("0.0.0.0", *port).run();
    return EXIT_SUCCESS;
}
