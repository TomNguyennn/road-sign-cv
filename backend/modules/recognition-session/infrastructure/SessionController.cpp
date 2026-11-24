#include "SessionController.hpp"

#include <drogon/HttpResponse.h>

#include <optional>
#include <string_view>

namespace RoadSign::RecognitionSession {
namespace {
using namespace drogon;

HttpResponsePtr jsonResponse(Json::Value body, HttpStatusCode status = k200OK) {
    auto response = HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    response->addHeader("Cache-Control", "no-store");
    return response;
}

std::optional<std::string> cookieValue(const HttpRequestPtr& request) {
    const auto& value = request->getCookie("rs_session");
    if (value.empty()) return std::nullopt;
    return value;
}

std::optional<SourceKind> parseSourceKind(std::string_view value) {
    if (value == "camera") return SourceKind::camera;
    if (value == "image") return SourceKind::image;
    if (value == "video") return SourceKind::video;
    return std::nullopt;
}

const char* uploadStatusName(UploadStatus status) {
    switch (status) {
    case UploadStatus::idle: return "idle";
    case UploadStatus::receiving: return "receiving";
    case UploadStatus::validating: return "validating";
    case UploadStatus::accepted: return "accepted";
    case UploadStatus::rejected: return "rejected";
    case UploadStatus::aborted: return "aborted";
    }
    return "idle";
}

Json::Value uploadSnapshot(const Entity& session) {
    Json::Value body;
    body["epoch"] = session.epoch;
    body["stateRevision"] = Json::UInt64{session.stateRevision};
    if (session.uploadReceipt.uploadAttemptId)
        body["uploadAttemptId"] = *session.uploadReceipt.uploadAttemptId;
    else
        body["uploadAttemptId"] = Json::nullValue;
    body["uploadStatus"] = uploadStatusName(session.uploadReceipt.status);
    if (session.uploadReceipt.errorCode)
        body["errorCode"] = *session.uploadReceipt.errorCode;
    else
        body["errorCode"] = Json::nullValue;
    body["uploadAttemptsUsed"] = session.uploadAttemptsUsed;
    body["maxUploadAttempts"] = session.maxUploadAttempts;
    body["hasAcceptedUpload"] = session.hasAcceptedUpload;
    body["canRetryUpload"] = session.canAdmitUpload();
    return body;
}

std::string serializeEvent(const Json::Value& envelope) {
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    return "event: " + envelope["type"].asString() + "\ndata: " +
           Json::writeString(writer, envelope) + "\n\n";
}
} // namespace

void registerSessionRoutes(HttpAppFramework& app, SessionService& sessions,
                           const Infrastructure::RuntimeConfig& config) {
    app.registerHandler("/api/v1/health", [](const HttpRequestPtr&,
        std::function<void(const HttpResponsePtr&)>&& callback) {
        Json::Value body; body["status"] = "ok"; callback(jsonResponse(std::move(body)));
    }, {Get});

    app.registerHandler("/api/v1/ready", [](const HttpRequestPtr&,
        std::function<void(const HttpResponsePtr&)>&& callback) {
        Json::Value body; body["code"] = "MODEL_UNAVAILABLE";
        body["message"] = "Recognition model is not available"; body["retryable"] = true;
        callback(jsonResponse(std::move(body), k503ServiceUnavailable));
    }, {Get});

    app.registerHandler("/api/v1/capabilities", [&config](const HttpRequestPtr&,
        std::function<void(const HttpResponsePtr&)>&& callback) {
        Json::Value body; body["supportedInputs"] = Json::arrayValue;
        body["supportedInputs"].append("camera"); body["supportedInputs"].append("image");
        body["supportedInputs"].append("video"); body["maxUploadAttempts"] = config.maxUploadAttempts;
        body["policyVersion"] = "1";
        body["limits"]["camera"]["maxWidth"] = config.maxCameraWidth;
        body["limits"]["camera"]["maxHeight"] = config.maxCameraHeight;
        body["limits"]["camera"]["maxFramesPerSecond"] = config.maxFramesPerSecond;
        body["limits"]["camera"]["maxFrameBytes"] = config.maxFrameBytes;
        body["limits"]["image"]["maxBytes"] = config.maxImageBytes;
        body["limits"]["image"]["maxDecodedPixels"] = config.maxDecodedPixels;
        body["limits"]["video"]["maxBytes"] = config.maxVideoBytes;
        body["limits"]["video"]["maxDurationSeconds"] = config.maxVideoDurationSeconds;
        body["limits"]["video"]["maxWidth"] = config.maxVideoWidth;
        body["limits"]["video"]["maxHeight"] = config.maxVideoHeight;
        body["limits"]["video"]["maxFramesPerSecond"] = config.maxVideoFramesPerSecond;
        callback(jsonResponse(std::move(body)));
    }, {Get});

    app.registerHandler("/api/v1/sessions", [&sessions, &config](const HttpRequestPtr& request,
        std::function<void(const HttpResponsePtr&)>&& callback) {
        auto json = request->getJsonObject();
        const auto origin = request->getHeader("origin");
        if (!json || json->size() != 1 || !(*json).isMember("sourceKind") ||
            !(*json)["sourceKind"].isString()) {
            Json::Value body; body["code"] = "INVALID_REQUEST";
            body["message"] = "Request body is invalid"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k400BadRequest)); return;
        }
        if (origin != config.trustedOrigin) {
            Json::Value body; body["code"] = "FORBIDDEN";
            body["message"] = "Request origin is not allowed"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k403Forbidden)); return;
        }
        const auto sourceKind = parseSourceKind((*json)["sourceKind"].asString());
        if (!sourceKind) {
            Json::Value body; body["code"] = "INVALID_REQUEST";
            body["message"] = "Source kind is not supported"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k400BadRequest)); return;
        }
        auto result = sessions.create(*sourceKind, origin, std::chrono::steady_clock::now());
        if (!result.session) {
            Json::Value body; body["code"] = "BUSY"; body["message"] = "A session is already active";
            body["retryable"] = true; callback(jsonResponse(std::move(body), k409Conflict)); return;
        }
        const auto& session = *result.session;
        auto responseBody = Json::Value{};
        responseBody["sessionId"] = session.sessionId; responseBody["csrfToken"] = session.csrfToken;
        responseBody["epoch"] = session.epoch; responseBody["audioGeneration"] = session.audioGeneration;
        responseBody["stateRevision"] = Json::UInt64{session.stateRevision};
        responseBody["eventsUrl"] = "/api/v1/sessions/" + session.sessionId + "/events";
        responseBody["uploadAttemptsUsed"] = 0;
        responseBody["maxUploadAttempts"] = session.maxUploadAttempts;
        auto response = jsonResponse(std::move(responseBody), k201Created);
        response->addHeader("Set-Cookie", "rs_session=" + session.sessionId +
            "; Path=/api/v1; Secure; HttpOnly; SameSite=Strict");
        callback(response);
    }, {Post});

    app.registerHandler("/api/v1/sessions/{1}", [&sessions, &config](const HttpRequestPtr& request,
        std::function<void(const HttpResponsePtr&)>&& callback, const std::string& id) {
        const auto csrf = request->getHeader("x-csrf-token");
        const auto origin = request->getHeader("origin");
        const auto cookie = cookieValue(request);
        if (origin != config.trustedOrigin) {
            Json::Value body; body["code"] = "FORBIDDEN";
            body["message"] = "Request authentication failed"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k403Forbidden)); return;
        }
        if (!cookie || *cookie != id) {
            Json::Value body; body["code"] = "NOT_FOUND";
            body["message"] = "Session was not found"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k404NotFound)); return;
        }
        const auto activeSession = sessions.snapshot();
        if (!activeSession || activeSession->sessionId != id) {
            Json::Value body; body["code"] = "NOT_FOUND";
            body["message"] = "Session was not found"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k404NotFound)); return;
        }
        if (request->method() == Delete) {
            if (csrf.empty() || !sessions.authenticate(id, csrf, origin)) {
                Json::Value body; body["code"] = "FORBIDDEN";
                body["message"] = "Request authentication failed"; body["retryable"] = false;
                callback(jsonResponse(std::move(body), k403Forbidden)); return;
            }
            const auto stateRevision = sessions.beginEnd(
                id, csrf, origin, std::chrono::steady_clock::now());
            if (!stateRevision) {
                Json::Value body; body["code"] = "STATE_CONFLICT";
                body["message"] = "Session is already ending"; body["retryable"] = false;
                callback(jsonResponse(std::move(body), k409Conflict)); return;
            }
            Json::Value body;
            body["state"] = "ending"; body["stateRevision"] = Json::UInt64{*stateRevision};
            if (!sessions.completeCleanup(id)) {
                Json::Value error; error["code"] = "STATE_CONFLICT";
                error["message"] = "Session cleanup did not complete"; error["retryable"] = true;
                callback(jsonResponse(std::move(error), k503ServiceUnavailable)); return;
            }
            auto response = jsonResponse(std::move(body), k202Accepted);
            response->addHeader("Set-Cookie", "rs_session=; Path=/api/v1; Secure; HttpOnly; SameSite=Strict; Max-Age=0");
            callback(response); return;
        }
        Json::Value body; body["code"] = "NOT_FOUND";
        body["message"] = "Route was not found"; body["retryable"] = false;
        callback(jsonResponse(std::move(body), k404NotFound));
    }, {Delete});

    app.registerHandler("/api/v1/sessions/{1}/heartbeat", [&sessions, &config](
        const HttpRequestPtr& request, std::function<void(const HttpResponsePtr&)>&& callback,
        const std::string& id) {
        const auto origin = request->getHeader("origin");
        const auto csrf = request->getHeader("x-csrf-token");
        if (origin != config.trustedOrigin) {
            Json::Value body; body["code"] = "FORBIDDEN";
            body["message"] = "Request origin is not allowed"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k403Forbidden)); return;
        }
        const auto cookie = cookieValue(request);
        const auto activeSession = sessions.snapshot();
        if (!cookie || *cookie != id || !activeSession || activeSession->sessionId != id) {
            Json::Value body; body["code"] = "NOT_FOUND";
            body["message"] = "Session was not found"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k404NotFound)); return;
        }
        auto json = request->getJsonObject();
        if (!json || json->size() != 1 || !(*json).isMember("epoch") ||
            !(*json)["epoch"].isUInt()) {
            Json::Value body; body["code"] = "INVALID_REQUEST";
            body["message"] = "Heartbeat body is invalid"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k400BadRequest)); return;
        }
        if (!sessions.heartbeat(id, csrf, origin, (*json)["epoch"].asUInt(),
                                std::chrono::steady_clock::now())) {
            Json::Value body; body["code"] = "FORBIDDEN";
            body["message"] = "Request authentication failed"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k403Forbidden)); return;
        }
        callback(jsonResponse(Json::Value{}, k204NoContent));
    }, {Post});

    app.registerHandler("/api/v1/sessions/{1}/upload-status", [&sessions, &config](
        const HttpRequestPtr& request, std::function<void(const HttpResponsePtr&)>&& callback,
        const std::string& id) {
        const auto origin = request->getHeader("origin");
        const auto session = sessions.snapshot();
        if (origin != config.trustedOrigin) {
            Json::Value body; body["code"] = "FORBIDDEN";
            body["message"] = "Request origin is not allowed"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k403Forbidden)); return;
        }
        const auto cookie = cookieValue(request);
        if (!cookie || *cookie != id || !session || session->sessionId != id) {
            Json::Value body; body["code"] = "NOT_FOUND";
            body["message"] = "Session was not found"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k404NotFound)); return;
        }
        if (session->sourceKind == SourceKind::camera) {
            Json::Value body; body["code"] = "STATE_CONFLICT";
            body["message"] = "Upload status is unavailable for camera sessions";
            body["retryable"] = false;
            callback(jsonResponse(std::move(body), k409Conflict)); return;
        }
        callback(jsonResponse(uploadSnapshot(*session)));
    }, {Get});

    app.registerHandler("/api/v1/sessions/{1}/events", [&sessions, &config](
        const HttpRequestPtr& request, std::function<void(const HttpResponsePtr&)>&& callback,
        const std::string& id) {
        const auto origin = request->getHeader("origin");
        if (origin != config.trustedOrigin) {
            Json::Value body; body["code"] = "FORBIDDEN";
            body["message"] = "Request origin is not allowed"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k403Forbidden)); return;
        }
        const auto session = sessions.snapshot();
        const auto cookie = cookieValue(request);
        if (!cookie || *cookie != id || !session || session->sessionId != id) {
            Json::Value body; body["code"] = "NOT_FOUND";
            body["message"] = "Session was not found"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k404NotFound)); return;
        }
        if (!sessions.attachEvents(id, std::chrono::steady_clock::now())) {
            Json::Value body; body["code"] = "STATE_CONFLICT";
            body["message"] = "Event stream cannot be attached"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k409Conflict)); return;
        }
        const auto attached = sessions.snapshot();
        if (!attached || attached->sessionId != id) {
            Json::Value body; body["code"] = "NOT_FOUND";
            body["message"] = "Session was not found"; body["retryable"] = false;
            callback(jsonResponse(std::move(body), k404NotFound)); return;
        }
        Json::Value envelope; envelope["sessionId"] = session->sessionId;
        envelope["epoch"] = attached->epoch;
        envelope["stateRevision"] = Json::UInt64{attached->stateRevision};
        envelope["eventSeq"] = Json::UInt64{1}; envelope["type"] = "session.ready";
        envelope["payload"]["state"] = "awaitingInput";
        envelope["payload"]["audioGeneration"] = attached->audioGeneration;
        auto readyEvent = serializeEvent(envelope);
        auto response = HttpResponse::newAsyncStreamResponse(
            [&sessions, id, csrf = attached->csrfToken, origin = config.trustedOrigin,
             readyEvent = std::move(readyEvent)](ResponseStreamPtr stream) {
                auto streamOwner = std::shared_ptr<ResponseStream>{std::move(stream)};
                if (!streamOwner->send(readyEvent)) {
                    streamOwner->close();
                    return;
                }
                auto eventSeq = std::make_shared<std::uint64_t>(1);
                auto timerTick = std::make_shared<std::uint32_t>(0);
                auto timerId = std::make_shared<trantor::TimerId>();
                auto* loop = drogon::app().getLoop();
                *timerId = loop->runEvery(1.0, [&sessions, id, csrf, origin, streamOwner,
                                                eventSeq, timerTick, timerId, loop] {
                    const auto current = sessions.snapshot();
                    if (!current || current->sessionId != id ||
                        current->state == State::ending || current->state == State::ended) {
                        streamOwner->close();
                        loop->invalidateTimer(*timerId);
                        return;
                    }
                    const auto now = std::chrono::steady_clock::now();
                    ++(*timerTick);
                    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                        current->heartbeatDeadline - now).count();
                    if (remaining > 0 && *timerTick % 5 != 0) return;
                    Json::Value heartbeat; heartbeat["sessionId"] = id;
                    heartbeat["epoch"] = current->epoch;
                    heartbeat["stateRevision"] = Json::UInt64{current->stateRevision};
                    heartbeat["eventSeq"] = Json::UInt64{++(*eventSeq)};
                    heartbeat["type"] = "heartbeat";
                    if (remaining <= 0) {
                        heartbeat["type"] = "session.ended";
                        heartbeat["payload"]["reason"] = "leaseExpired";
                    } else {
                        heartbeat["payload"]["leaseRemainingMs"] = Json::Int64{remaining};
                    }
                    const auto event = serializeEvent(heartbeat);
                    const bool sent = streamOwner->send(event);
                    if (remaining <= 0 || !sent) {
                        static_cast<void>(sessions.beginEnd(id, csrf, origin, now));
                        static_cast<void>(sessions.completeCleanup(id));
                        streamOwner->close();
                        loop->invalidateTimer(*timerId);
                    }
                });
            });
        response->setStatusCode(k200OK); response->addHeader("Cache-Control", "no-store");
        response->addHeader("Content-Type", "text/event-stream; charset=utf-8");
        callback(response);
    }, {Get});
}
} // namespace RoadSign::RecognitionSession
