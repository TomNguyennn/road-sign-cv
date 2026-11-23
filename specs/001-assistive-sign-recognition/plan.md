# Implementation Plan: Accessible Sign Recognition Web Prototype

**Branch**: `001-assistive-sign-recognition` | **Date**: 2026-11-23 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/001-assistive-sign-recognition/spec.md`

## Summary

Build a single C++ modular-monolith backend and an accessible TypeScript browser client. Users send
camera frames or upload images/videos; the backend performs detection, tracking, OCR, consolidation,
and speech synthesis. The browser plays generated speech and exposes optional visual results.
Use CPU inference as the reproducible baseline; record measured research results, not promised FPS.

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

**Storage**: No database or accounts. Session state and audio in memory; uploaded video and request
spools on bounded tmpfs. Authorised research media/model bundles are explicit read-only mounts;
evaluation outputs are written only by an offline evaluation command. No ordinary demo media,
recognised text, audio, or history is persisted.

**Testing**: GoogleTest/CTest for domain, native integration and contracts; ASan/UBSan and a separate
TSan build; pytest for dataset/export checks; Vitest for client state; Playwright/axe for workflows;
manual desktop and phone screen-reader tests and five-participant study. No tests have run yet.

**Target Platform**: Linux amd64 CPU deployment, 4 CPU cores and 8 GiB host RAM as reference minimum;
backend container budget 4 GiB including a 256 MiB tmpfs. Docker Desktop may run the same amd64
image for development; emulated results must be labelled and not used as the native benchmark.
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
  checklists/requirements.md
  contracts/api.md
  contracts/model-bundle.md
  contracts/ui.md
```

`tasks.md` will be generated by `$speckit-tasks`, not this workflow.

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
Dockerfile
compose.yaml
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
  Uploaded-video review uses matching browser timestamps; unsupported local playback does not
  prevent backend recognition or textual/audio review.
- Source replacement ends the old session. Seeking retains the uploaded file but creates a new
  epoch, resets tracking and audio, and discards all old-epoch results. Completing an input leaves
  results reviewable until explicit end/disconnect.

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
5. Run held-out evaluation, ten-minute native performance assessment, and the participant study.
   Report incomplete evidence explicitly; no benchmark score is assumed from library selection.

Detailed validation and requirement coverage are in quickstart.md. Future task generation must
keep tests alongside every behavioral change and include model/data preparation as real tasks.

## Complexity Tracking

No violations or added distributed infrastructure. Native library adapters and two worker queues
are necessary to satisfy C++ ownership, responsive controls, and blocking third-party APIs.
