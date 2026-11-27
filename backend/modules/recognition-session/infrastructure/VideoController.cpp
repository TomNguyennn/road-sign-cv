#include "VideoController.hpp"

#include <drogon/HttpResponse.h>

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
} // namespace

void registerVideoRoutes(HttpAppFramework& app, SessionService& sessions,
                         const Infrastructure::RuntimeConfig& config) {
    app.registerHandler("/api/v1/sessions/{1}/seek", [&sessions, &config](
        const HttpRequestPtr& request, std::function<void(const HttpResponsePtr&)>&& callback,
        const std::string& id) {
        const auto current = sessions.snapshot();
        const auto cookie = request->getCookie("rs_session");
        if (cookie.empty() || !current || current->sessionId != id ||
            current->cookieToken != cookie) {
            callback(errorResponse("NOT_FOUND", "Session was not found", k404NotFound));
            return;
        }
        const auto origin = request->getHeader("origin");
        const auto csrf = request->getHeader("x-csrf-token");
        if (origin != config.trustedOrigin ||
            !sessions.authenticate(id, csrf, origin)) {
            callback(errorResponse("FORBIDDEN", "Request authentication failed", k403Forbidden));
            return;
        }
        const auto body = request->getJsonObject();
        if (!body || !body->isObject() || body->size() != 2 ||
            !body->isMember("epoch") || !(*body)["epoch"].isUInt() ||
            !body->isMember("positionMs") || !(*body)["positionMs"].isUInt64()) {
            callback(errorResponse("INVALID_REQUEST", "Seek body is invalid", k400BadRequest));
            return;
        }
        const auto seeked = sessions.seekVideo(id, csrf, origin,
            (*body)["epoch"].asUInt(), (*body)["positionMs"].asUInt64());
        if (!seeked) {
            callback(errorResponse("STATE_CONFLICT", "Seek is stale or outside the video",
                                   k409Conflict));
            return;
        }
        Json::Value response;
        response["epoch"] = seeked->epoch;
        response["audioGeneration"] = seeked->audioGeneration;
        response["stateRevision"] = Json::UInt64{seeked->stateRevision};
        callback(jsonResponse(std::move(response), k200OK));
    }, {Post});
}
} // namespace RoadSign::RecognitionSession
