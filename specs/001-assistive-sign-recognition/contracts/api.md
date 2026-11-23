# HTTP and Event Contract v1

Normative design for the C++ server and TypeScript client. All endpoints below are under `/api/v1`.
JSON is UTF-8 with camelCase keys. Unknown fields are rejected. No account, database, WebSocket,
cloud service or resumable session protocol is introduced.

## Origin, ownership and errors

Serve UI/API from one HTTPS origin. `POST /sessions` requires an exact configured Origin and returns
an opaque 256-bit cookie (`Secure; HttpOnly; SameSite=Strict; Path=/api/v1`) plus a separate random
csrfToken in its JSON body. Subsequent endpoints require the cookie and matching sessionId; every
mutation also requires `X-CSRF-Token` and Origin. Neither secret is stored in browser storage or
URLs. One successful creation acquires the global active-session lock. Busy creation cannot
rotate another client's cookie. Read access by any other cookie returns 404 without metadata.

All session/result/audio responses have `Cache-Control: no-store`. Body/text/token logging is
forbidden. Error body: `{code: string, message: string, retryable: boolean}` with fixed codes.
Use 400 invalid request/sequence, 403 bad origin/CSRF, 404 not owned/not found, 409 busy/state conflict,
410 expired epoch/resource (only for authenticated owners), 413 size limit, 415 unsupported format,
422 invalid media/dimensions/duration, 429 frame capacity/rate, 503 model/runtime unavailable.
Readiness checks expose no filesystem paths, raw media, text or stack traces.

## Endpoints

| Method/path | Request | Success and behavior |
|-------------|---------|----------------------|
| GET /health | none | 200 `{status:"ok"}` process liveness |
| GET /ready | none | 200 `{status:"ready",modelId}` after bundle verified; otherwise 503 |
| GET /capabilities | none | 200 supported input types/limits, catalogue, policyVersion; safe public configuration |
| POST /sessions | `{sourceKind:"camera"\|"image"\|"video"}` | 201 `{sessionId,csrfToken,epoch:1,audioGeneration:1,stateRevision,eventsUrl,uploadAttemptsUsed:0,maxUploadAttempts}`; 409 BUSY if lease held |
| GET /sessions/{id}/events | cookie; Accept text/event-stream | 200 SSE; exactly one attachment; attach within 10 s; duplicate attach 409 |
| POST /sessions/{id}/heartbeat | `{epoch}` | 204, extends lease 15 s; requires owning cookie/CSRF |
| POST /sessions/{id}/frames | raw image/jpeg; headers `X-Epoch`, `X-Frame-Id`, `X-Source-Time-Ms`, `X-Capture-Time-Ms` | 202 `{frameId,epoch}`; camera only, max 1 MiB, <=1280x720 |
| POST /sessions/{id}/media | multipart field `file`; header `X-Epoch` | 202 `{epoch,uploadAttemptsUsed,maxUploadAttempts}` after format/limits validated; image/video only; at most one successfully accepted upload per session; failed attempts may retry within the attempt budget |
| POST /sessions/{id}/seek | `{epoch,positionMs}` | 200 `{epoch:newEpoch,audioGeneration,stateRevision}`; video only; clears old tracks/results/audio; bounds 0<=positionMs<duration |
| POST /sessions/{id}/audio | `{action:"mute"\|"unmute"\|"enable"\|"repeat"\|"recover",epoch,descriptionId?}` | 200 `{isMuted,isAudioBlocked,audioGeneration,stateRevision}`; repeat while muted/empty 409; recover requires the current descriptionId and an activated client audio context |
| GET /sessions/{id}/audio/{announcementId} | cookie | 200 audio/wav with `X-Audio-Valid-For-Ms` residual validity; stale/released 410 |
| POST /sessions/{id}/playback | `{announcementId,epoch,audioGeneration,status,clientTimeMs}` | 204; status started/ended/failed/blocked; stale results are ignored; cancellation acknowledgment uses its separate endpoint |
| POST /sessions/{id}/cancellations/{cancellationId}/ack | `{cancelThroughAudioGeneration}` plus owning cookie/CSRF | 204 for the matching barrier or its acknowledged duplicate; otherwise 410; no current-epoch/generation filter |
| DELETE /sessions/{id} | cookie/CSRF, no body | 202 `{state:"ending",stateRevision}`; immediately invalidates publication; client already stops capture/audio |

