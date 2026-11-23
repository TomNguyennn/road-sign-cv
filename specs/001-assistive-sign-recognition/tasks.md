---
description: "Dependency-ordered implementation tasks for the accessible sign recognition web prototype"
---

# Tasks: Accessible Sign Recognition Web Prototype

**Input**: Design documents from `specs/001-assistive-sign-recognition/`.
**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md),
[data-model.md](data-model.md), [API contract](contracts/api.md),
[model contract](contracts/model-bundle.md), [UI contract](contracts/ui.md),
and [quickstart.md](quickstart.md).

**Tests**: Required by the specification's acceptance scenarios and constitution. Write the listed
behavioral/contract tests before their implementation, demonstrate the expected failure, then make
them pass. Add edge-case unit tests alongside each behavioral implementation task; test fixtures
must not become production fallback implementations. Checked tasks mean completed with evidence,
not merely drafted code. All tasks below are intentionally unchecked.

**Organization**: Setup, common foundations, one phase per user story in specification order,
then cross-cutting validation. No application implementation is performed by this task-generation
command. Do not commit changes, including the constitution, unless the user later requests it.

## Format: `[ID] [P?] [Story] Description`

- `[P]` marks disjoint-file tasks that may run together after their named phase/wave prerequisites.
- `[US1]`–`[US4]` identify the specification's user stories, not technical modules.
- All paths in task descriptions are repository-relative; named headers have matching `.cpp`
  implementations where necessary. Do not create unused directories or placeholder production code.
- Without an explicit parallel wave below, work in task-ID order. Dependencies mentioned in a task
  are additional gates, not permission to skip earlier foundational work.

## Path Conventions

Backend domains live in `backend/modules/<domain>/` with public interfaces and private layers.
Frontend features live in `frontend/src/modules/<domain>/`; Python tooling in
`training/road_sign_training/`. Module tests stay with their module; cross-module integration tests
live in `backend/tests/integration/`. Offline research data/models/reports are ignored generated
artifacts, distinct from session-only demo media. Exact contracts and policy numbers come from
the linked design documents; do not change them silently during implementation.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish reproducible toolchains and test entry points without implementing features.

- [ ] T001 Create scoped artifact exclusions in `.gitignore` and `.dockerignore` for `.venv/`, build outputs, secrets, `models/`, `research-data/`, and `reports/`; retain authorised small fixtures and manifests deliberately, without hiding source files or modifying the constitution.
- [ ] T002 Establish C++20/GCC13, CMake/Ninja, pinned native dependency fetching and source checksums in `CMakeLists.txt`, `CMakePresets.json`, and `cmake/Dependencies.cmake`; include OpenCV/FFmpeg, ORT CPU, Drogon, Eigen, ByteTrack tracker sources, Tesseract, eSpeak NG and GoogleTest; define separate dev, ASan+UBSan and TSan presets.
- [ ] T003 [P] Initialise the strict TypeScript/React/Vite frontend and locked Node22 dependencies in `frontend/package.json`, `frontend/package-lock.json`, `frontend/tsconfig.json`, and `frontend/vite.config.ts`; provide lint, typecheck, test, build and test:e2e commands without a deployed Vite server.
- [ ] T004 [P] Initialise the typed Python3.12 training/evaluation package and immutable dependency lock in `training/pyproject.toml`, `training/requirements.lock`, and `training/road_sign_training/__init__.py`; include PyTorch/Ultralytics, ONNX tooling, pytest and metric dependencies without adding Python to the runtime backend.
- [ ] T005 [P] Record dependency versions/checksums and upstream code/model/data licence notices in `config/dependency-manifest.json` and `docs/dependency-provenance.md`; include the ByteTrack source commit, English OCR/voice data and Ultralytics/eSpeak terms without selecting a new repository licence.
- [ ] T006 Configure naming, formatting, lint and type checks in `.clang-format`, `.clang-tidy`, `frontend/eslint.config.js`, and `training/pyproject.toml`; preserve PascalCase C++/TS files/types, camelCase C++/TS values, snake_case Python and strict annotations without suppressions.
- [ ] T007 Add native module targets with private include paths and acyclic public dependencies in `backend/CMakeLists.txt` and `backend/tests/ModuleBoundariesTest.cmake`; test that private-domain/infrastructure imports between modules fail instead of exposing all backend includes globally.
- [ ] T008 Configure deterministic test entry points in `frontend/vitest.config.ts`, `frontend/playwright.config.ts`, `backend/tests/TestSupport.hpp`, and `training/tests/conftest.py`; provide controllable clocks/worker barriers and test-only observation/audio fixtures, and make the planned commands discoverable.

**Checkpoint**: Dependency manifests, language checks and empty test runners work; no trained model
or dataset is assumed present. T003–T005 form one parallel wave after T002.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared lifecycle, ownership, protocol and deployment needed by all stories.
**Gate**: Finish this phase before story implementation; test-only collaborators may substitute for
unfinished leaf modules, but production readiness remains false until the real bundle loads.

