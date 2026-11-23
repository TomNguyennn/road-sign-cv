# Research Decisions

**Date**: 2026-11-23 | **Feature**: [spec.md](spec.md)

Research resolves the design questions below. It does not claim installed dependencies, available
training data, measured performance, or validated device compatibility. Candidate release families
are fixed in plan.md; immutable patches/hashes must be recorded during implementation setup.

## Native inference and training handoff

**Decision**: Fine-tune YOLO11n on the eight catalogue classes; explicitly export the trained model
to static FP32 ONNX, batch 1, 640x640, opset 17, without embedded NMS. Use ONNX Runtime CPU in C++.

**Rationale**: Keeps Python in offline training while allowing the same weights to be exercised by
native runtime tests. YOLO11 is explicitly documented; default examples may refer to newer models,
so model selection must not depend on exporter defaults. Startup validates manifest and tensors;
Python/C++ parity is a release prerequisite, not an assumption.

**Alternatives considered**: OpenCV DNN offers fewer runtime dependencies but a different operator
compatibility surface; LibTorch couples deployment more tightly to training. GPU providers are
unnecessary before the CPU baseline is measured. No alternate runtime is selected implicitly if
export validation fails.

Sources: [YOLO11](https://docs.ultralytics.com/models/yolo11/),
[export](https://docs.ultralytics.com/modes/export/),
[ORT C++](https://onnxruntime.ai/docs/get-started/with-cpp.html),
[ORT compatibility](https://onnxruntime.ai/docs/reference/compatibility.html).

## Media, tracking and OCR

**Decision**: OpenCV 4.x decodes images/video and prepares owned frames/crops. Adapt tracker-only
ByteTrack C++/Eigen code from a pinned upstream commit, one tracker per category; do not include
its ncnn inference sample. Replace frame-count expiry with source-time expiry and bound removed
track history. Use Tesseract 5.x English tessdata_fast with a worker-owned API instance.

**Rationale**: OpenCV buffer views can share storage and must not outlive/reuse their backing frame.
The inspected ByteTrack example ignores labels, ages by frame count and retains removed tracks;
these require explicit adaptation for a variable-rate, bounded-memory pipeline. OCR starts from
native sign crops: street names use single-line segmentation, other text-bearing signs sparse text;
deskew, upscale small crops and add borders. OCR confidence alone is not certainty.

**Alternatives considered**: Writing a new tracker adds algorithm maintenance; invoking Python
tracking violates runtime responsibilities. Neural OCR may improve difficult scenes, but introduces
another model contract before the native baseline is measured.

Sources: [OpenCV Mat ownership](https://docs.opencv.org/4.x/d3/d63/classcv_1_1Mat.html),
[ByteTrack C++](https://github.com/FoundationVision/ByteTrack/tree/main/deploy/ncnn/cpp),
[inspected tracker](https://raw.githubusercontent.com/FoundationVision/ByteTrack/main/deploy/ncnn/cpp/src/BYTETracker.cpp),
[Tesseract API](https://tesseract-ocr.github.io/tessdoc/APIExample.html),
[OCR preparation](https://tesseract-ocr.github.io/tessdoc/ImproveQuality.html).

## Speech and accessible interaction

**Decision**: eSpeak NG C API in retrieval mode, invoked by a serialized C++ worker, produces PCM/WAV
for the browser. Default voice en-gb, 160 words/minute; speech clarity is evaluated, not presumed.
One browser AudioContext plays one generated buffer at a time. Start/Enable audio explicitly resumes
the context; failed activation blocks automatic playback and exposes an accessible recovery button.
Use native HTML controls, a separate polite status region, and non-live recognition transcripts.

**Rationale**: Keeps TTS in C++ and avoids server audio-device assumptions. User activation and
screen-reader status handling require explicit UI behavior; browser speech synthesis would move
TTS out of the required runtime. General status is not duplicated as application TTS.

**Alternatives considered**: Browser speech violates the chosen boundary; neural TTS adds model
and compute requirements. A component library is unnecessary for the small control surface.

Sources: [eSpeak API](https://raw.githubusercontent.com/espeak-ng/espeak-ng/master/src/include/espeak-ng/speak_lib.h),
[Web Audio autoplay](https://developer.mozilla.org/en-US/docs/Web/Media/Guides/Autoplay),
[W3C status messages](https://www.w3.org/WAI/WCAG22/Understanding/status-messages.html).

## Transport and lifecycle

**Decision**: Drogon REST commands/uploads and server-sent events (SSE), behind same-origin Caddy.
Native EventSource receives results and audio instructions. Session cookie plus in-memory CSRF
secret protects ownership and mutations. Input uses bounded JPEG HTTP posts; one C++ vision worker
and one speech worker leave network/control processing responsive. Every item carries session,
source epoch and audio generation where applicable.

**Rationale**: A single-user sampled camera does not require WebRTC negotiation or a binary
WebSocket protocol. SSE reconnection must be disabled by closing EventSource on error; there is
no replay/resume contract. Client/server heartbeats every 5 seconds expire the session after
15 seconds without liveness. Stop and errors cancel local media immediately; server cleanup
follows detectable disconnect or lease expiry.

**Alternatives considered**: Polling increases state coordination, WebSocket adds protocol surface,
and unbounded frame streaming creates stale results. See contracts/api.md for exact messages.

Sources: [Drogon threading](https://github.com/drogonframework/drogon/wiki/ENG-FAQ-1-Understanding-drogon-threading-model),
[EventSource](https://developer.mozilla.org/en-US/docs/Web/API/EventSource).

## Temporary storage and phone deployment

**Decision**: Caddy serves the built frontend and proxies a private backend. Its internal CA is
trusted explicitly on test phones/computers. Backend is read-only except a 256 MiB tmpfs used for
Drogon request spools and decoder files. Set request memory threshold 10 MiB, total body cap
101 MiB including multipart overhead; reject excess at proxy and application. Limit video to
100 MiB. A Caddy pre-body admission check reserves one global upload slot through the
same backend; a second upload is rejected before spooling. Disable core dumps, request bodies and transcript logging; use no-store responses.

**Rationale**: Phone camera access needs a secure context. Drogon's large-body caching otherwise
can create persistent files. tmpfs can swap; strict no-durable-media validation requires disabled
swap on the Linux host/VM and no media-bearing crash snapshots. Persist only Caddy CA state and
explicit offline evaluation outputs. Do not claim tmpfs alone proves no disk persistence.

**Alternatives considered**: Plain LAN HTTP fails camera requirements; writing uploaded media to
normal container layers violates session-only storage. Entirely in-memory video decoding would
require a more complex decoder interface than OpenCV's file-backed path.

Sources: [getUserMedia](https://developer.mozilla.org/en-US/docs/Web/API/MediaDevices/getUserMedia),
[Caddy HTTPS](https://caddyserver.com/docs/automatic-https),
[Drogon body limits](https://github.com/drogonframework/drogon/wiki/ENG-11-Configuration-File),
[Docker tmpfs](https://docs.docker.com/engine/storage/tmpfs/),
[Vite build tooling](https://vite.dev/guide/),
[Caddy pre-body authorization](https://caddyserver.com/docs/caddyfile/directives/forward_auth).

## Catalogue, data and evaluation

**Decision**: Create an authorised project dataset with real UK scenes and the eight documented
categories. Record provenance, permission, source/location groups, boxes, transcripts, track IDs,
and condition tags. Use deterministic grouped 70/15/15 train/validation/test assignment (seed 42),
stratifying category coverage without splitting source/location groups. Small/imbalanced data
requires additional collection, not moving near-duplicates across splits. Augmentation happens
only after splitting and only in training.

**Rationale**: No existing dataset in this repo establishes coverage of all eight categories.
Official DfT artwork helps catalogue reference and labelled synthetic fixtures, not real-world
OCR/tracking accuracy. Training includes negatives and out-of-catalogue examples; distinguish
synthetic policy fixtures from held-out real scenes in every report.

**Alternatives considered**: Mapillary is a possible supplementary source, not an assumed solution;
its terms/taxonomy must be checked before import. Ordinary demo sessions cannot become training data.

Sources: [DfT traffic-sign artwork](https://www.gov.uk/guidance/traffic-sign-images),
[Mapillary dataset information](https://ai.meta.com/ai-for-good/datasets/mapillary-training-datasets/).

## Dependency provenance

**Decision**: Inventory code, model and dataset licences in the dependency/model manifests and
preserve upstream notices. Do not change the repository licence in this workflow or assume ONNX
conversion removes upstream terms. The chosen eSpeak NG and Ultralytics components have copyleft
or commercial licensing terms that must be reflected in any subsequent distribution decision.

**Rationale**: These are concrete dependencies in the proposed stack; the controlled prototype
plan is not a declaration that third-party code or weights are permissively licensed.

**Alternatives considered**: Replacing TTS or training packages is an explicit design amendment
if later distribution needs are incompatible, not an undocumented implementation substitution.

Sources: [eSpeak NG](https://github.com/espeak-ng/espeak-ng),
[Ultralytics licensing](https://www.ultralytics.com/license),
[ByteTrack](https://github.com/FoundationVision/ByteTrack).

## Fixed starting policies

These are project-selected initial settings, not library defaults or measured quality guarantees.
They must be versioned and changed only using training/validation data, never the held-out test set.

| Policy | Initial decision |
|--------|------------------|
| Camera | JPEG quality 0.8, aspect-preserving max 1280x720, <=5 Hz, max 1 MiB/frame |
| Upload attempts | MAX_UPLOAD_ATTEMPTS default3; integer0–100, 0 unlimited; one accepted upload/session; rejected attempts retry within budget |
| Images | JPEG/PNG only, <=10 MiB, <=20 million decoded pixels, upright orientation |
| Video | MP4/H.264 or WebM/VP8/VP9, <=100 MiB, <=300 s, <=1920x1080, <=60 source FPS |
| Detection | FP32 640 input; keep scores >=0.10 for tracking; class-aware NMS IoU 0.45 |
| Accepted sign | score >=0.60 and same category in 3 of last 5 observations within 2 s |
| Still image | single observation score >=0.75; no fictitious temporal consensus |
| Tracker | high/low scores 0.50/0.10, new track >=0.60, association cost <=0.80; expiry 2 source seconds |
| Track safety | per-class association, session-composite IDs; max 100 active/lost tracks total; reject excess new candidates by score |
| OCR | per text-bearing track <=1 Hz, max 4 crops/frame, mean valid word confidence >=70/100 |
| Text consensus | separate last three OCR samples within 2 s; Unicode NFKC, trim/collapse whitespace; same case-folded result twice within 2 s; preserve display spelling |
| Still OCR | one accepted crop, same confidence threshold; do not fabricate text if rejected |
| Crop bounds | minimum 16x16; resize processed crop longest edge <=1600; preserve aspect ratio |
| Speech queue | one offered/playing + at most 3 pending; max pending age 5 wall-clock seconds |
| Priority | warnings/restrictions 0, directions 1, general information 2; oldest first within tier |
| Queue overflow | drop lowest priority, newest on ties; never displace an older equal-priority item |
| Speech payload | <=500 Unicode characters, phrase-boundary truncation marked as partial; <=45 s WAV |
| Lifecycle | heartbeats 5 s, lease 15 s, initial event attachment timeout 10 s |
| History | latest 200 announcement records; announce history truncation status once; retain latest repeat text |
| Resource failure | stop and clear session on decoder/inference allocation failure; no silent spill to disk |

Acceptance timestamps use source time for tracking (video media time or monotonic capture time),
wall time for queue freshness, and client monotonic time for capture-to-playback measurements.
Class 0–3 warnings/restrictions have priority 0; class 5 directions priority 1; classes 4/6 priority 2.
Class 7 temporary pedestrian notices use priority 0 as a conservative category-level setting,
without inferring prohibitions or route instructions from arbitrary text.

## Review corrections: decisions and alternatives

**Decision**: Explicit stateRevision responses, configurable admitted-upload attempts, separate
cancellation receipts, latest-accepted-result recovery, split image/camera acceptance, lock-dependent
provenance capture, and text/audio-only video fallback when local rendering is unavailable.

**Rationale**: These resolve I1/I2/U1/U2/I3/I4/U3 without changing recognition scope or adding media
retention. Cancellation acknowledgment is not a stale playback result. An image can be recovered
without requiring any prior successful speech. MAX_UPLOAD_ATTEMPTS is a documented edit point,
not a placeholder or a fixed successful-upload count.

**Alternatives considered**: Reusing playback acknowledgments leaves ambiguous generation fencing;
automatic replay restores stale queues; server-preview images add transport/storage obligations;
moving all tracking into foundations unnecessarily blocks the initial image milestone.

## Azure, Terraform and GitHub Actions

**Decision**: Application Gateway WAF_v2 with managed/bot rules, path-specific rate limits and
DDoS IP Protection fronts a private Ubuntu VM running the existing Docker Compose application.
Use ACR, Key Vault/managed identities, explicit NAT egress, sanitized Monitor/Log Analytics and
budget alerts. Terraform owns infrastructure and protected Blob state; GitHub Actions uses OIDC
for reviewed plans, tested immutable releases and protected deployment/rollback.

**Rationale**: This retains the measured native CPU/container environment and session ownership
model while meeting the requested cloud security and delivery controls. Explicit response-buffer
configuration preserves SSE. Multipart upload limits must be tested separately from WAF inspection
depth. Application tmpfs guarantees cannot be extended to managed WAF internals. Cloud deployments
interrupt sessions; application rollback restores a compatible release, never an audio backlog.

**Alternatives considered**: AKS and separate module services add orchestration without a scaling
requirement; managed container platforms change the host/swap verification boundary; Front Door
adds another proxy not needed for this regional controlled demo. Bicep is excluded by the user's
Terraform choice. Static Azure credentials and mutable image tags weaken delivery provenance.

Detailed decisions, current primary-source references, service inventory, editable policies,
release fields and test gates are in [azure-deployment.md](azure-deployment.md). Optional Azure
services are distinguished from the required baseline; no cloud resources were created.

## Upload reconciliation and historical audio corrections

**Decision**: Correlate each explicitly submitted upload with a client UUID, expose an atomic
UploadStatusSnapshot through a read endpoint and dedicated events, and publish terminal failure
only after cleanup. Unknown admission outcomes require explicit session replacement. Manual
repeat/recovery uses null live-source fields, retaining text without frame/track lifetime coupling.
FR-010 and US4 scenario 6 now expressly allow text/audio fallback when local video overlays cannot
be decoded or synchronized; their accessible explanation and continued recognition are test gates.

**Rationale**: A failed transfer can leave lifecycle state unchanged, so session.state cannot prove
its outcome. Bounded latest-receipt storage resolves observable transfers without an unbounded
idempotency ledger. Historical speech need not fabricate observation IDs or keep old pixels alive.
Null source fields also prevent manual replay from corrupting live overlays or latency measures.

**Alternatives considered**: Automatic retries or treating timeouts as failure can duplicate uncertain
work. Retaining every upload receipt or historical frame adds unnecessary state. Requiring overlays
on browsers without compatible local decoding would contradict the agreed text/audio prototype.