After cleanup the session endpoints return 404. DELETE of an already missing session returns 204
only when origin/CSRF can still be validated; otherwise the normal authentication error applies.
No resource must be resurrected to support idempotent cleanup. Clients treat 404 as already ended.

The client caps capture to 5 Hz, JPEG quality 0.8, preserving aspect ratio. It has one outstanding
frame POST. The server has one in-progress and one latest pending frame; replacing pending work
emits frame.dropped for the displaced ID. 429 tells the client to skip that frame, not retry old
frames. Frame IDs strictly increase per epoch; source and capture times must be finite,
nonnegative and monotonic. Header values are metadata, not paths or allocation sizes.

Upload limits: JPEG/PNG 10 MiB and 20 million decoded pixels; MP4/H.264 or WebM/VP8/VP9 100 MiB,
300 seconds, 1920x1080 and <=60 FPS. Reject zero-byte files, unknown/unsafe dimensions and
unsupported codecs even with a valid extension. Strip/ignore input audio tracks. Apply orientation
before detection/coordinate reporting. Multipart body including overhead <=101 MiB. Do not trust
supplied filenames; use generated tmpfs paths. Abort/cancel removes partial spools.

## SSE envelope and events

Send standard SSE `event: <type>` and `data: <JSON>`, with no replayable `id` field. Envelope:
`{sessionId,epoch,stateRevision,eventSeq,type,payload}`. eventSeq strictly increases within the session. Payloads:

| Type | Payload |
|------|---------|
| session.ready | `{state:"awaitingInput",audioGeneration}` |
| heartbeat | `{leaseRemainingMs}` every 5 s |
| session.state | `{state,audioGeneration,isMuted,isAudioBlocked,sourceTimeMs?,durationMs?}` |
| frame.result | `{frameId,sourceTimeMs,clientCaptureTimeMs?,width,height,observations:[...]}` |
| frame.dropped | `{frameId,reason:"superseded"}` |
| audio.ready | `{announcementId,trackId,revision,audioGeneration,text,isReplay,origin,audioUrl,durationMs,validForMs,frameId,clientCaptureTimeMs?}` |
| audio.cancel | `{cancellationId,cancelThroughAudioGeneration,audioGeneration,reason}`; cancel every artifact through the specified generation |
| description.latest | `{descriptionId,trackId,revision,text,acceptedAtMs}` for the current epoch; text is an accepted description, not raw OCR |
| session.error | `{code,message,retryable}`; no raw library exception |
| session.ended | `{reason:"stop"\|"disconnect"\|"leaseExpired"\|"failure"}` |

Each observation is `{trackId,category,confidence,box:{x,y,width,height},text:string|null,
isAccepted:boolean}`. Coordinates are upright original pixels; numbers are finite and bounded.
Never emit raw unvalidated OCR text to speech. UI may label unaccepted visual detections as uncertain.
JSON events <=64 KiB; max 100 observations; long text <=500 characters per accepted description.
Output overflow coalesces frame.result events to the latest frame; control/cancel/end events have
priority. If bounded output cannot drain, end the session instead of retaining unbounded data.

## Ordering and cancellation

Client waits for session.ready before input. The client and server send heartbeats every 5 s;
15 s without liveness ends the session. On EventSource error the client calls close(), cancels
capture/audio and requires explicit restart; built-in EventSource reconnect is not used.

Seek returns the incremented epoch; all subsequent messages use it. A frame/media request with an
old epoch cannot advance state. Source replacement is DELETE then a new POST after cleanup, never
reuse. On mute, the browser immediately stops audio and rejects locally old-generation promises;
the server increments audioGeneration and clears pending speech. Stop locally cancels before
waiting for DELETE, so a slow network cannot keep the user's speaker active.