- [ ] T009 [P] Write lifecycle/authentication contract tests in `backend/modules/recognition-session/tests/SessionFoundationTest.cpp` for atomic one-session admission, cookie/Origin/CSRF checks, 10-second SSE attachment, 5-second heartbeat/15-second lease, late callbacks, cleanup and second-client isolation.
- [ ] T010 [P] Write ownership, resource-policy and error-redaction tests in `backend/tests/integration/RuntimeFoundationTest.cpp` for retained frame backing storage, bounded worker queues, finite geometry, no raw text/token logs and no persistent session spools.
- [ ] T011 Define shared immutable contracts in `backend/shared/Frame.hpp`, `backend/shared/Geometry.hpp`, and `backend/shared/Identity.hpp`; preserve Frame constraints verbatim: "frameId uint64 monotonic per epoch, sourceTimeMs finite nonnegative, clientCaptureTimeMs optional, width/height, owned pixel bytes" and "One active + one latest pending"; reject invalid dimensions/arithmetic before allocation and provide copy/move lifetime tests.
- [ ] T012 Define module public interfaces in `backend/modules/media-processing/public/MediaSource.hpp`, `backend/modules/sign-recognition/public/SignObservation.hpp`, `backend/modules/text-extraction/public/ExtractTextRequest.hpp`, `backend/modules/announcements/public/AcceptedInformation.hpp`, and `backend/modules/speech/public/AudioArtifact.hpp`; preserve "confidence [0,1]", "box finite xywh in upright original pixels, clipped before ROI access", "revision uint32", "formatting/case-only text changes do not advance revision", and audio "<=45 seconds; released at completion/cancellation/end"; keep library handles private.
- [ ] T013 Implement the session entity and serialised lifecycle in `backend/modules/recognition-session/domain/RecognitionSession.hpp` and `backend/modules/recognition-session/application/SessionService.cpp`; preserve "sessionId UUID", "sourceKind camera/image/video", "epoch uint32 starting 1", "stateRevision monotonic uint64", "csrfToken random secret", "audioGeneration uint32", "isMuted bool", and "One global active lease"; implement created/awaitingInput/processing/reviewing/ending/ended and release the lease only after cleanup.
- [ ] T014 Implement owned vision/speech worker handoff and cooperative cancellation in `backend/modules/recognition-session/infrastructure/SessionExecutor.cpp`; tag work by session/epoch/generation, discard stale completions before publication, cap vision work at one active plus one latest pending, and never detach workers referencing destroyed state.
- [ ] T015 Implement JSON validation, fixed safe error codes, cookie/Origin/CSRF ownership, POST/DELETE sessions and GET health/ready/capabilities handlers in `backend/modules/recognition-session/infrastructure/SessionController.cpp`; follow every status/no-store rule in `contracts/api.md`, reject unknown fields, do not reveal another session and return unavailable until model verification exists.
- [ ] T016 Implement the ordered single-attachment SSE stream and heartbeat endpoint in `backend/modules/recognition-session/infrastructure/SessionEventsController.cpp`; emit session/epoch/stateRevision/eventSeq, 64-KiB maximum events, latest-result coalescing, priority cancel/end messages and no replay IDs; expire undrained streams/leases and test state-before-result ordering.
- [ ] T017 Implement validated environment/policy loading and redacted logs in `backend/infrastructure/RuntimeConfig.cpp`, `backend/infrastructure/Logger.cpp`, and `config/policy.json`; encode all fixed policies from `research.md`, monotonic clocks and resource ceilings, with tests that model-supplied policies cannot enlarge hard media/storage limits.
- [ ] T018 Implement the application composition root and session cleanup hooks in `backend/app/Main.cpp`; inject module public interfaces, establish graceful shutdown and RAII cleanup of request spools/buffers, and ensure a readiness failure does not instantiate fake inference/OCR/TTS behavior.
- [ ] T019 Implement pre-body upload reservation and validation in `backend/modules/recognition-session/infrastructure/MediaAdmissionController.cpp`; expose `/internal/admit-media` privately, reserve one upload atomically, validate original request ownership, return an internal nonce, reject a second upload before spooling and enforce 60-second body/65-second admission cleanup deadlines.
- [ ] T020 Provide tested multi-stage `Dockerfile`, `compose.yaml`, `config/Caddyfile`, and `config/demo.env.example`; build backend-runtime/web-runtime targets, proxy same-origin HTTPS with trusted internal CA, protect the private admission route, use read-only non-root backend with 4-GiB memory/256-MiB tmpfs and no core dumps, cap multipart bodies at 101 MiB and memory spooling at 10 MiB, persist only CA state, and document host/VM swap verification.

**Checkpoint**: T009/T010 pass against the foundation; real model readiness may remain unavailable.
The deployment can serve health/status without claiming the recognition feature is complete.

---

## Phase 3: User Story 1 - Hear Sign Information (Priority: P1)

**Goal**: Turn an uploaded image or browser-camera observation into spoken supported sign meanings
and readable text. Custom data/model preparation is part of delivering this story.

**Independent Test**: Present authorised labelled examples for each of the eight categories through
image and camera inputs; compare accepted descriptions/text with the reference and verify that
uncertain/unreadable/unsupported information is not fabricated, with visual results hidden.

### Tests for User Story 1

