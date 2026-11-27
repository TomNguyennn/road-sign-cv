# Implementation Plan: Accessible Sign Recognition Web Prototype

## Revised catalogue and obstacle work

The eight class IDs are fixed in `contracts/model-bundle.md`: retain IDs 0–4 and 6; replace ID 5 with the shared-path-with-cyclist sign and ID 7 with fixed street-furniture obstacles. Update the catalogue, policy, validator, training, model metadata and tests together before training. T025/T026 need revision; existing checkmarks do not prove the revised contract. Output remains `[1,12,8400]`; changed semantics require new training and parity evidence.

Review selected ROD dataset poles, bins, benches, cones, barriers and similar fixed objects, source permissions, annotations and UK relevance before importing. Exclude untraceable or unlicensed images, preserve source/location groups in held-out splits, and review partially labelled images before treating omitted objects as background. Preserve existing local files and do not commit raw datasets.

For accepted obstacles, announce a possible obstacle on the left, centre or right of the camera view. Derive position from the clipped box centre in equal-width image thirds. Apply existing tracking, confidence and duplicate suppression; priority is 0. Do not infer distance, collision, a clear path or a safe direction. Test position boundaries, uncertainty, camera movement and repeated alerts; evaluate on held-out UK scenes.

For normalized horizontal centre `x`, left is `0 <= x < 1/3`, centre is `1/3 <= x < 2/3`, and
right is `2/3 <= x <= 1`. Position describes the frame used for the accepted observation. A
position-only change on the same tracked obstacle does not advance its accepted revision or trigger
another automatic announcement; a new track follows the existing announcement policy.