Only one audio artifact may be offered/playing. Client fetches it, verifies session/epoch/generation
again after fetch/decode, and starts only while unmuted, connected and not expired. Report started
when AudioContext playback is scheduled, and ended on completion. Acknowledgments of cancellation
use the cancellation ID, never the playback-status endpoint. Do not start the next artifact
before prior completion/cancellation. Expired offered audio receives audio.cancel. A generation
change cancels pending fetches and decoded buffers as well as playback.

## Control reconciliation, expiry and upload admission

Session creation, seek and audio-control responses include stateRevision. Every accepted lifecycle
or audio-state mutation advances this revision on the session executor. Emit session.state with
new epoch/generation before any new-epoch result. The client accepts an authoritative session.state
from its current session even if its HTTP control reply has not arrived, reconciles by increasing
stateRevision, and ignores older control replies. Other events cannot advance epoch themselves.
Serialize seek/audio-control requests in the browser. Stop/mute still apply local cancellation
immediately; rapid user intents stay queued in order, and an earlier unmute response cannot clear
a later locally pending mute. Resume frame submission after seek confirmation, not speculative epoch.

The audio GET validates the artifact at response time and returns remaining offer lifetime in
X-Audio-Valid-For-Ms. The client starts a monotonic timer before the fetch and subtracts the entire
fetch/decode duration from that residual lifetime (conservative under network delay). Reject if
nonpositive. SSE validForMs is informational and never resets expiry on receipt. A late started
acknowledgment for an expired item triggers cancellation, not silent acceptance. On playback timeout,
send audio.cancel and await its matching cancellation acknowledgment before reusing the slot; end the session if
no acknowledgment arrives within 5 seconds. No missing acknowledgment implies playback stopped.

For /media only, Caddy performs a body-free forward-auth GET to the private backend path
/internal/admit-media before forwarding the original body. This validates original URI, cookie,
Origin and CSRF and atomically reserves the single global upload slot. Return an internal admission
nonce copied into the proxied request; strip client-supplied admission headers first. Reject a
second upload with 409 before it reaches Drogon's spool. The private route is never exposed by the
proxy and all business checks remain in C++. Limit body-read time to 60 seconds. Release admission
only after completion/abort cleanup; a failed upload does not consume the successful-upload slot; expire a stalled admission after 65 seconds and terminate its
session before allowing another upload. Backend checks the nonce and removes partial request files.
This is a resource admission guard, not another service or user-authentication system.

## Timing and evaluation

Browser monotonic capture time is echoed in results/audio. Compute live capture-to-result and
capture-to-playback latency within that same browser clock; do not subtract server monotonic time
from browser time. Backend stage durations are separate metrics. Playback scheduling measures a
software onset estimate, not acoustic perception; record browser output-latency estimates where
available. Uploaded-video source-time results are reported separately from live latency.

The server exposes no online metrics endpoint containing transcripts, images or histories. Offline
evaluation injects authorised fixtures into the same public pipeline interfaces and writes reports.

## Upload attempts and editable policy (I2)

`maxUploadAttempts` is the session snapshot of `MAX_UPLOAD_ATTEMPTS` from the server configuration.
Default **3**; allowed values are integers 0–100, with **0 meaning unlimited attempts**. This is the
explicit edit point: set `MAX_UPLOAD_ATTEMPTS` in `config/demo.env` (deployment environment override)
or change the default in `config/policy.json`. Configuration is validated at startup; existing
sessions keep their original limit. GET /capabilities and session creation expose the effective limit.

Increment uploadAttemptsUsed once when the pre-body admission gate grants a new upload. Authentication,
busy, stale-epoch, successful-slot-already-used and admission-rejection requests do not increment it.
An admitted upload that aborts or fails validation counts as an attempt; remove its partial files
and release admission, leaving awaitingInput so the next attempt is allowed while budget remains.
Only a completely received, validated file committed as MediaSource consumes the successful-upload
slot. Decoder/inference failures after that acceptance follow the processing-failure lifecycle.