- [ ] T021 [P] [US1] Write image/camera/media/audio HTTP contract tests in `backend/tests/integration/ImageRecognitionContractTest.cpp` covering input modes, limits, accepted/uncertain results, no-result status, WAV ownership, missing bundle readiness and known-error responses.
- [ ] T022 [P] [US1] Write dataset/export/parity tests in `training/tests/test_model_handoff.py` for provenance, group leakage, class order, manifest hashes, exact tensors/preprocessing, trained-framework/export/native agreement and failure on incompatible models.
- [ ] T023 [P] [US1] Write detection/crop/OCR/speech boundary tests in `backend/tests/integration/NativeRecognitionTest.cpp` for non-square images, partial-edge boxes, letterboxing, finite scores, unreadable text, PCM ownership and unavailable native resources.
- [ ] T024 [P] [US1] Write source selection, permission, single-frame transfer and generated-audio playback tests in `frontend/tests/e2e/HearSignInformation.spec.ts`; use deterministic test-only fixtures and assert no microphone permission, visual dependency or browser speech synthesis.

### Implementation for User Story 1

- [ ] T025 [US1] Define the eight-entry catalogue in `resources/catalogue.json` and annotation rules in `docs/dataset-annotation.md`; preserve "classId 0–7, category key, spokenName, isTextRelevant, priority, included/excluded variants", exact model-contract order and symbol/text boundaries, including negative/unsupported examples and no inferred navigation instructions.
- [ ] T026 [US1] Implement dataset manifest types/validation in `training/road_sign_training/dataset_manifest.py` and `training/road_sign_training/validate_dataset.py`; preserve "datasetId/version, authorised sources and usage terms, scene/location group, split, annotations, condition tags, checksums" and "group-separated deterministic splits"; enforce typed boxes/transcripts/track IDs, grouped 70/15/15 split with seed42, coverage checks and train-only augmentation.
- [ ] T027 [US1] Acquire or collect authorised UK scenes and annotate images/video for all eight categories in `research-data/uk-signs-v1/manifest.json`; record permissions, source/location grouping, transcripts, physical track IDs and robustness tags, freeze held-out splits and include negatives; if source data/access is unavailable, record the actual blocker in `reports/dataset-readiness.md` and leave this task incomplete instead of fabricating data or using demo sessions.
- [ ] T028 [US1] Implement reproducible custom YOLO11n training in `training/road_sign_training/train.py` and `training/config/train.yaml`; explicitly load the named checkpoint, record seeds/toolchain/config and dataset hashes, tune only on validation data and write the documented `reports/training-v1/best.pt` output.
- [ ] T029 [US1] Implement export and bundle verification in `training/road_sign_training/export.py` and `training/road_sign_training/model_manifest.py`; preserve ModelManifest "modelId, model hash, class order, exporter/runtime versions, shapes, preprocessing/NMS, parity results"; emit manifest/catalogue/policy/parity metadata, FP32 `[1,3,640,640]` input and `[1,12,8400]` output, opset17 and nms=false, with no generic fallback.
- [ ] T030 [US1] Implement JPEG/PNG/frame decoding and MediaSource validation in `backend/modules/media-processing/infrastructure/OpenCvImageClient.cpp`; preserve "sourceKind, original dimensions, durationMs for video, current mediaTimeMs, tmpfs file handle for uploads" and "generated filename, never user path"; enforce 1-MiB/1280x720 camera and 10-MiB/20-million-pixel image limits, orientation and finite bounds before ROI/allocation.
- [ ] T031 [US1] Implement ORT CPU bundle loading, tensor validation, shared explicit letterboxing/normalisation and class-aware NMS in `backend/modules/sign-recognition/infrastructure/OnnxDetectionClient.cpp`; use the model contract, allow negative top-left coordinates before clipping, bound runtime candidates to100 and expose no private ORT/OpenCV handles across modules.
- [ ] T032 [US1] Implement native parity inference mode in `backend/app/EvaluateMain.cpp` and Python comparison in `training/road_sign_training/check_parity.py`; verify trained-framework versus export and native outputs with atol1e-4/rtol1e-3, identical fixture labels and restored boxes within1 pixel, including explicit threshold-boundary expectations.
- [ ] T033 [US1] Run dataset validation, custom training, export and native parity after T027–T032; produce `models/signs-v1/manifest.json`, `models/signs-v1/parity.json`, and `reports/training-v1/run.json` with real hashes/results, recording missing data/compute as unfinished work rather than marking model acceptance complete.
- [ ] T034 [US1] Implement worker-owned English Tesseract extraction in `backend/modules/text-extraction/infrastructure/TesseractTextClient.cpp`; enforce crop minimum16x16/longest-edge1600, appropriate segmentation and word-confidence>=70, Unicode normalisation and correct RAII deletion of returned text; test unreadable/empty/multiline signs and never invent missing words.
- [ ] T035 [US1] Implement still-image accepted-information rules in `backend/modules/announcements/domain/AcceptImageInformationUseCase.cpp`; preserve "trackId, revision uint32, category, displayText, normalizedText, priority, capture reference", require score>=0.75 for stills and accepted OCR confidence, mark truncated descriptions as partial and quote observed information without route/crossing decisions.
- [ ] T036 [US1] Implement serialised retrieval-mode eSpeak NG and owned WAV output in `backend/modules/speech/infrastructure/EspeakSpeechClient.cpp`; preserve "announcementId, WAV bytes, sampleRateHz, durationMs, epoch, audioGeneration", "<=45 seconds", en-gb/160 words per minute and <=500-character input, with cancellation-safe callbacks and no server sound-device output.
- [ ] T037 [US1] Implement the initial single-offer speech lifecycle in `backend/modules/announcements/application/AnnouncementService.cpp`; preserve "announcementId UUID, trackId/revision, text <=500 characters, priority, isReplay, epoch, audioGeneration, created/expiry times, state" and signature `(trackId, revision)`; offer only one WAV, suppress duplicate pending/synthesizing/offered work, enforce expiry and started/ended/cancelled/failed handling before slot reuse.
- [ ] T038 [US1] Wire the image/camera pipeline in `backend/modules/recognition-session/application/RecogniseFrameUseCase.cpp`; compose media, inference, OCR, acceptance and speech only through public interfaces, fence late callbacks, produce no-result image status and leave completed image results reviewable.
- [ ] T039 [US1] Implement frame/media and audio/playback handlers in `backend/modules/recognition-session/infrastructure/RecognitionController.cpp`; use admission nonces for uploads, correct SSE result/audio schemas, session-owned WAV fetch with residual-validity header, safe error statuses and no-store; unsupported video operation remains explicit until US2 implements decoding.
- [ ] T040 [US1] Implement typed session API/EventSource client and revision-aware reducer in `frontend/src/modules/recognition-session/SessionClient.ts` and `frontend/src/modules/recognition-session/SessionStore.ts`; reject stale messages, keep secrets only in memory, close EventSource on error, send heartbeats and require explicit restart.
- [ ] T041 [US1] Implement image upload and bounded camera capture in `frontend/src/modules/media-input/MediaInput.tsx` and `frontend/src/modules/media-input/CameraCaptureService.ts`; request rear-facing camera as preference, capture JPEG quality0.8 at<=5Hz with one outstanding POST, skip stale frames, show limits and release tracks on stop/failure/hidden page.
- [ ] T042 [US1] Implement native-generated audio playback in `frontend/src/modules/audio-feedback/AudioPlaybackService.ts`; activate AudioContext on user gesture, fetch/decode one WAV, subtract elapsed fetch/decode time from residual validity, validate epoch/generation again before starting, send playback acknowledgments and cancel locally before waiting for network cleanup.
- [ ] T043 [US1] Compose the initial labelled source/start/stop/status interface in `frontend/src/modules/recognition-session/RecognitionPage.tsx` and `frontend/src/App.tsx`; expose privacy and unavailable-model status, provide a safe stop action and optional non-live result text, and do not present US2/US3 controls as implemented until wired.
- [ ] T044 [US1] Execute the independent image/camera scenarios with the verified bundle and save sanitised evidence in `reports/acceptance/us1.md`; run T021–T024 plus module unit tests, verify all eight categories on reference fixtures and separate model errors from deterministic contract failures.