**Branch**: `001-assistive-sign-recognition` | **Date**: 2026-11-23 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/001-assistive-sign-recognition/spec.md`

## Summary

Build a single C++ modular-monolith backend and an accessible TypeScript browser client. Users send
camera frames or upload images/videos; the backend performs detection, tracking, OCR, consolidation,
and speech synthesis. The browser plays generated speech and exposes optional visual results.
Use CPU inference as the reproducible baseline; record measured research results, not promised FPS.
Deploy the controlled Azure demo behind Application Gateway WAF on a private VM running Docker
Compose. Terraform owns infrastructure; GitHub Actions owns validation, immutable publishing and
protected deployment/rollback. [azure-deployment.md](azure-deployment.md) defines the service
inventory, WAF/bot/rate/DDoS controls, secrets, analytics and delivery contracts.

The repository currently contains documentation and Spec Kit scaffolding, not application code,
models, datasets, or build infrastructure. This command creates design documents only. Commands in
quickstart.md describe the implementation deliverables to be created by later tasks.

## Technical Context

**Language/Version**: C++20 with GCC 13 on Ubuntu 24.04; Python 3.12 for training/export/evaluation;
TypeScript 5.x strict mode with React 19 and Vite 7; Node.js 22 (minimum 22.12).

**Primary Dependencies**: OpenCV 4.x with FFmpeg video decoding; ONNX Runtime 1.x CPU C++ API;
Ultralytics YOLO11n eight-class custom detector; ByteTrack tracker-only C++ implementation and
Eigen 3; Tesseract 5.x with English tessdata_fast; eSpeak NG 1.52 C API; Drogon 1.9.x;
Caddy 2 for static frontend and same-origin HTTPS. CMake 3.28+, Ninja, GoogleTest, pytest,
Vitest/Testing Library, Playwright and axe-core. Release families are design baselines, not claims
of installed versions. Implementation must resolve supported patches within these families,
commit immutable source hashes/package locks/image digests, and record the tested manifest.
No floating latest dependencies in a reproducible build.
Terraform CLI/AzureRM provider and GitHub Actions must also use tested pinned versions/commit SHAs;
commit provider locks during implementation. Azure authentication uses federated/managed identities.

**Storage**: No database or accounts. Session state and audio in memory; uploaded video and request
spools on bounded tmpfs. Authorised research media/model bundles are explicit read-only mounts;
evaluation outputs are written only by an offline evaluation command. No ordinary demo media,
recognised text, audio, or history is persisted.
Azure Blob stores protected Terraform state/plans; ACR stores images and authorised model artifacts.
Sanitized operational telemetry goes to Log Analytics. These stores must never receive session media.
Azure runtime metrics use one fixed-schema JSONL sample/minute, bounded to 16 MiB of host files,
collected by AMA/DCR into RoadSignRuntime_CL. The agent's separate disk-cache quota is 4,000 MB.
Only the aggregate-log mount is persistent/writable; media/request spools stay on bounded tmpfs.

**Testing**: GoogleTest/CTest for domain, native integration and contracts; ASan/UBSan and a separate
TSan build; pytest for dataset/export checks; Vitest for client state; Playwright/axe for workflows;
manual desktop and phone screen-reader tests and five-participant study. No application tests have run yet; document validation is reported separately.

**Target Platform**: Linux amd64 CPU deployment, 4 CPU cores and 8 GiB host RAM as reference minimum;
backend container budget 4 GiB including a 256 MiB tmpfs. Apple Silicon development uses native
Linux arm64 containers with separate dependency/build directories; amd64 deployment checks run
on native amd64 runners. Emulated results must be labelled and not used as the native benchmark.
Azure starts with a private Ubuntu 24.04 Standard_D4s_v5 VM (4 vCPU/16 GiB), UK South subject to
quota, behind WAF_v2 with DDoS IP Protection; keep the same container limits and singleton session.
Browser acceptance: Chrome and Firefox desktop with NVDA/Windows, Safari/macOS with VoiceOver,
Safari/iPhone with VoiceOver, Chrome/Android with TalkBack. Test current stable versions at each
release and record exact versions; no background-tab capture or unsupported-browser guarantee.

**Project Type**: Controlled web research demo, one backend executable and a static web client.

**Performance Goals**: Camera ingestion capped at 5 frames/s and 1280x720; one active inference plus
one latest pending frame. This is a resource policy, not a minimum throughput promise. Measure
latency p50/p95, processed FPS and peak memory for ten minutes. Hard bounds prevent input/audio
queues growing without limit; correctness is tested separately from recognition accuracy.

**Constraints**: Eight catalogue categories; English; no runtime Python; no cloud OCR/TTS;
no retained session media; session-exclusive access; trusted HTTPS for phone camera. Stop and
mute cancel browser audio immediately and fence late worker results before publication.
Azure access is restricted to configured tester CIDRs. Application-controlled storage must stay
ephemeral; the UI must disclose cloud processing and avoid promising RAM-only handling inside
Azure's managed WAF. No persistent raw request/body/session telemetry may be configured.

**Scale/Scope**: One active session, three input modes, custom training, research evaluation;
no embedded systems, navigation, crossing decisions, accounts, or public multi-user deployment.

## Constitution Check

*GATE: Evaluated before Phase 0 research and re-evaluated after Phase 1 design.*

| Principle | Before research | After design |
|-----------|-----------------|--------------|
| Domain-first modules; explicit public boundaries | PASS: no existing code to migrate | PASS: modules below, acyclic public contracts, private include paths |
| C++ runtime and Python-only training | PASS | PASS: native inference/OCR/TTS/API; browser performs capture/playback only |
| Naming, readable functions, strict typing | PASS | PASS: PascalCase types/files, camelCase C++/TS, snake_case Python, strict TS; lint gates |
| Memory safety | PASS: ownership required | PASS: RAII handles, owned frame buffers, bounded queues, cancellation fences, sanitizers |
| Surgical edits and behavioral tests | PASS | PASS: design-only artifacts now; unit/contract/regression tests required in implementation |
| Docker and reproducible model handoff | PASS | PASS: two deployment containers, CPU model manifest/parity test, pinned builds |

No constitution exceptions are required. These passes assess design compliance, not execution or
production readiness. Dataset collection, model training, compatibility tests, and user evaluation
remain implementation work and must not be represented as completed evidence.

## Project Structure

### Documentation (this feature)

```text
specs/001-assistive-sign-recognition/
  spec.md
  plan.md
  research.md
  data-model.md
  quickstart.md
  azure-deployment.md
  checklists/requirements.md
  contracts/api.md
  contracts/model-bundle.md
  contracts/model-promotion.md
  contracts/telemetry.md
  contracts/ui.md
