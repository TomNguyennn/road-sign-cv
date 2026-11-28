#include "SessionEventsController.hpp"

#include <drogon/HttpResponse.h>

#include <memory>
#include <string>

namespace RoadSign::RecognitionSession {
namespace {
using namespace drogon;
constexpr std::size_t kMaxEventBytes = 64U * 1024U;

HttpResponsePtr jsonResponse(Json::Value body, HttpStatusCode status) {
    auto response = HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    response->addHeader("Cache-Control", "no-store");
    return response;
}

std::string stateName(State state) {
    switch (state) {
    case State::created: return "created";
    case State::awaitingInput: return "awaitingInput";
    case State::processing: return "processing";
    case State::reviewing: return "reviewing";
    case State::ending: return "ending";
    case State::ended: return "ended";
    }
    return "ended";
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

Json::Value uploadPayload(const Entity& session) {
    Json::Value body;
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

bool sendEvent(const std::shared_ptr<ResponseStream>& stream, const std::string& sessionId,
               const Entity& session, std::uint64_t& eventSeq, const char* type,
               Json::Value payload = {}) {
    Json::Value envelope;
    envelope["sessionId"] = sessionId;
    envelope["epoch"] = session.epoch;
    envelope["stateRevision"] = Json::UInt64{session.stateRevision};
    envelope["eventSeq"] = Json::UInt64{++eventSeq};
    envelope["type"] = type;
    envelope["payload"] = std::move(payload);
    const auto serialized = serializeEvent(envelope);
    return serialized.size() <= kMaxEventBytes && stream->send(serialized);
}

} // namespace

void registerSessionEventRoutes(HttpAppFramework& app, SessionService& sessions,
                                SessionEventBuffer& events,
                                const Infrastructure::RuntimeConfig& config) {
    app.registerHandler("/api/v1/sessions/{1}/events", [&sessions, &events, &config](
        const HttpRequestPtr& request, std::function<void(const HttpResponsePtr&)>&& callback,
        const std::string& id) {
        const auto session = sessions.snapshot();
        const auto& cookie = request->getCookie("rs_session");
        if (cookie.empty() || !session || session->sessionId != id ||
            cookie != session->cookieToken) {
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
        Json::Value readyPayload;
        readyPayload["state"] = stateName(attached->state);
        readyPayload["audioGeneration"] = attached->audioGeneration;
        auto response = HttpResponse::newAsyncStreamResponse(
            [&sessions, &events, id, csrf = attached->csrfToken, origin = config.trustedOrigin,
             readyPayload = std::move(readyPayload), attached = *attached](ResponseStreamPtr stream) {
                auto streamOwner = std::shared_ptr<ResponseStream>{std::move(stream)};
                auto eventSeq = std::make_shared<std::uint64_t>();
                if (!sendEvent(streamOwner, id, attached, *eventSeq, "session.ready",
                               std::move(readyPayload))) {
                    streamOwner->close();
                    return;
                }
                auto previous = std::make_shared<Entity>(attached);
                auto timerTick = std::make_shared<std::uint32_t>(0);
                auto timerId = std::make_shared<trantor::TimerId>();
                auto* loop = trantor::EventLoop::getEventLoopOfCurrentThread();
                if (!loop) {
                    streamOwner->close();
                    return;
                }
                *timerId = loop->runEvery(1.0, [&sessions, &events, id, csrf, origin, streamOwner,
                                                previous, eventSeq, timerTick, timerId, loop] {
                    const auto now = std::chrono::steady_clock::now();
                    const auto expiredAdmission = sessions.expireUploadAdmission(now);
                    if (expiredAdmission && *expiredAdmission == id) {
                        const auto expired = sessions.snapshot();
                        if (expired && expired->sessionId == id) {
                            Json::Value state; state["state"] = stateName(expired->state);
                            state["audioGeneration"] = expired->audioGeneration;
                            state["isMuted"] = expired->isMuted;
                            state["isAudioBlocked"] = false;
                            static_cast<void>(sendEvent(streamOwner, id, *expired, *eventSeq,
                                                        "session.state", std::move(state)));
                            static_cast<void>(sendEvent(streamOwner, id, *expired, *eventSeq,
                                                        "upload.state", uploadPayload(*expired)));
                            Json::Value ended; ended["reason"] = "failure";
                            static_cast<void>(sendEvent(streamOwner, id, *expired, *eventSeq,
                                                        "session.ended", std::move(ended)));
                        }
                        static_cast<void>(sessions.completeCleanup(id));
                        streamOwner->close();
                        loop->invalidateTimer(*timerId);
                        return;
                    }
                    sessions.expireAudioCancellation(now);
                    const auto current = sessions.snapshot();
                    if (!current || current->sessionId != id) {
                        streamOwner->close();
                        loop->invalidateTimer(*timerId);
                        return;
                    }
                    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                        current->heartbeatDeadline - now).count();
                    if (remaining <= 0) {
                        events.clear(id);
                        Json::Value ended; ended["reason"] = "leaseExpired";
                        static_cast<void>(sendEvent(streamOwner, id, *current, *eventSeq,
                                                    "session.ended", std::move(ended)));
                        static_cast<void>(sessions.beginEnd(id, csrf, origin, now));
                        static_cast<void>(sessions.completeCleanup(id));
                        streamOwner->close();
                        loop->invalidateTimer(*timerId);
                        return;
                    }
                    if (current->state == State::ending || current->state == State::ended) {
                        events.clear(id);
                        Json::Value ended; ended["reason"] = "stop";
                        static_cast<void>(sendEvent(streamOwner, id, *current, *eventSeq,
                                                    "session.ended", std::move(ended)));
                        static_cast<void>(sessions.completeCleanup(id));
                        streamOwner->close();
                        loop->invalidateTimer(*timerId);
                        return;
                    }
                    for (const auto& upload : sessions.drainUploadEvents(id)) {
                        Entity eventState = *current;
                        eventState.epoch = upload.epoch;
                        eventState.stateRevision = upload.stateRevision;
                        eventState.uploadReceipt = upload.receipt;
                        eventState.uploadAttemptsUsed = upload.uploadAttemptsUsed;
                        eventState.maxUploadAttempts = upload.maxUploadAttempts;
                        eventState.hasAcceptedUpload = upload.hasAcceptedUpload;
                        auto payload = uploadPayload(eventState);
                        payload["canRetryUpload"] = upload.canRetryUpload;
                        if (!sendEvent(streamOwner, id, eventState, *eventSeq,
                                       "upload.state", std::move(payload))) {
                            static_cast<void>(sessions.beginEnd(id, csrf, origin, now));
                            static_cast<void>(sessions.completeCleanup(id));
                            streamOwner->close();
                            loop->invalidateTimer(*timerId);
                            return;
                        }
                    }
                    const bool hasVideoProgress = current->sourceKind == SourceKind::video &&
                        current->acceptedMedia && previous->acceptedMedia &&
                        current->acceptedMedia->positionMs != previous->acceptedMedia->positionMs;
                    const bool stateChanged = hasVideoProgress ||
                        (current->stateRevision != previous->stateRevision &&
                         (current->epoch != previous->epoch || current->state != previous->state ||
                          current->audioGeneration != previous->audioGeneration ||
                          current->isMuted != previous->isMuted ||
                          current->isAudioBlocked != previous->isAudioBlocked));
                    if (stateChanged) {
                        Json::Value payload; payload["state"] = stateName(current->state);
                        payload["audioGeneration"] = current->audioGeneration;
                        payload["isMuted"] = current->isMuted;
                        payload["isAudioBlocked"] = current->isAudioBlocked;
                        if (current->sourceKind == SourceKind::video && current->acceptedMedia) {
                            if (current->acceptedMedia->positionMs)
                                payload["sourceTimeMs"] =
                                    Json::UInt64{*current->acceptedMedia->positionMs};
                            if (current->acceptedMedia->durationMs)
                                payload["durationMs"] =
                                    Json::UInt64{*current->acceptedMedia->durationMs};
                        }
                        if (!sendEvent(streamOwner, id, *current, *eventSeq,
                                       "session.state", std::move(payload))) {
                            static_cast<void>(sessions.beginEnd(id, csrf, origin, now));
                            static_cast<void>(sessions.completeCleanup(id));
                            streamOwner->close();
                            loop->invalidateTimer(*timerId);
                            return;
                        }
                    }
                    if (current->audioGeneration != previous->audioGeneration) {
                        const auto cancellation = sessions.pendingAudioCancellation(id);
                        if (cancellation) {
                            Json::Value payload;
                            payload["cancellationId"] = cancellation->cancellationId;
                            payload["cancelThroughAudioGeneration"] =
                                cancellation->cancelThroughAudioGeneration;
                            payload["audioGeneration"] = current->audioGeneration;
                            payload["reason"] = "mute";
                            if (!sendEvent(streamOwner, id, *current, *eventSeq,
                                           "audio.cancel", std::move(payload))) {
                                static_cast<void>(sessions.beginEnd(id, csrf, origin, now));
                                static_cast<void>(sessions.completeCleanup(id));
                                streamOwner->close();
                                loop->invalidateTimer(*timerId);
                                return;
                            }
                        }
                    }
                    for (auto& event : events.drain(id, current->epoch, current->stateRevision)) {
                        Entity eventState = *current;
                        eventState.epoch = event.epoch;
                        eventState.stateRevision = event.stateRevision;
                        const char* type = event.kind == BufferedEvent::Kind::audioCancel
                            ? "audio.cancel" : "frame.result";
                        if (!sendEvent(streamOwner, id, eventState, *eventSeq,
                                       type, std::move(event.payload))) {
                            static_cast<void>(sessions.beginEnd(id, csrf, origin, now));
                            static_cast<void>(sessions.completeCleanup(id));
                            streamOwner->close();
                            loop->invalidateTimer(*timerId);
                            return;
                        }
                    }
                    *previous = *current;
                    ++(*timerTick);
                    if (*timerTick % 5 != 0) return;
                    Json::Value heartbeat;
                    heartbeat["leaseRemainingMs"] = Json::Int64{remaining};
                    if (!sendEvent(streamOwner, id, *current, *eventSeq,
                                   "heartbeat", std::move(heartbeat))) {
                        static_cast<void>(sessions.beginEnd(id, csrf, origin, now));
                        static_cast<void>(sessions.completeCleanup(id));
                        streamOwner->close();
                        loop->invalidateTimer(*timerId);
                    }
                });
            });
        response->setStatusCode(k200OK);
        response->addHeader("Cache-Control", "no-store");
        response->addHeader("Content-Type", "text/event-stream; charset=utf-8");
        callback(response);
    }, {Get});
}
} // namespace RoadSign::RecognitionSession