**Checkpoint**: Full US1 is testable without optional visual review or the research reporting UI.
An uploaded-image demonstration is the first integration milestone; full continuous assistive use
still requires US2 repetition handling and US3 audio/accessibility controls. T033/T044 cannot pass
on fake or generic weights.

---

## Phase 4: User Story 2 - Follow Signs Across Changing Scenes (Priority: P1)

**Goal**: Recognise live/uploaded video with distinct identities, stable text, bounded speech and
pedestrian-relevant priority instead of repeated or stale announcements.

**Independent Test**: Replay annotated sequences for unchanged, occluded, identical-looking and
changing signs; compare tracks and announcements against the policy. Test scheduler behavior using
fixed observations independently from detector quality.

### Tests for User Story 2

- [ ] T045 [P] [US2] Write tracker/consensus tests in `backend/modules/sign-recognition/tests/TrackingTest.cpp` for per-class overlaps, variable source-time intervals, 2-second loss/reacquisition, separate same-text signs, frame-history versus OCR-history retention, track bounds and removed-history cleanup.
- [ ] T046 [P] [US2] Write priority/duplicate/cancellation tests in `backend/modules/announcements/tests/AnnouncementPolicyTest.cpp` for pending/synthesizing/offered/started signatures, changed text, whitespace-only changes, queue overflow, five-second expiry and cancellation-ack timeout before reuse.
- [ ] T047 [P] [US2] Write video/upload/seek contract tests in `backend/tests/integration/VideoRecognitionContractTest.cpp` for codecs/limits, EOF review, source-time pacing, admission rejection, epoch reset during inference/TTS and no old-epoch playback.

### Implementation for User Story 2