Upload acceptance and rejection responses include uploadAttemptsUsed and maxUploadAttempts for
authenticated owners; error responses additionally use the standard error fields. A session with
one accepted file returns 409 UPLOAD_ALREADY_ACCEPTED on another /media request; change source by
ending it and creating a new session. At the finite attempt limit return 409 UPLOAD_ATTEMPTS_EXHAUSTED
before reading a new body. Explain that the user can end/start a session and try again; retain no
failed file. A failed upload must never silently become a successful upload or block the next valid
attempt solely because a previous request was sent. Do not automatically retry uploads on network
uncertainty: reconcile the session.state event first to avoid duplicate acceptance.

## Cancellation barriers (U1)

A cancellation is a distinct receipt, not a playback result. Before mute, seek or timeout fences
audio generation k, allocate a UUID cancellationId, record cancelThroughAudioGeneration=k, then
advance audioGeneration to k+1 (it never resets during a session, including seeks). Emit audio.cancel
for the current session. It invalidates every offered, fetched, decoded or playing artifact with
generation<=k, across old source epochs. Do not offer another artifact until the barrier is acknowledged.

The client processes audio.cancel for its current session before ordinary stale-epoch filtering.
It stops matching playback, aborts matching fetches, drops matching buffers, then POSTs to the
cancellation acknowledgment endpoint with the exact cancellationId and cancelThroughAudioGeneration.
The server looks up the outstanding receipt using authenticated session ownership; it MUST NOT reject
this acknowledgment merely because its target generation/epoch is older than current state. It cannot
mark any description spoken or restore playback. Ordinary started/ended/failed/blocked results still
require current session/epoch/generation/announcement matching and cannot satisfy this barrier.

Keep at most one pending cancellation receipt plus the last acknowledged receipt for idempotency.
If another control action must extend a pending fence, issue a fresh cancellationId with the higher
cutoff, supersede the old pending receipt and restart its five-second acknowledgment deadline. An
old receipt cannot clear the newer barrier; return 410 and let the newer SSE cancel be acknowledged.
Repeated acknowledgment of the latest completed receipt returns 204 without side effects. Missing
acknowledgment after five seconds ends the session. Stop/disconnect ends the session and needs no
acknowledgment to complete cleanup; it cannot offer new audio afterward.

## Explicit latest-result recovery (U2)

Retain a separate latestAcceptedDescription, even when its audio never started. Update it on accepted
information and emit description.latest; use acceptance order, selecting the highest-priority item
first for a batch and publishing that batch's highest-priority description as latest. It contains
only <=500 characters and a fresh descriptionId; clear it on seek/source replacement/session end.

After the user activates AudioContext, the labelled Read latest result control sends
`{action:"recover",epoch,descriptionId}`. Reject mute, missing data or a superseded descriptionId with
409 and a safe explanation. Recover clears the audio-blocked state and queues exactly that description
as one intentional item (`origin:"recovery", isReplay:true`); repeated clicks for the same pending
description coalesce. It may be read in reviewing state or after its track left view because the user
explicitly requested the last recognised result. It does not restore an automatic backlog.

Enable audio only enables future dispatch; it never silently replays old results. Repeat latest
continues to mean the last description whose playback started (`origin:"repeat", isReplay:true`).
Automatic items use `origin:"automatic", isReplay:false`. Recovery does not require a previous
started announcement. If a cancellation barrier is outstanding, the explicit request waits for its
acknowledgment before synthesis/offer; failure to clear the barrier ends the session as above.

## Visual fallback boundary (U3)

The API returns observation metadata, not preview-image bytes. The client can overlay locally retained
camera/image frames or locally decodable video at matching timestamps. If local video decoding or
synchronisation is unavailable, disable that video's overlay and provide text/audio review plus an
accessible explanation. No server preview-frame endpoint or sampled-frame fallback is part of v1.
