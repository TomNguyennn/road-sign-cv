#include "AudioControlsController.hpp"

#include <chrono>
#include <functional>
#include <string>
#include <utility>

namespace RoadSign::RecognitionSession {
namespace {
using namespace drogon;

HttpResponsePtr jsonResponse(Json::Value body, HttpStatusCode status) {
    auto response = HttpResponse::newHttpJsonResponse(std::move(body));
    response->setStatusCode(status);
    response->addHeader("Cache-Control", "no-store");
    return response;
}

HttpResponsePtr errorResponse(const char* code, const char* message, HttpStatusCode status) {
    Json::Value body;
    body["code"] = code;
    body["message"] = message;
    body["retryable"] = false;
    return jsonResponse(std::move(body), status);
}

bool owned(const HttpRequestPtr& request, const Entity& session,
           const std::string& id) {
    const auto& cookie = request->getCookie("rs_session");
    return !cookie.empty() && session.sessionId == id && session.cookieToken == cookie;
}
} // namespace

void registerAudioControlRoutes(HttpAppFramework& app, SessionService& sessions,
                                const Infrastructure::RuntimeConfig& config) {
    app.registerHandler("/api/v1/sessions/{1}/audio", [&sessions, &config](
        const HttpRequestPtr& request, std::function<void(const HttpResponsePtr&)>&& callback,
        const std::string& id) {
        const auto current = sessions.snapshot();
        if (!current || !owned(request, *current, id)) {
            callback(errorResponse("NOT_FOUND", "Session was not found", k404NotFound)); return;
        }
        const auto origin = request->getHeader("origin");
        const auto csrf = request->getHeader("x-csrf-token");
        if (origin != config.trustedOrigin || !sessions.authenticate(id, csrf, origin)) {
            callback(errorResponse("FORBIDDEN", "Request authentication failed", k403Forbidden)); return;
        }
        const auto body = request->getJsonObject();
        if (!body || !body->isObject() || !body->isMember("action") ||
            !(*body)["action"].isString() || !body->isMember("epoch") ||
            !(*body)["epoch"].isUInt()) {
            callback(errorResponse("INVALID_REQUEST", "Audio control body is invalid", k400BadRequest));
            return;
        }
        const auto action = (*body)["action"].asString();
        const bool recover = action == "recover";
        if ((recover && (body->size() != 3 || !body->isMember("descriptionId") ||
                        !(*body)["descriptionId"].isString() ||
                        (*body)["descriptionId"].asString().empty())) ||
            (!recover && body->size() != 2) ||
            (action != "mute" && action != "unmute" && action != "enable" &&
             action != "repeat" && !recover)) {
            callback(errorResponse("INVALID_REQUEST", "Audio control body is invalid", k400BadRequest));
            return;
        }
        const auto descriptionId = recover ? (*body)["descriptionId"].asString() : std::string{};
        const auto result = sessions.controlAudio(id, csrf, origin, (*body)["epoch"].asUInt(),
            action, recover ? std::optional<std::string_view>{descriptionId} : std::nullopt,
            std::chrono::steady_clock::now());
        if (!result) {
            callback(errorResponse("STATE_CONFLICT", "Audio control is stale", k409Conflict)); return;
        }
        if (result->status != AudioControlStatus::accepted) {
            callback(errorResponse("AUDIO_UNAVAILABLE", "Audio action is unavailable", k409Conflict));
            return;
        }
        const auto updated = sessions.snapshot();
        if (!updated || updated->sessionId != id) {
            callback(errorResponse("NOT_FOUND", "Session was not found", k404NotFound)); return;
        }
        Json::Value response;
        response["isMuted"] = updated->isMuted;
        response["isAudioBlocked"] = updated->isAudioBlocked;
        response["audioGeneration"] = updated->audioGeneration;
        response["stateRevision"] = Json::UInt64{updated->stateRevision};
        callback(jsonResponse(std::move(response), k200OK));
    }, {Post});

    app.registerHandler("/api/v1/sessions/{1}/cancellations/{2}/ack", [&sessions, &config](
        const HttpRequestPtr& request, std::function<void(const HttpResponsePtr&)>&& callback,
        const std::string& id, const std::string& cancellationId) {
        const auto current = sessions.snapshot();
        if (!current || !owned(request, *current, id)) {
            callback(errorResponse("NOT_FOUND", "Session was not found", k404NotFound)); return;
        }
        const auto origin = request->getHeader("origin");
        const auto csrf = request->getHeader("x-csrf-token");
        if (origin != config.trustedOrigin || !sessions.authenticate(id, csrf, origin)) {
            callback(errorResponse("FORBIDDEN", "Request authentication failed", k403Forbidden)); return;
        }
        const auto body = request->getJsonObject();
        if (!body || !body->isObject() || body->size() != 1 ||
            !body->isMember("cancelThroughAudioGeneration") ||
            !(*body)["cancelThroughAudioGeneration"].isUInt()) {
            callback(errorResponse("INVALID_REQUEST", "Cancellation body is invalid", k400BadRequest));
            return;
        }
        if (!sessions.acknowledgeAudioCancellation(id, csrf, origin, cancellationId,
                (*body)["cancelThroughAudioGeneration"].asUInt())) {
            callback(errorResponse("EXPIRED", "Cancellation receipt is unavailable", k410Gone));
            return;
        }
        auto response = HttpResponse::newHttpResponse();
        response->setStatusCode(k204NoContent);
        response->addHeader("Cache-Control", "no-store");
        callback(response);
    }, {Post});
}
} // namespace RoadSign::RecognitionSession