- [ ] T048 [US2] Implement TrackedSign and its independent histories in `backend/modules/sign-recognition/domain/TrackedSign.hpp`; preserve "trackId string composite of session/epoch/category/localId", "last five detection observations", "separate last three OCR samples within 2 source seconds", and "Session owns max 100 active/lost tracks"; implement tentative/confirmed/lost/removed states without matching by text alone.
- [ ] T049 [US2] Adapt pinned tracker-only ByteTrack in `backend/modules/sign-recognition/infrastructure/ByteTrackClient.cpp`; use per-category trackers, elapsed source time in prediction/expiry, thresholds0.50/0.10/new0.60 and association-cost<=0.80, 2-second loss retention, bounded removed history and the no-reused-identity contract.
- [ ] T050 [US2] Implement temporal acceptance and OCR scheduling in `backend/modules/announcements/domain/ConsolidateInformationUseCase.cpp`; confirm category>=0.60 in3-of-last5 observations within2s, schedule <=1 OCR/track/s and <=4 crops/frame, require two normalised matching OCR samples within2s, preserve accepted text on transient conflicts and advance revision only on substantive changes.
- [ ] T051 [US2] Extend the scheduler in `backend/modules/announcements/application/AnnouncementService.cpp` to enforce "At most 3 pending + one offered/playing", five-second freshness, category priorities/oldest-first ties and specified overflow; invalidate lost/obsolete pending results, retain started signatures per track/revision and never preempt playing speech merely for a higher priority.
- [ ] T052 [US2] Implement OpenCV/FFmpeg video decoding in `backend/modules/media-processing/infrastructure/OpenCvVideoClient.cpp`; accept only MP4/H.264 or WebM/VP8/VP9 <=100MiB/300s/1920x1080/60FPS, ignore input audio, validate timestamps/dimensions, process<=5Hz source-time samples and skip stale frames rather than growing queues.
- [ ] T053 [US2] Implement uploaded-video processing/review and seek orchestration in `backend/modules/recognition-session/application/ProcessVideoUseCase.cpp` and `backend/modules/recognition-session/infrastructure/VideoController.cpp`; increment epoch/stateRevision atomically before events, reset tracking/history/audio on seek, retain the validated tmpfs file and reject out-of-range or stale seek requests.
- [ ] T054 [US2] Integrate live-frame tracking and consensus into `backend/modules/recognition-session/application/RecogniseFrameUseCase.cpp`; propagate source times/track IDs/revisions, apply ROI limits and OCR scheduling, and preserve the US1 still-image acceptance path.
- [ ] T055 [US2] Add video upload, timestamped progress, EOF review and labelled seek in `frontend/src/modules/media-input/VideoInput.tsx`; coordinate seek through `frontend/src/modules/recognition-session/SessionClient.ts`, await authoritative epoch/stateRevision and clear old results/audio before resuming.
- [ ] T056 [US2] Add streaming scenario tests in `frontend/tests/e2e/TrackSigns.spec.ts` for upload/live sequences, early new-epoch SSE before HTTP replies, stale offered audio, seek during fetch and backend/frontend cancellation agreement.
- [ ] T057 [US2] Verify long-run tracking/queue limits with a deterministic ten-minute sequence in `backend/modules/sign-recognition/tests/TrackingSoakTest.cpp`; assert <=100 tracks, bounded removed/OCR histories, one active/one pending frame and no lingering ended-session references under sanitizers.
- [ ] T058 [US2] Run all US2 independent scenarios and regress US1, recording observed identities, automatic versus intentional repeats and cancellation outcomes in `reports/acceptance/us2.md`; do not present fixture success as held-out tracking accuracy.

**Checkpoint**: US2 is independently measurable on annotated video and policy fixtures; its full
integration depends on the US1 pipeline. No fixed minimum FPS/accuracy has been invented.

---

## Phase 5: User Story 3 - Control Recognition Without Visual Interaction (Priority: P1)

**Goal**: Make source, start/stop, mute/unmute, repeat, audio activation and error recovery usable
without visual results, with reliable cancellation and screen-reader status.

**Independent Test**: Complete every primary action keyboard-only and separately with desktop/phone
screen readers; force permission, playback and connection errors and verify recovery without stale
capture or speech. Backend controls can be tested with deterministic audio observations.

### Tests for User Story 3

- [ ] T059 [P] [US3] Write backend audio-control tests in `backend/modules/recognition-session/tests/AudioControlsTest.cpp` for mute/generation changes, unmute without backlog, empty/muted repeat, blocked activation, stale acknowledgments and missing-cancel-ack session termination.
- [ ] T060 [P] [US3] Write state/playback race tests in `frontend/src/modules/audio-feedback/AudioPlaybackService.test.ts` for immediate mute/stop, rapid mute-unmute intent ordering, expired GET validity, superseded fetches and no playback-slot reuse before confirmed cancellation.
- [ ] T061 [P] [US3] Write keyboard/focus/status and recovery tests in `frontend/tests/e2e/AccessibleControls.spec.ts` for every control, hidden visuals, non-live transcripts, denial/revocation, malformed uploads, EventSource failure, hidden-page cleanup and no automatic resume.

### Implementation for User Story 3

