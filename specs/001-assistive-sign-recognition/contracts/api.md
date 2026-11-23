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
| POST /sessions | `{sourceKind:"camera"\|"image"\|"video"}` | 201 `{sessionId,csrfToken,epoch:1,audioGeneration:1,eventsUrl}`; 409 BUSY if lease held |
| GET /sessions/{id}/events | cookie; Accept text/event-stream | 200 SSE; exactly one attachment; attach within 10 s; duplicate attach 409 |
| POST /sessions/{id}/heartbeat | `{epoch}` | 204, extends lease 15 s; requires owning cookie/CSRF |
| POST /sessions/{id}/frames | raw image/jpeg; headers `X-Epoch`, `X-Frame-Id`, `X-Source-Time-Ms`, `X-Capture-Time-Ms` | 202 `{frameId,epoch}`; camera only, max 1 MiB, <=1280x720 |
| POST /sessions/{id}/media | multipart field `file`; header `X-Epoch` | 202 `{epoch}` after format/limits validated; image/video only, once per session |
| POST /sessions/{id}/seek | `{epoch,positionMs}` | 200 `{epoch:newEpoch,audioGeneration}`; video only; clears old tracks/results/audio; bounds 0<=positionMs<duration |
| POST /sessions/{id}/audio | `{action:"mute"\|"unmute"\|"enable"\|"repeat",epoch}` | 200 `{isMuted,isAudioBlocked,audioGeneration}`; repeat while muted/empty 409 |
| GET /sessions/{id}/audio/{announcementId} | cookie | 200 audio/wav with `X-Audio-Valid-For-Ms` residual validity; stale/released 410 |
| POST /sessions/{id}/playback | `{announcementId,epoch,audioGeneration,status,clientTimeMs}` | 204; status started/ended/cancelled/failed/blocked; never trust as proof of audible perception |
| DELETE /sessions/{id} | cookie/CSRF, no body | 202 `{state:"ending"}`; immediately invalidates publication; client already stops capture/audio |

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
| audio.ready | `{announcementId,trackId,revision,audioGeneration,text,isReplay,audioUrl,durationMs,validForMs,frameId,clientCaptureTimeMs?}` |
| audio.cancel | `{announcementId?,audioGeneration,reason}`; absent announcementId cancels all |
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
when AudioContext playback is scheduled, and ended on completion. Do not start the next artifact
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
send audio.cancel and await cancelled acknowledgment before reusing the slot; end the session if
no acknowledgment arrives within 5 seconds. No missing acknowledgment implies playback stopped.

For /media only, Caddy performs a body-free forward-auth GET to the private backend path
/internal/admit-media before forwarding the original body. This validates original URI, cookie,
Origin and CSRF and atomically reserves the single global upload slot. Return an internal admission
nonce copied into the proxied request; strip client-supplied admission headers first. Reject a
second upload with 409 before it reaches Drogon's spool. The private route is never exposed by the
proxy and all business checks remain in C++. Limit body-read time to 60 seconds. Release admission
only after completion/abort cleanup; expire a stalled admission after 65 seconds and terminate its
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