```

`tasks.md` is present and synchronized for the dependency corrections and Azure/Terraform/CI work.
The image-only US1 checkpoint precedes US2; full US1 camera acceptance occurs with US2 after
tracking and temporal consensus exist. Provenance capture follows package-lock resolution.

### Source Code (repository root)

```text
backend/
  app/                        # composition root and executable entry points
  modules/
    recognition-session/      # orchestration, lifecycle, HTTP/SSE adapters
    media-processing/         # image validation and OpenCV video decoding
    sign-recognition/         # model inference, geometry and ByteTrack
    text-extraction/          # OCR and text normalisation
    announcements/            # consolidation, priority and repetition policy
    speech/                   # C++ synthesis and PCM ownership
  shared/                     # immutable frame/geometry/identity contracts only
  infrastructure/             # configuration and redacted logging
  tests/integration/
frontend/
  src/modules/
    recognition-session/
    media-input/
    audio-feedback/
    results-review/
  src/shared/
  tests/e2e/
training/
  road_sign_training/          # dataset, train, export and evaluate Python commands
  tests/
config/
resources/                    # catalogue and non-user test fixtures
models/                       # ignored, checksum-verified model bundles
research-data/                # ignored, authorised offline datasets only
reports/                      # ignored, offline evaluation outputs
infra/terraform/              # bootstrap and one parameterized Azure environment stack
infra/cloud-init/             # host setup without secret values
infra/tests/                  # infrastructure policy and redaction tests
scripts/deploy/               # manifest validation, Compose switch and rollback
.github/workflows/            # CI, Terraform plan, publish and protected deploy
Dockerfile
compose.yaml
compose.azure.yaml
CMakeLists.txt
CMakePresets.json
```

Each backend module uses only the needed `public/`, `domain/`, `application/use-cases/`,
`application/ports/`, `infrastructure/`, and `tests/` directories. Public includes and CMake links
must not expose private layers. `recognition-session` calls public interfaces of the leaf modules;
leaf modules depend on shared value contracts, not each other's internals. The composition root
injects adapters. Do not introduce a generic service/repository hierarchy or an event broker.
Frontend features follow the same domain-first rule, with PascalCase component files.

**Structure Decision**: One C++ deployment unit with internal domain libraries. Caddy serves the
frontend and proxies API traffic, not a second business-logic service. Training is offline tooling.
Cloud resources protect and operate these same containers; they do not split domain modules into
services. The Azure profile uses public trusted certificates and private origin access; the local
profile retains the internal-CA workflow.

## Execution Design

- Session state transitions are serialized on one owning executor; native inference and speech use
  separate bounded workers. Shared callbacks carry session ID and epoch, never borrowed image
  views whose owner has gone out of scope. End/mute/seek invalidates work before waiting on workers.
- Drogon handles HTTP/SSE; React posts bounded JPEG camera frames. Uploaded images are decoded in
  C++; uploaded video uses OpenCV and FFmpeg from tmpfs, sampled at up to 5 FPS and paced by source
  time. Slow processing skips forward to the next current frame instead of accumulating delay.
- Training produces a static FP32 ONNX detector. C++ applies identical resize/normalisation/NMS,
  then time-aware ByteTrack association, cropped OCR, consensus and announcement scheduling.
  Concrete settings and tensor contracts are in research.md and contracts/model-bundle.md.
- Backend speech produces WAV. The browser has a single audio slot, acknowledges actual playback,
  and never runs browser speech synthesis. The server sends the next item after completion.
- Live frame coordinates and IDs are returned with the input epoch/sequence. Optional camera
  overlays display the corresponding sampled image, not an incorrectly aligned newer frame.
  Uploaded-video overlays require local decoding and matching timestamps. When unavailable,
  disable the overlay with an accessible explanation; backend recognition and textual/audio review
  continue. There is no server preview-image endpoint.
- Source replacement ends the old session. Seeking retains the uploaded file but creates a new
  epoch, resets tracking and audio, and discards all old-epoch results. Completing an input leaves
  results reviewable until explicit end/disconnect.
- Lost upload responses are reconciled through an atomic upload-status snapshot correlated with
  the transfer UUID. Terminal upload events are independent of lifecycle changes; unknown outcomes
  require explicit end/start rather than speculative resubmission.
- Manual repeat/recovery has null live-source fields in audio.ready and cannot affect overlays,
  automatic signature suppression or capture latency metrics. It retains text, not historical frames.
- Trusted publication retrieves the reviewed ACR candidate digest, validates its inventory/evidence
  and reruns parity. No GitHub job depends on a developer's ignored local model directory. Runtime
  operational metrics have a native emitter and explicit ingestion contract, not only dashboards.

## Implementation Sequence and Validation Gates

1. Build native/TS toolchains, public module targets, dependency locks, and empty-state accessible
   controls. Add session, media-limit, privacy, and protocol tests with deterministic fixtures.
2. Assemble authorised labelled dataset/catalogue, train YOLO11n, export and prove Python/C++
   parity. No pretrained generic model may silently stand in for the eight-class custom model.
3. Implement image recognition/OCR/speech as the first complete user slice, then live/video
   tracking, consolidation and cancellation. Test rules with deterministic observations separately
   from measured model accuracy.
4. Complete Docker HTTPS, phone access, upload/spool cleanup, memory bounds, sanitizers, API
   ownership tests, keyboard/screen-reader flows, and end-to-end reference fixtures.
5. Add Terraform state/bootstrap, private Azure networking/VM, WAF/rate/bot/DDoS controls, managed
   identity/Key Vault and sanitized Monitor analytics. Add GitHub Actions CI, reviewed plans,
   immutable ACR publishing and protected deployment with tested rollback; no deployment in planning.
6. Run held-out evaluation, ten-minute native performance assessment, and the participant study.
   Report incomplete evidence explicitly; no benchmark score is assumed from library selection.

Detailed validation and requirement coverage are in quickstart.md. Future task generation must
keep tests alongside every behavioral change and include model/data preparation as real tasks.

## Complexity Tracking

No constitution violations. Native library adapters and bounded vision/speech workers satisfy C++
ownership, responsive controls and blocking third-party APIs. A separate one-slot aggregate-metrics
writer keeps telemetry I/O off those paths. Azure adds the explicitly requested security
and operations infrastructure while preserving a single application deployment unit. No cluster,
message broker, database or distributed domain service is introduced.

## Analysis Remediation Decisions

- I1: stateRevision is explicit in creation, seek, audio-control and end responses; test agreement
  with SSE state ordering instead of inferring it from prose.
- I2: one successfully accepted upload per session; failed admitted attempts can retry within a
  startup-configured MAX_UPLOAD_ATTEMPTS budget (default3, 0 unlimited). Keep this editable in
  config/demo.env with a validated default in config/policy.json; exact accounting is in api.md.
- U1: a UUID cancellation receipt and its generation cutoff have a separate acknowledgment endpoint
  and session-owned ledger. Stale playback results cannot clear that barrier.
- U2: retain latest accepted information separately from last-started speech; an explicit Read
  latest result action can recover even the first blocked image announcement without a backlog.
- I3: T044 validates only the image slice. T058 validates the full camera journey after T048–T054;
  do not advertise camera recognition as complete or expose it as ready before temporal policy works.
- I4: T005 dependency provenance waits for both frontend and Python locks; it is not parallel to them.
- U3: video overlays require local decoding/synchronisation; otherwise text/audio review is the
  defined fallback. No preview-image transport is added.
- I5: upload.state plus GET upload-status expose the current/latest attempt and atomic counters;
  lost responses, unchanged lifecycle state and late events have explicit reconciliation rules.
- I6: audio.ready uses origin-discriminated source fields; repeat/recovery sets source IDs to null,
  has a bounded fresh offer lifetime and is excluded from live-frame associations and timing.
- U4: model-promotion.md defines operator-authenticated initial upload, reviewed candidate selection,
  trusted parity validation and digest-selected release manifests, including the first release.
- C1: telemetry.md defines the C++ aggregate schema/cadence/bounds and AMA/DCR/table/query path,
  including duplicate/gap handling and live ingestion evidence.

FR-010 and US4 scenario 6 explicitly accept the local-video overlay fallback; text/audio recognition
must continue across the documented browser matrix. Post-remediation constitution review remains
PASS: no language/boundary exceptions, retained session media, runtime Python or new domain service.

Commit coherent validated design/implementation increments to provide rollback points, as requested.
Use explicit paths for staging and leave .specify/memory/constitution.md uncommitted.