- [ ] T062 [US3] Implement mute/unmute/enable/repeat orchestration and endpoint in `backend/modules/recognition-session/application/ControlAudioUseCase.cpp` and `backend/modules/recognition-session/infrastructure/AudioControlsController.cpp`; increment stateRevision/audioGeneration, clear pending work, coalesce repeated clicks, enforce isReplay=true and unavailable-state responses without reviving cancelled artifacts.
- [ ] T063 [US3] Complete generation-safe control reconciliation in `frontend/src/modules/recognition-session/SessionStore.ts`; serialize seek/audio commands, accept newer authoritative session.state before late HTTP replies, ignore older revisions and preserve immediate local mute/stop intent across in-flight unmute responses.
- [ ] T064 [US3] Complete audio recovery, repeat and cancellation acknowledgment in `frontend/src/modules/audio-feedback/AudioPlaybackService.ts`; handle blocked context, Enable audio, playback timeout/cancel and no backlog on unmute, and clear decoded buffers/object URLs on generation/session change.
- [ ] T065 [US3] Add labelled native controls and accessible state/reason text in `frontend/src/modules/audio-feedback/AudioControls.tsx` and `frontend/src/modules/recognition-session/SessionControls.tsx`; implement the prescribed control order, visible focus and no-result/empty-repeat/muted behavior without hidden shortcuts or visual-only indications.
- [ ] T066 [US3] Implement coalesced polite status and nonduplicated error recovery in `frontend/src/modules/recognition-session/SessionStatus.tsx`; cover busy/unavailable/permission/input/connection/audio errors without focus theft, per-frame announcements or simultaneous automatic transcript speech.
- [ ] T067 [US3] Complete cleanup for stop, hidden page, source replacement, disconnect and aborted uploads in `frontend/src/modules/recognition-session/SessionLifecycleService.ts`; abort fetches, stop camera, close EventSource, silence audio and clear temporary state immediately, treating server cleanup as asynchronous and new sessions as fresh.
- [ ] T068 [US3] Test trusted-HTTPS camera/audio and actual screen readers across the UI-contract desktop/phone matrix, recording OS/browser/assistive versions and pass/fail primary tasks in `reports/accessibility/device-matrix.md`; automated WebKit or axe results cannot substitute for physical phone/assistive tests.
- [ ] T069 [US3] Fix only accessibility/control defects found by T059–T068 in their owning components and add regression cases in `frontend/tests/e2e/AccessibleControls.spec.ts`; keep status versus transcript behavior separate and preserve US1/US2 recognition.
- [ ] T070 [US3] Run the independent controls/recovery scenarios and prior-story regression suite; record keyboard/screen-reader task completion, immediate local stop and residual data checks in `reports/acceptance/us3.md`, leaving unavailable devices as explicit unfinished validation.

**Checkpoint**: All P1 recognition, repetition and accessible-control behavior is integrated. Phone
CA trust, real audio and assistive-tool evidence are required before claiming those environments work.

---

## Phase 6: User Story 4 - Review and Evaluate the Prototype (Priority: P2)

**Goal**: Provide optional visual/session review and reproducible research evidence without retaining
ordinary demo input or treating unmeasured model quality as a pass.

**Independent Test**: Compare optional visual/history content with reference observations, then run
the offline evaluator against labelled fixtures and held-out data, verifying every required metric,
condition count, provenance field and explicit missing-evidence indication.

### Tests for User Story 4

- [ ] T071 [P] [US4] Write result/history tests in `frontend/src/modules/results-review/ResultsReview.test.tsx` for matching frame/box/text, optional visuals, non-live accessible review, intentional-replay labels, truncation and session/epoch clearing.
- [ ] T072 [P] [US4] Write evaluation arithmetic/schema tests in `training/tests/test_evaluation.py` using hand-computed detections, missing/unmatched OCR regions, ID switches and automatic/replay events; verify denominators, timing clock domains, condition counts and no implied success on absent evidence.

### Implementation for User Story 4

- [ ] T073 [US4] Implement bounded HistoryEntry and review snapshots in `backend/modules/announcements/domain/HistoryEntry.hpp` and `backend/modules/announcements/application/HistoryService.cpp`; preserve "announcementId, text, isReplay, playback outcome, startedAt" and "Latest 200 only; accessible notice on truncation; cleared at session end", retain only latest repeat text separately and never persist histories.
- [ ] T074 [US4] Implement optional matching-frame overlays and review text in `frontend/src/modules/results-review/ResultsReview.tsx`; align boxes to sampled camera frame or video source timestamp, preserve accessible text when visuals are hidden and do not attach stale boxes to unrelated pixels.
- [ ] T075 [US4] Implement chronological announcement history in `frontend/src/modules/results-review/AnnouncementHistory.tsx`; derive entries from current-session playback events, label intentional replay/outcome, cap200 and show truncation, and clear on seek/end/new session without storage persistence.
- [ ] T076 [US4] Extend the offline native evaluator in `backend/app/EvaluateMain.cpp` to process authorised labelled media through the real public pipeline, export detections/OCR/identities/announcements and stage timings to the explicit output directory, and prevent the online server from writing offline research mounts.
- [ ] T077 [US4] Implement metric aggregation and EvaluationRun validation in `training/road_sign_training/evaluate.py` and `training/road_sign_training/evaluation_run.py`; preserve "runId, dataset/model/config hashes, environment, sampled counts, metrics, failures, study summary" and "user participants identified by study codes, not session media"; compute ranked mAP at floor0.001/max300 before tracking, separate runtime P/R, end-to-end/matched CER/WER, IDF1/switches/fragmentation and replay-excluded duplicates with the quickstart definitions.
- [ ] T078 [US4] Implement explicit evaluation-only browser timing/counter export in `frontend/src/modules/recognition-session/EvaluationMetricsService.ts` and backend counters in `backend/infrastructure/EvaluationMetrics.cpp`; use client monotonic capture-to-result/playback timing, separate upload/source timing, record dropped/processed frames and queue/memory peaks, and exclude images, transcripts and audio from ordinary demo logs.
- [ ] T079 [US4] Run held-out recognition/OCR/tracking and condition-stratified evaluation, recording true samples, misses, false positives and provenance in `reports/evaluation-v1/metrics.json` and `reports/evaluation-v1/summary.md`; publish incomplete coverage honestly and never tune on this held-out run.
- [ ] T080 [US4] Run the ten-minute live-camera benchmark on the documented native reference host and record p50/p95 latency, actual FPS, start/end/peak process and container memory, dropped frames, queue high-water marks and environment hashes in `reports/performance/live-camera.json`; distinguish native/emulated measurements and ingestion cap from measured throughput.
- [ ] T081 [US4] Prepare consented participant tasks and collect the five-person blind/partially sighted usability evaluation in `docs/usability-study.md` and `reports/usability/study-summary.json`; record coded participant counts, task completion and 1–5 clarity/relevance/timing ratings, and leave recruitment/evidence incomplete if participants are unavailable rather than substituting developer ratings.
- [ ] T082 [US4] Run the independent review/evaluation scenarios and assemble evidence against SC-001–SC-010 in `reports/acceptance/us4.md`; link model/data hashes and prior-story evidence, distinguish functional pass/fail from research scores and explicitly list unperformed study/device/performance checks.

**Checkpoint**: Review and research reporting exist as a separate increment. T079–T082 require real
artifacts/evidence; executable metric tooling alone does not complete the research evaluation.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Verify the complete prototype and its delivery instructions without expanding scope.

- [ ] T083 [P] Add final lifecycle/privacy integration tests in `backend/tests/integration/SessionPrivacyTest.cpp` for stop during multipart spooling, nonce abuse/concurrent uploads, invalid cookies/CSRF, session replacement and container restart; inspect no-store responses, tmpfs cleanup, redacted logs and absence of user data in persistent mounts/browser storage.
- [ ] T084 [P] Add cross-module cancellation stress tests in `backend/tests/integration/CancellationStressTest.cpp` for late inference/OCR/TTS callbacks, reordered control replies, disconnect during playback and repeated start/stop; exercise both sanitizers and verify no surviving session-owned buffers or detached workers.
- [ ] T085 Run native builds/unit/contracts/integration, ASan+UBSan, separate TSan, Python checks, strict TS/lint, frontend tests and browser workflows, recording actual commands/results and any unsupported instrumentation in `reports/validation/final-checks.md`; fix failures in scoped files with regression tests and do not suppress checks to pass.
- [ ] T086 Validate both Docker build targets, Compose startup/readiness, non-root/resource bounds, proxy admission and physical-phone HTTPS trust using `Dockerfile`, `compose.yaml`, and `config/Caddyfile`; record native dependency/model hashes, host swap state and any failed deployment checks in `reports/validation/deployment.md`.
- [ ] T087 Update `README.md` and `specs/001-assistive-sign-recognition/quickstart.md` to the commands actually implemented and exercised; document dependency bootstrap, dataset/model acquisition, CA trust, privacy boundaries, measured limitations and cleanup without claiming unexecuted tests passed.
- [ ] T088 Audit every requirement and evidence gate against the constitution in `reports/validation/completion.md`; verify no production stubs, runtime Python, private cross-module includes, unexpected persistence or scope additions, leave unfinished external-data/device/study tasks unchecked and do not commit anything without a new user instruction.

---

## Dependencies & Execution Order

### Phase Dependencies

```text
Setup T001–T008 -> Foundations T009–T020 -> US1 T021–T044
                                             |
                       +---------------------+---------------------+
                       v                                           v
                 US2 T045–T058                          US3 test/control preparation
                       |                                           |
                       +--------------> US3 T059–T070 <-------------+
                                             |
                                             v
                                      US4 T071–T082
                                             |
                                             v
                                    Polish T083–T088
```

The default execution is sequential by phase. Overlap is optional and restricted below; the graph
does not imply all story implementations are independent. Each story has its own observable test,
but full-stack US2 builds on US1 and full US3 validation needs US2 seek/continuous behavior.

### User Story Dependencies

- **US1**: Requires foundations and an authorised dataset plus a verified custom bundle for final
  acceptance. Its native adapters and UI can be developed with test-only fixtures while data work
  is pending; do not mark T033/T044 complete without real evidence.
- **US2**: Requires US1's media/inference/OCR/speech contracts and implementation. Pure tracker and
  scheduler tests can be authored earlier against public contracts, but integration waits for US1.
- **US3**: Control tests and disjoint UI components can be prepared after US1; full validation requires
  US2 seek/streaming. T063/T064 touch existing US1 files and must not overlap their earlier owners.
- **US4**: Offline metric unit tests and study preparation can start after foundations and catalogue
  definition; actual evaluation requires the trained model, continuous pipeline and accessible
  prototype. Reports/visual review integrate after US3 in the default sequence.
- **Polish**: Requires all implemented stories; completeness cannot be claimed while dataset,
  participant, native-host or device evidence remains unavailable.

### Within Each User Story

- Test wave first, then entities/adapters, orchestration/handlers, UI integration and acceptance.
- Each implementation task must run its paired tests plus new unit tests for uncovered behavior.
- T027 -> T028–T029 -> T033 is the real data/model execution chain; T030–T032 provide native parity
  dependencies for T033. Exporting a model and checking compatibility are different deliverables.
- T036–T037 -> T038–T039 -> T040–T043 -> T044 is US1's speech/API/UI integration chain.
- T048–T050 -> T051/T054 and T052 -> T053 -> T055 precede US2 integration/acceptance.
- T059–T061 -> T062–T067 -> T068–T070 precede complete accessible-control acceptance.
- T071/T072 -> T073–T078 -> T079–T081 -> T082 precede research acceptance.

### Parallel Opportunities

`[P]` means only the named wave's tasks are safe to run concurrently after its prerequisites:

| Wave | Prerequisite | Disjoint tasks |
|------|--------------|----------------|
| Setup packages/provenance | T001–T002 | T003, T004, T005 |
| Foundation tests | Phase1 complete | T009, T010 |
| US1 contract/test design | Phase2 complete | T021, T022, T023, T024 |
| US2 policy/video test design | US1 complete | T045, T046, T047 |
| US3 controls/test design | US2 complete | T059, T060, T061 |
| US4 review/metrics test design | US3 complete | T071, T072 |
| Cross-cutting integration tests | US4 complete | T083, T084 |

Do not run tasks sharing configuration, composition roots or controllers in parallel merely because
they belong to different stories. Test implementation can wait for a missing dependency, but its
acceptance must not be marked complete until it executes and exhibits the intended failure/pass.

## Parallel Example: User Story 1

After foundations, author T021 HTTP contracts, T022 Python handoff tests, T023 native recognition
boundaries and T024 browser journey tests together; each writes its own named test file. Integrate
interfaces before running implementation-dependent tests. Then proceed with T025 onward.

## Parallel Example: User Story 2

After US1, author T045 tracker tests, T046 scheduler tests and T047 uploaded-video contract tests
together. Each has distinct files and controlled time/observations. Integrate T048–T055 in order;
shared scheduler/orchestration edits are not parallel tasks.

## Parallel Example: User Story 3

After US2, author T059 server controls, T060 browser playback races and T061 keyboard/recovery tests
together. T062–T067 implementation then proceeds in order to avoid conflicting session-state edits.

## Parallel Example: User Story 4

After US3, author T071 result/history component tests and T072 evaluator arithmetic tests together.
They have no shared production-file edits. Research evaluation and participant work may only run
once their actual input/model/device prerequisites are present.

## Requirement Coverage

| Requirement/outcome | Principal task coverage |
|---------------------|-------------------------|
| FR-001 input modes | T030, T039–T041, T052–T056 |
| FR-002 catalogue | T025–T029, T031–T033 |
| FR-003 OCR | T023, T034–T035, T050 |
| FR-004 validation/consolidation | T017, T031, T035, T045–T050 |
| FR-005 identities | T045, T048–T049, T054, T057 |
| FR-006 text changes/repetition | T046, T050–T051 |
| FR-007 speech scheduling | T036–T037, T046, T051 |
| FR-008 audio controls | T059–T065 |
| FR-009 accessibility | T024, T043, T061, T065–T070 |
| FR-010 optional results/history | T071, T073–T075 |
| FR-011 input/connection errors | T015–T020, T021, T030, T039–T042, T061, T066–T067 |
| FR-012 cancellation | T013–T016, T037–T042, T047, T053, T056, T059–T067, T084 |
| FR-013 one-session demo | T009, T013, T015, T020, T068, T083 |
| FR-014 privacy | T001, T010, T014–T020, T027, T067, T076, T078, T083 |
| FR-015 training/provenance | T004–T005, T022, T025–T029, T032–T033 |
| FR-016 evaluation | T072, T076–T082 |
| FR-017 no inferred navigation | T025, T035, T044, T088 |
| SC-001 reference scenarios | T044, T058, T070, T082 |
| SC-002 accessible tasks | T061, T068–T070 |
| SC-003 repeats/overlap/stop | T046, T056–T060, T064, T078–T079, T084 |
| SC-004, SC-005, SC-006 detection/OCR/tracking scores | T072, T076–T079 |
| SC-007 runtime performance | T057, T078, T080 |
| SC-008 intended-user study | T081 |
| SC-009 robustness breakdown | T027, T072, T077, T079 |
| SC-010 lifecycle/privacy | T009–T010, T067, T073–T075, T083 |

## Implementation Strategy

### MVP First (User Story 1)

Complete setup/foundations, then US1 with a real custom model. Use uploaded images as the first
end-to-end integration milestone, then finish US1's camera path. This MVP demonstrates recognition
and speech; it is not the complete assistive prototype until US2 and US3 P1 behavior is validated.
If real dataset/model inputs are unavailable, continue independent tooling/tests and explicitly
retain the data/training/real-recognition tasks as incomplete; do not downgrade the acceptance gate.

### Incremental Delivery

1. Setup/foundations establish deterministic contracts, ownership and delivery.
2. US1 delivers recognisable image/camera information and native speech.
3. US2 adds video, stable identities, text consensus and bounded relevant announcements.
4. US3 completes accessible control/recovery and real-device evidence.
5. US4 adds optional review and actual research outcomes.
6. Final validation exercises packaging, memory safety, isolation and documented run instructions.

### Parallel Team Strategy

Use only the disjoint test/setup waves above by default. If multiple implementers are assigned,
allocate ownership by domain and serialize composition-root/API changes. Data collection and
participant recruitment are external activities that require genuine access/consent; no agent can
complete them by inventing evidence. No parallel execution is launched by generating this file.

## Notes

- Every task remains unchecked until its implementation or evidence deliverable is verified.
- This task list does not authorize commits, application deployment, participant contact or
  collection of data without permission. Preparing local code/tests/docs is distinct from outreach.
- Do not modify the constitution or checked requirements checklist as a shortcut around failures.
- Numerical model quality remains a research report; functional behavior and privacy are test gates.
