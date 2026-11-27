---
description: "Dependency-ordered implementation tasks for the accessible sign recognition web prototype"
---

# Tasks: Accessible Sign Recognition Web Prototype

**Input**: Design documents from `specs/001-assistive-sign-recognition/`.
**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md),
[data-model.md](data-model.md), [API contract](contracts/api.md),
[model contract](contracts/model-bundle.md), [UI contract](contracts/ui.md),
[model promotion](contracts/model-promotion.md), [runtime telemetry](contracts/telemetry.md),
[quickstart.md](quickstart.md), and [Azure delivery design](azure-deployment.md).

**Tests**: Required by the specification's acceptance scenarios and constitution. Write the listed
behavioral/contract tests before their implementation, demonstrate the expected failure, then make
them pass. Add edge-case unit tests alongside each behavioral implementation task; test fixtures
must not become production fallback implementations. Checked tasks mean completed with evidence,
not merely drafted code. Task checkboxes below record verified completion.

**Organization**: Setup, common foundations, one phase per user story in specification order,
then Azure delivery and cross-cutting validation. This regeneration preserves T001–T100 so existing
plan references remain valid; it does not execute application or infrastructure implementation. The user has requested frequent rollback commits: commit coherent changes after their
relevant checks pass, stage explicit paths, and leave the constitution uncommitted.

**Task totals**: 100 tasks: setup 8, foundations 12, US1 24, US2 14, US3 12,
US4 12, Azure delivery 12, final validation 6. Eighteen tasks form seven disjoint parallel waves.

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

- [X] T001 Create scoped artifact exclusions in `.gitignore` and `.dockerignore` for `.venv/`, build outputs, secrets, `models/`, `research-data/`, and `reports/`; retain authorised small fixtures and manifests deliberately, without hiding source files or modifying the constitution.
- [X] T002 Establish C++20/GCC13, CMake/Ninja, pinned native dependency fetching and source checksums in `CMakeLists.txt`, `CMakePresets.json`, and `cmake/Dependencies.cmake`; include OpenCV/FFmpeg, ORT CPU, Drogon, Eigen, ByteTrack tracker sources, Tesseract, eSpeak NG and GoogleTest; define separate dev, ASan+UBSan and TSan presets. Verified on native Linux arm64: dependency build and each 4-test suite pass; amd64 dev smoke passes, while native amd64 sanitizer execution remains a deployment validation requirement. See `docs/native-build.md`.
- [X] T003 [P] Initialise the strict TypeScript/React/Vite frontend and locked Node22 dependencies in `frontend/package.json`, `frontend/package-lock.json`, `frontend/tsconfig.json`, and `frontend/vite.config.ts`; provide lint, typecheck, test, build and test:e2e commands without a deployed Vite server.
- [X] T004 [P] Initialise the typed Python3.12 training/evaluation package and immutable dependency lock in `training/pyproject.toml`, `training/requirements.lock`, and `training/road_sign_training/__init__.py`; include PyTorch/Ultralytics, ONNX tooling, pytest and metric dependencies without adding Python to the runtime backend.
- [X] T005 Record dependency versions/checksums and upstream code/model/data licence notices in `config/dependency-manifest.json` and `docs/dependency-provenance.md`; include the ByteTrack source commit, English OCR/voice data and Ultralytics/eSpeak terms without selecting a new repository licence. Wait for T003 and T004 to finish dependency resolution before collecting their hashes.
- [X] T006 Configure naming, formatting, lint and type checks in `.clang-format`, `.clang-tidy`, `frontend/eslint.config.js`, and `training/pyproject.toml`; preserve PascalCase C++/TS files/types, camelCase C++/TS values, snake_case Python and strict annotations without suppressions. Checked with LLVM18.1.3 format/tidy, ESLint10, Prettier3.10.1, Ruff and strict mypy; ARM64 native tests remain passing.
- [X] T007 Add native module targets with private include paths and acyclic public dependencies in `backend/CMakeLists.txt` and `backend/tests/ModuleBoundariesTest.cmake`; test that private-domain/infrastructure imports between modules fail instead of exposing all backend includes globally.
- [X] T008 Configure deterministic test entry points in `frontend/vitest.config.ts`, `frontend/playwright.config.ts`, `backend/tests/TestSupport.hpp`, and `training/tests/conftest.py`; provide controllable clocks/worker barriers and test-only observation/audio fixtures. Create `.github/workflows/ci.yml` for the available native/Python/TS checks on PR/main, with pinned actions, read-only permissions and no cloud credentials; add each suite to CI in the task that introduces it, report unavailable suites explicitly, and keep release eligibility false until the full planned checks exist and pass. Verified frontend lint/typecheck/Vitest/build/format, Python Ruff/format/mypy, and native ARM64 CTest 5/5. Audit was unavailable offline; backend/browser/model suites remain explicitly deferred and release eligibility stays false.

**Checkpoint**: Dependency manifests, language checks, test entry points and initial credential-free CI work; no trained model
or dataset is assumed present. T003–T004 form one parallel wave after T002; T005 waits for both locks before recording hashes.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared lifecycle, ownership, protocol and deployment needed by all stories.
**Gate**: Finish this phase before story implementation; test-only collaborators may substitute for
unfinished leaf modules, but production readiness remains false until the real bundle loads.

- [X] T009 [P] Write lifecycle/authentication contract tests in `backend/modules/recognition-session/tests/SessionFoundationTest.cpp` for atomic one-session admission, cookie/Origin/CSRF checks, 10-second SSE attachment, 5-second heartbeat/15-second lease, late callbacks, cleanup and second-client isolation.
- [X] T010 [P] Write ownership, resource-policy and error-redaction tests in `backend/tests/integration/RuntimeFoundationTest.cpp` for retained frame backing storage, bounded worker queues, finite geometry, no raw text/token logs and no persistent session spools.
- [X] T011 Define shared immutable contracts in `backend/shared/Frame.hpp`, `backend/shared/Geometry.hpp`, and `backend/shared/Identity.hpp`; reject invalid dimensions/arithmetic before allocation and provide copy/move lifetime tests. Frame constraints (verbatim): "frameId uint64 monotonic per epoch, sourceTimeMs finite nonnegative, clientCaptureTimeMs optional, width/height, owned pixel bytes"; "One active + one latest pending; clones or retained immutable backing storage, never a borrowed capture buffer". `Frame::copyOf` validates dimensions/timestamps and clones pixels; `FrameSequence` enforces increasing IDs per nondecreasing epoch. ARM64 CTest 6/6 passed.
- [X] T012 Define module public interfaces in `backend/modules/media-processing/public/MediaSource.hpp`, `backend/modules/sign-recognition/public/SignObservation.hpp`, `backend/modules/text-extraction/public/ExtractTextRequest.hpp`, `backend/modules/announcements/public/AcceptedInformation.hpp`, and `backend/modules/speech/public/AudioArtifact.hpp`; use the owning entity constraints below, retain immutable pixel ownership across calls and keep library handles private. SignObservation constraints (verbatim): "frameId, category, confidence [0,1], box, raw text candidate, OCR confidence, source time"; "Bound to frame/epoch; box finite xywh in upright original pixels, clipped before ROI access". All public headers compile in the ARM64 target; confidence, epoch, source time and clipped geometry are covered by the foundation test.
- [X] T013 Implement the session entity and serialised lifecycle in `backend/modules/recognition-session/domain/RecognitionSession.hpp` and `backend/modules/recognition-session/application/SessionService.cpp`; implement created/awaitingInput/processing/reviewing/ending/ended, snapshot the validated upload-attempt setting and release the lease only after cleanup. Only a completely received, validated and committed MediaSource consumes the successful-source slot; failed admitted uploads release reservations and remain retryable within budget. RecognitionSession constraints (verbatim): "sessionId UUID, sourceKind camera/image/video, epoch uint32 starting 1, state, stateRevision monotonic uint64, csrfToken random secret, audioGeneration uint32, isMuted bool, heartbeat deadline, uploadAttemptsUsed uint32 starting 0, maxUploadAttempts uint32 0–100 (0 unlimited), hasAcceptedUpload bool starting false"; "One global active lease; owns all runtime objects; secret only returned to owning client". Own UploadReceipt with the session and advance stateRevision on receipt changes even if lifecycle state is unchanged. Tests cover admission serialization, origin/CSRF, attachment timing, cleanup-before-reuse, late callbacks, retryable failure, attempt exhaustion, accepted-source lock, and revision changes; ARM64 CTest 8/8 passed.
- [X] T014 Implement owned vision/speech worker handoff and cooperative cancellation in `backend/modules/recognition-session/infrastructure/SessionExecutor.cpp`; tag work by session/epoch/generation, discard stale completions before publication, cap vision work at one active plus one latest pending, and never detach workers referencing destroyed state. Frame tests cover pending replacement and dropped IDs; vision and speech tests cover stale publication fencing and stop requests after identity changes. Destruction joins both workers. ARM64 CTest 8/8 passed.
- [X] T015 Implement JSON validation, fixed safe error codes, cookie/Origin/CSRF ownership, POST/DELETE sessions and GET health/ready/capabilities handlers in `backend/modules/recognition-session/infrastructure/SessionController.cpp`; follow every status/no-store rule in `contracts/api.md`, reject unknown fields, do not reveal another session and return unavailable until model verification exists. Return stateRevision explicitly in creation and ending responses and expose the effective attempt limit without private session metadata.
- [X] T016 Implement the ordered single-attachment SSE stream and heartbeat endpoint in `backend/modules/recognition-session/infrastructure/SessionEventsController.cpp`; emit session/epoch/stateRevision/eventSeq, 64-KiB maximum events, latest-result coalescing, priority cancel/end messages and no replay IDs; expire undrained streams/leases and test state-before-result ordering.
- [X] T017 Implement validated environment/policy loading and redacted logs in `backend/infrastructure/RuntimeConfig.cpp`, `backend/infrastructure/Logger.cpp`, and `config/policy.json`; encode all fixed policies from `research.md`, monotonic clocks and resource ceilings, with tests that model-supplied policies cannot enlarge hard media/storage limits. Make MAX_UPLOAD_ATTEMPTS an explicit environment edit point with default3 in config/policy.json, integer range0–100 and 0=unlimited; snapshot it per session. Validate RUNTIME_METRICS_ENABLED (false locally, true in Azure); this enables only the aggregate exporter in telemetry.md, never session-content logging.
- [X] T018 Implement the application composition root and session cleanup hooks in `backend/app/Main.cpp`; inject module public interfaces, establish graceful shutdown and RAII cleanup of request spools/buffers, and ensure a readiness failure does not instantiate fake inference/OCR/TTS behavior.
- [X] T019 Implement pre-body upload reservation and validation in `backend/modules/recognition-session/infrastructure/MediaAdmissionController.cpp`; expose `/internal/admit-media` privately, reserve one upload atomically, validate original request ownership, return an internal nonce, reject a second upload before spooling and enforce 60-second body/65-second admission cleanup deadlines. Count only granted admission attempts, never consume successful-upload ownership on failure, release partial-spool reservations before retries and return UPLOAD_ATTEMPTS_EXHAUSTED at a finite budget. Add `backend/modules/recognition-session/domain/UploadReceipt.hpp` and public GET upload-status; correlate X-Upload-Attempt-Id with the nonce, atomically expose the snapshot and emit upload.state on admission/validation/terminal cleanup. Preserve only the current/latest receipt and reject its duplicate submission before admission. UploadReceipt constraints (verbatim): "uploadAttemptId UUID\|null, uploadStatus idle/receiving/validating/accepted/rejected/aborted, errorCode fixed safe code\|null"; "Session owns only current/latest admitted receipt; ID null only for idle; errorCode non-null only for rejected/aborted; terminal failure published after spool/reservation cleanup".
- [X] T020 Provide tested multi-stage `Dockerfile`, `compose.yaml`, `config/Caddyfile`, and `config/demo.env.example`; build backend-runtime/web-runtime targets, proxy same-origin HTTPS with trusted internal CA, protect the private admission route, use read-only non-root backend with 4-GiB memory/256-MiB tmpfs and no core dumps, cap multipart bodies at 101 MiB and memory spooling at 10 MiB, persist only CA state, and document host/VM swap verification. Add both image builds and a missing-bundle/not-ready Compose smoke test to `.github/workflows/ci.yml`; add the real-model readiness/recognition suite with T033 and execute it in the trusted T091 publication gate, never in credential-free fork CI and never with a generic fallback.

T009–T020 foundation verification: ARM64 native CTest 9/9, rebuilt Compose HTTPS lifecycle/upload/expiry smoke, internal-CA certificate check and container-limit inspection passed. The live smoke covers wrong-cookie isolation, 5-second SSE heartbeats, 15-second lease expiry, empty session tmpfs and absence of a media canary in backend logs. T016's output buffer has native coalescing/priority/boundary tests; T018's cleanup test proves an accepted temporary file is unlinked on shutdown. Real leaf adapters are wired as their modules are implemented. The real-model suite and trusted publication evidence remain assigned to T033/T091; these checkboxes do not make a release eligible.

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

- [ ] T021 [P] [US1] Write image/camera/media/audio HTTP contract tests in `backend/tests/integration/ImageRecognitionContractTest.cpp` covering input modes, limits, accepted/uncertain results, no-result status, WAV ownership, missing bundle readiness and known-error responses. Include failed-first/valid-second uploads, one accepted source, budget exhaustion/reset, 0-unlimited behavior and stateRevision response assertions. Cover lost 202/rejection/abort replies, terminal upload events without lifecycle changes, stale snapshots, duplicate latest attempt IDs, unknown delayed requests and end/start invalidation. Assert automatic audio source IDs are non-null and repeat/recovery source IDs are null with no capture timestamp.
- [ ] T022 [P] [US1] Write dataset/export/parity tests in `training/tests/test_model_handoff.py` for provenance, group leakage, class order, manifest hashes, exact tensors/preprocessing, trained-framework/export/native agreement and failure on incompatible models. Add candidate inventory/hash/size/shape/path tests in `training/tests/test_model_candidate.py`; reject undeclared files, unsafe references and missing framework/parity evidence.
- [ ] T023 [P] [US1] Write detection/crop/OCR/speech boundary tests in `backend/tests/integration/NativeRecognitionTest.cpp` for non-square images, partial-edge boxes, letterboxing, finite scores, unreadable text, PCM ownership and unavailable native resources.
- [ ] T024 [P] [US1] Write source selection, permission, single-frame transfer and generated-audio playback tests in `frontend/tests/e2e/HearSignInformation.spec.ts`; use deterministic test-only fixtures and assert no microphone permission, visual dependency or browser speech synthesis. Add lost-response reconciliation tests: matching accepted means no resend; cleaned-up failure allows a new UUID; receiving waits; unmatched/idle uncertainty requires end/start; poll at most 1 Hz for 65 seconds and never hide Stop.

### Implementation for User Story 1

#### Revised catalogue and obstacle prerequisites

The following tasks amend the earlier T025/T026 results without erasing their historical evidence.
T101–T103 precede T027–T029 and T031–T033; T104 precedes the dataset import in T105, and T105
precedes T027/T033 acceptance. T106–T107 precede T038/T044; continuous behavior is T109 in US2,
after T048–T050 and before T054/T058. Keep existing user-provided files untouched until reviewed.

- [X] T101 [US1] Revise `resources/catalogue.json` and `docs/dataset-annotation.md` to match `contracts/model-bundle.md`: preserve IDs 0–4 and 6, set ID 5 to shared-path-with-cyclist and ID 7 to fixed street-furniture obstacles, with text relevance false and priorities 1 and 0 respectively; define inclusion/exclusion, UK sign examples, obstacles and ambiguous/negative annotations. Update `training/tests/test_model_handoff.py` for all eight ordered names, text flags and priorities.
- [X] T102 [US1] Update `training/road_sign_training/dataset_manifest.py`, `training/road_sign_training/validate_dataset.py` and `training/tests/test_dataset_manifest.py` for the revised class meanings and text/track rules; verify class 5 and 7 do not require OCR, old mappings cannot silently pass as the revised catalogue, group isolation and class coverage still hold, and user data is not modified by validation.
- [X] T103 [US1] Align the eight class priorities in `config/policy.json` and `backend/infrastructure/RuntimeConfig.cpp` with the revised catalogue. Complete this task when `backend/tests/integration/RuntimeFoundationTest.cpp` accepts `[0,0,0,0,2,1,2,0]` and rejects an incorrect priority mapping.
- [X] T104 [US1] Audit `training/road_sign_training/dataset/`, `training/road_sign_training/get_data.py` and the proposed ROD dataset for exact revision, per-source licence, permissions, source/location grouping, annotations, UK relevance and existing splits; record a source/class inclusion table and unresolved provenance in `reports/dataset-readiness.md`, excluding data without verified rights. Preserve existing files and never commit raw media.
- [X] T105 [US1] Implement a reproducible reviewed ROD subset import in `training/road_sign_training/import_obstacles.py` with tests in `training/tests/test_obstacle_import.py`; map approved poles, bins, benches, cones, barriers and similar fixed furniture to class 7, preserve box geometry and per-image provenance, verify hashes and source rights, flag partially labelled images for review, avoid source/location leakage, and leave unavailable data blocked in `reports/dataset-readiness.md`. Do not overwrite original data or treat dropped labels as confirmed background.
- [ ] T106 [US1] Add obstacle-position and announcement tests in `backend/modules/announcements/tests/ObstacleAnnouncementTest.cpp` for clipped box-centre thirds and exact boundaries (left `[0,1/3)`, centre `[1/3,2/3)`, right `[2/3,1]`), invalid geometry, uncertainty and priority 0; assert spoken wording says left/centre/right of the camera view and makes no distance, route or safety claim. Include continuous same-track position-change tests for T109.
- [X] T107 [US1] Implement still-image accepted-obstacle descriptions in `backend/modules/announcements/domain/AcceptImageInformationUseCase.cpp`; derive camera-view position from the clipped box centre using the boundaries in T106, apply still-image acceptance, use priority 0, and never infer distance, collision or a safe path. Position constraint (verbatim): "left/centre/right of camera view, derived from clipped box centre in equal-width image thirds"; "Observation only; never physical distance or safe route; boundaries and position-only revision policy in plan.md".

- [X] T025 [US1] Define the eight-entry catalogue in `resources/catalogue.json` and annotation rules in `docs/dataset-annotation.md`; preserve exact model-contract class order and symbol/text boundaries, including negative/unsupported examples and no inferred navigation instructions. CatalogueEntry constraints (verbatim): "classId 0–7, category key, spokenName, isTextRelevant, priority, included/excluded variants"; "Read-only versioned catalogue; exact order in model-bundle.md". Catalogue order/fields checked by `training/tests/test_model_handoff.py`.
- [X] T026 [US1] Implement dataset manifest types/validation in `training/road_sign_training/dataset_manifest.py` and `training/road_sign_training/validate_dataset.py`; enforce typed boxes/transcripts/track IDs, grouped 70/15/15 split with seed42, coverage checks and train-only augmentation. DatasetManifest constraints (verbatim): "datasetId/version, authorised sources and usage terms, scene/location group, split, annotations, condition tags, checksums"; "Offline research only; group-separated deterministic splits". Local Python lint, strict typing and 12 tests passed; the training CI job now runs these tests.
- [ ] T027 [US1] Acquire or collect authorised UK scenes and annotate images/video for all eight categories in `research-data/uk-signs-v1/manifest.json`; record permissions, source/location grouping, transcripts, physical track IDs and robustness tags, freeze held-out splits and include negatives; if source data/access is unavailable, record the actual blocker in `reports/dataset-readiness.md` and leave this task incomplete instead of fabricating data or using demo sessions.
- [ ] T028 [US1] Implement reproducible custom YOLO11n training in `training/road_sign_training/train.py` and `training/config/train.yaml`; explicitly load the named checkpoint, record seeds/toolchain/config and dataset hashes, tune only on validation data and write the documented `reports/training-v1/best.pt` output.
- [ ] T029 [US1] Implement export and bundle verification in `training/road_sign_training/export.py` and `training/road_sign_training/model_manifest.py`; emit manifest/catalogue/policy/parity metadata, FP32 `[1,3,640,640]` input and `[1,12,8400]` output, opset17 and nms=false, with no generic fallback. Enforce every required manifest field in `contracts/model-bundle.md`, including schemaVersion, split hashes, catalogue hash, provenance and parity evidence. ModelManifest constraints (verbatim): "modelId, model hash, class order, exporter/runtime versions, shapes, preprocessing/NMS, parity results"; "Immutable model-bundle.md contract; mounted read-only". Implement `training/road_sign_training/package_candidate.py` per `contracts/model-promotion.md`, producing the allowlisted regular-file OCI candidate inventory/package manifest and enforcing file-count/size/provenance limits; no archives, scripts, training pickle checkpoints or demo media. In `training/tests/test_model_handoff.py`, verify that export validation accepts the revised catalogue and rejects the previous class order even when its hash is internally consistent and its tensor shape matches.
- [X] T030 [US1] Implement JPEG/PNG/frame decoding and MediaSource validation in `backend/modules/media-processing/infrastructure/OpenCvImageClient.cpp`; enforce 1-MiB/1280x720 camera and 10-MiB/20-million-pixel image limits, orientation and finite bounds before ROI/allocation; validate the generated tmpfs handle and remove partial uploads on rejection. MediaSource constraints (verbatim): "sourceKind, original dimensions, durationMs for video, current mediaTimeMs, tmpfs file handle for uploads"; "Owned by session; generated filename, never user path; validated limits in research.md". ARM64 CTest 10/10 and focused ASan/UBSan 5/5 passed; T023's other native boundaries remain open.
- [ ] T031 [US1] Implement ORT CPU bundle loading, tensor validation, shared explicit letterboxing/normalisation and class-aware NMS in `backend/modules/sign-recognition/infrastructure/OnnxDetectionClient.cpp`; use the model contract, allow negative top-left coordinates before clipping, bound runtime candidates to100 and expose no private ORT/OpenCV handles across modules. In `backend/tests/integration/NativeRecognitionTest.cpp`, verify that the native loader rejects a bundle advertising the previous catalogue before enabling inference, even with matching dimensions and internally consistent hashes.
- [ ] T032 [US1] Implement native parity inference mode in `backend/app/EvaluateMain.cpp` and Python comparison in `training/road_sign_training/check_parity.py`; verify trained-framework versus export and native outputs with atol1e-4/rtol1e-3, identical fixture labels and restored boxes within1 pixel, including explicit threshold-boundary expectations. Save authorised fixture-associated trained-framework FP32 reference tensors and local parity hashes during this real comparison; candidate CI can validate ONNX/native output against these numerical references without executing a training checkpoint.
- [ ] T033 [US1] Run dataset validation, custom training, export and native parity after T027–T032; produce `models/signs-v1/manifest.json`, `models/signs-v1/parity.json`, and `reports/training-v1/run.json` with real hashes/results, recording missing data/compute as unfinished work rather than marking model acceptance complete. Produce `models/candidates/signs-v1/package-manifest.json` and its verified authorised fixture/reference inventory for later operator upload; package creation requires no Azure access.
- [ ] T034 [US1] Implement worker-owned English Tesseract extraction in `backend/modules/text-extraction/infrastructure/TesseractTextClient.cpp`; enforce crop minimum16x16/longest-edge1600, appropriate segmentation and word-confidence>=70, Unicode normalisation and correct RAII deletion of returned text; test unreadable/empty/multiline signs and never invent missing words.
- [ ] T035 [US1] Implement still-image accepted-information rules in `backend/modules/announcements/domain/AcceptImageInformationUseCase.cpp`; require score>=0.75 for stills and accepted OCR confidence, mark truncated descriptions as partial and quote observed information without route/crossing decisions. Store `backend/modules/announcements/domain/LatestAcceptedDescription.hpp` independently of started speech; use a fresh descriptionId per accepted result, choose the highest-priority description for a batch, and clear it on seek/end/replacement. AcceptedInformation constraints (verbatim): "trackId, revision uint32, category, displayText, normalizedText, priority, capture reference"; "Update only after consensus; formatting/case-only text changes do not advance revision". LatestAcceptedDescription constraints (verbatim): "descriptionId UUID, epoch, trackId, revision, text <=500 characters, acceptedAtMs"; "One current-epoch accepted description independent of started speech; cleared on seek/end".
- [ ] T036 [US1] Implement serialised retrieval-mode eSpeak NG and owned WAV output in `backend/modules/speech/infrastructure/EspeakSpeechClient.cpp`; use en-gb/160 words per minute and <=500-character input, with cancellation-safe callbacks and no server sound-device output. AudioArtifact constraints (verbatim): "announcementId, WAV bytes, sampleRateHz, durationMs, epoch, audioGeneration"; "Generated by native worker; <=45 seconds; released at completion/cancellation/end".
- [ ] T037 [US1] Implement the initial single-offer speech lifecycle in `backend/modules/announcements/application/AnnouncementService.cpp` and the receipt in `backend/modules/announcements/domain/CancellationReceipt.hpp`; suppress duplicate pending/synthesizing/offered work, enforce five-second automatic offer expiry and started/ended/failed/blocked handling. Before reuse, fence all artifacts through the receipt cutoff and require its distinct acknowledgment; support supersession/latest-completed idempotency and terminate after five seconds without acknowledgment. Ordinary stale playback results cannot clear a barrier. Announcement constraints (verbatim): "announcementId UUID, trackId string\|null, revision uint32\|null, frameId uint64\|null, clientCaptureTimeMs optional, text <=500 characters, priority, isReplay, origin automatic/repeat/recovery, epoch, audioGeneration, created/expiry times, state"; "At most 3 pending + one offered/playing; automatic origin requires non-null trackId/revision/frameId and uses signature (trackId, revision); manual origins require null source IDs, omit capture time and bypass automatic signature accounting". CancellationReceipt constraints (verbatim): "cancellationId UUID, cancelThroughAudioGeneration uint32, deadline, state pending/acknowledged"; "One pending plus latest acknowledged receipt; matched independently of current epoch/generation; no text/audio". Copy only scalar source metadata into automatic offers. Repeat/recovery has null trackId/revision/frameId and no clientCaptureTimeMs; automatic signature accounting excludes manual origins. Bound each new manual offer to five seconds and completion to duration+5 seconds.
- [ ] T038 [US1] Wire the image/camera pipeline in `backend/modules/recognition-session/application/RecogniseFrameUseCase.cpp`; compose media, inference, OCR, acceptance and speech only through public interfaces, fence late callbacks, produce no-result image status and leave completed image results reviewable.
- [ ] T039 [US1] Implement frame/media and audio/playback handlers in `backend/modules/recognition-session/infrastructure/RecognitionController.cpp`; use admission nonces for uploads, correct SSE result/audio schemas, session-owned WAV fetch with residual-validity header, safe error statuses and no-store; unsupported video operation remains explicit until US2 implements decoding. Publish description.latest and origin in audio.ready; implement the separate cancellationId acknowledgment endpoint with old-target acceptance, supersession and idempotency per api.md.
- [ ] T040 [US1] Implement typed session API/EventSource client and revision-aware reducer in `frontend/src/modules/recognition-session/SessionClient.ts` and `frontend/src/modules/recognition-session/SessionStore.ts`; reject stale messages, keep secrets only in memory, close EventSource on error, send heartbeats and require explicit restart. Read UploadStatusSnapshot through GET upload-status and upload.state with revision reconciliation; never infer receipt state from session.state alone.
- [ ] T041 [US1] Implement image upload and bounded camera capture in `frontend/src/modules/media-input/MediaInput.tsx` and `frontend/src/modules/media-input/CameraCaptureService.ts`; request rear-facing camera as preference, capture JPEG quality0.8 at<=5Hz with one outstanding POST, skip stale frames, show limits and attempt counters, permit explicit retry after a rejected upload within the session budget, reconcile uncertain acceptance before another request, and release tracks on stop/failure/hidden page. Assign each explicit transfer a UUID and follow the matching receipt rules; do not resend an uncertain attempt or enable another upload from an unrelated/idle snapshot.
- [ ] T042 [US1] Implement native-generated audio playback in `frontend/src/modules/audio-feedback/AudioPlaybackService.ts`; activate AudioContext on user gesture, fetch/decode one WAV, subtract elapsed fetch/decode time from residual validity, validate epoch/generation again before starting, send playback acknowledgments and implement the separate cancellation receipt acknowledgment after local fetch/decode/playback cancellation; process it before stale-epoch filtering and cancel locally before waiting for network cleanup. Validate the origin-discriminated audio.ready shape; accept manual null source fields without dereferencing tracks/frames.
- [ ] T043 [US1] Compose the initial labelled source/start/stop/status interface in `frontend/src/modules/recognition-session/RecognitionPage.tsx` and `frontend/src/App.tsx`; expose privacy and unavailable-model status, provide a safe stop action and optional non-live result text, and do not present US2/US3 controls as implemented until wired.
- [ ] T044 [US1] Execute the uploaded-image subset of US1 scenarios with the verified bundle and save sanitised evidence in `reports/acceptance/us1-image.md`; run image-relevant T021–T024 cases and module unit tests, record camera cases as pending T048–T054/T058, and do not advertise full US1 completion at this checkpoint.
- [ ] T108 [US1] After T033 and T044, evaluate the revised eight classes on authorised held-out UK still images. Record source permissions, split and model hashes, per-class errors, lighting conditions and obstacle position results at the image-third boundaries in `reports/acceptance/us1-image.md`. Close T108 on this image evidence.

**Checkpoint**: The US1 uploaded-image slice is independently testable. Full US1 camera acceptance
is deliberately deferred to T058 after temporal consensus exists; capabilities must keep live
recognition unavailable until T054 integrates that policy. T033/T044 cannot pass on fake weights.
The complete P1 experience additionally requires US3 controls and recovery.

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

- [X] T048 [US2] Implement TrackedSign and its independent histories in `backend/modules/sign-recognition/domain/TrackedSign.hpp`; implement tentative/confirmed/lost/removed states, geometry/motion/category association and two-source-second reacquisition without matching by text alone. TrackedSign constraints (verbatim): "trackId string composite of session/epoch/category/localId, first/last source times, state, last five detection observations, separate last three OCR samples within 2 source seconds, accepted text/category, revision"; "Session owns max 100 active/lost tracks; match by geometry/motion/category, never text alone".
- [X] T049 [US2] Adapt pinned tracker-only ByteTrack in `backend/modules/sign-recognition/infrastructure/ByteTrackClient.cpp`; use per-category trackers, elapsed source time in prediction/expiry, thresholds0.50/0.10/new0.60 and association-cost<=0.80, 2-second loss retention, bounded removed history and the no-reused-identity contract.
- [X] T050 [US2] Implement temporal acceptance and OCR scheduling in `backend/modules/announcements/domain/ConsolidateInformationUseCase.cpp`; confirm category>=0.60 in3-of-last5 observations within2s, schedule <=1 OCR/track/s and <=4 crops/frame, require two normalised matching OCR samples within2s, preserve accepted text on transient conflicts and advance revision only on substantive changes.
- [X] T109 [US2] Extend `backend/modules/announcements/domain/ConsolidateInformationUseCase.cpp` after T048–T050 and before T054/T058 to apply T106's camera-view position boundaries to accepted obstacles. Position-only changes on the same track do not advance accepted revision or trigger another automatic announcement; a new track follows existing announcement policy. Run the continuous tests in `backend/modules/announcements/tests/ObstacleAnnouncementTest.cpp`, including camera movement and repeated observations.
- [ ] T051 [US2] Extend the scheduler in `backend/modules/announcements/application/AnnouncementService.cpp` to enforce "At most 3 pending + one offered/playing", five-second freshness, category priorities/oldest-first ties and specified overflow; invalidate lost/obsolete pending results, retain started signatures per track/revision and never preempt playing speech merely for a higher priority.
- [ ] T052 [US2] Implement OpenCV/FFmpeg video decoding in `backend/modules/media-processing/infrastructure/OpenCvVideoClient.cpp`; accept only MP4/H.264 or WebM/VP8/VP9 <=100MiB/300s/1920x1080/60FPS, ignore input audio, validate timestamps/dimensions, process<=5Hz source-time samples and skip stale frames rather than growing queues.
- [ ] T053 [US2] Implement uploaded-video processing/review and seek orchestration in `backend/modules/recognition-session/application/ProcessVideoUseCase.cpp` and `backend/modules/recognition-session/infrastructure/VideoController.cpp`; increment epoch/stateRevision atomically before events, reset tracking/history/audio on seek, retain the validated tmpfs file and reject out-of-range or stale seek requests.
- [ ] T054 [US2] Integrate live-frame tracking and consensus into `backend/modules/recognition-session/application/RecogniseFrameUseCase.cpp`; propagate source times/track IDs/revisions, apply ROI limits and OCR scheduling, and preserve the US1 still-image acceptance path.
- [ ] T055 [US2] Add video upload, timestamped progress, EOF review and labelled seek in `frontend/src/modules/media-input/VideoInput.tsx`; coordinate seek through `frontend/src/modules/recognition-session/SessionClient.ts`, await authoritative epoch/stateRevision and clear old results/audio before resuming.
- [ ] T056 [US2] Add streaming scenario tests in `frontend/tests/e2e/TrackSigns.spec.ts` for upload/live sequences, early new-epoch SSE before HTTP replies, stale offered audio, seek during fetch and backend/frontend cancellation agreement.
- [ ] T057 [US2] Verify long-run tracking/queue limits with a deterministic ten-minute sequence in `backend/modules/sign-recognition/tests/TrackingSoakTest.cpp`; assert <=100 tracks, bounded removed/OCR histories, one active/one pending frame and no lingering ended-session references under sanitizers.
- [ ] T058 [US2] Run US2 independent scenarios and complete the deferred full US1 camera acceptance after T048–T054; record identities, automatic/manual repeats and cancellation outcomes in `reports/acceptance/us2.md` and camera evidence in `reports/acceptance/us1-camera.md`; regress the image slice, run the remaining T021–T024 camera cases, and distinguish fixture success from held-out quality. Include authorised held-out UK camera sequences in the camera report. Record permissions, split and model hashes, lighting and camera movement, position correctness, track continuity and the count of unwanted repeat announcements when the same obstacle crosses image thirds.

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

- [ ] T059 [P] [US3] Write backend audio-control tests in `backend/modules/recognition-session/tests/AudioControlsTest.cpp` for mute/generation changes, unmute without backlog, empty/muted repeat, blocked activation, stale acknowledgments and missing-cancel-ack session termination. Test acknowledgments for cutoff k after generation k+1, cancel-all behavior, latest completed receipt replay and unknown/superseded IDs, plus first-image recovery without any started history.
- [ ] T060 [P] [US3] Write state/playback race tests in `frontend/src/modules/audio-feedback/AudioPlaybackService.test.ts` for immediate mute/stop, rapid mute-unmute intent ordering, expired GET validity, superseded fetches and no playback-slot reuse before confirmed cancellation. Test Read latest result after initial audio rejection, stale descriptionId, coalesced recovery clicks and barrier acknowledgment independent of epoch filtering. Cover repeat/recovery after track removal and history truncation with null source IDs; verify finite manual-offer expiry and no accidental overlay/timing updates.
- [ ] T061 [P] [US3] Write keyboard/focus/status and recovery tests in `frontend/tests/e2e/AccessibleControls.spec.ts` for every control, hidden visuals, non-live transcripts, denial/revocation, malformed uploads, EventSource failure, hidden-page cleanup and no automatic resume.

### Implementation for User Story 3

- [ ] T062 [US3] Implement mute/unmute/enable/repeat/recover orchestration and endpoint in `backend/modules/recognition-session/application/ControlAudioUseCase.cpp` and `backend/modules/recognition-session/infrastructure/AudioControlsController.cpp`; advance stateRevision for accepted state mutations, advance audioGeneration when fencing audio and include stateRevision in every control response. Repeat targets last-started speech; recover targets the current descriptionId, rejects mute/missing/stale data, clears the blocked state and coalesces manual clicks with origin=repeat/recovery and isReplay=true. Wait behind any cancellation barrier; enable/unmute must not restore an automatic backlog.
- [ ] T063 [US3] Complete generation-safe control reconciliation in `frontend/src/modules/recognition-session/SessionStore.ts`; serialize seek/audio commands, accept newer authoritative session.state before late HTTP replies, ignore older revisions and preserve immediate local mute/stop intent across in-flight unmute responses. Retain description.latest for the current epoch and process cancellation receipts before stale-epoch filtering; clear latest accepted information on seek/end.
- [ ] T064 [US3] Complete audio recovery, repeat and cancellation acknowledgment in `frontend/src/modules/audio-feedback/AudioPlaybackService.ts`; handle blocked context, Enable audio, playback timeout/cancel and no backlog on unmute, and clear decoded buffers/object URLs on generation/session change. Use /cancellations/{cancellationId}/ack, not playback status=cancelled; acknowledge only after matching buffers/fetches/playback stop. Read latest result may recover an accepted description whose first playback never started. Support manual null source fields after the source track/frame has been released; keep latest text recovery independent of history and use the normal bounded offer timeout.
- [ ] T065 [US3] Add labelled native controls and accessible state/reason text in `frontend/src/modules/audio-feedback/AudioControls.tsx` and `frontend/src/modules/recognition-session/SessionControls.tsx`; implement the prescribed control order, visible focus and no-result/empty-repeat/muted behavior without hidden shortcuts or visual-only indications. Provide Read latest result independently from Repeat latest, show unavailable/muted reasons and remaining upload attempts; zero limit is labelled unlimited.
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

- [ ] T071 [P] [US4] Write result/history tests in `frontend/src/modules/results-review/ResultsReview.test.tsx` for matching frame/box/text, optional visuals, non-live accessible review, intentional-replay labels, truncation and session/epoch clearing. Test unavailable local video decoding/synchronisation produces accessible text/audio fallback and no thumbnail request. Exercise spec.md US4 acceptance scenario 6 with disabled local decoding/synchronization, and verify historical manual speech cannot move a current-frame overlay.
- [ ] T072 [P] [US4] Write evaluation arithmetic/schema tests in `training/tests/test_evaluation.py` using hand-computed detections, missing/unmatched OCR regions, ID switches and automatic/replay events; verify denominators, timing clock domains, condition counts and no implied success on absent evidence.

### Implementation for User Story 4

- [ ] T073 [US4] Implement bounded HistoryEntry and review snapshots in `backend/modules/announcements/domain/HistoryEntry.hpp` and `backend/modules/announcements/application/HistoryService.cpp`; retain only latest repeat text separately, label intentional replays and clear history on seek/end without persistence. HistoryEntry constraints (verbatim): "announcementId, text, isReplay, playback outcome, startedAt"; "Latest 200 only; accessible notice on truncation; cleared at session end".
- [ ] T074 [US4] Implement optional matching-frame overlays and review text in `frontend/src/modules/results-review/ResultsReview.tsx`; align boxes to sampled camera frame or video source timestamp, preserve accessible text when visuals are hidden and do not attach stale boxes to unrelated pixels. For unsupported local video rendering, disable overlays with an accessible explanation and keep text/audio review; do not implement a server preview-image endpoint. Treat unavailable local video rendering as the explicitly supported FR-010/US4 scenario 6 fallback across applicable browser tests.
- [ ] T075 [US4] Implement chronological announcement history in `frontend/src/modules/results-review/AnnouncementHistory.tsx`; derive entries from current-session playback events, label intentional replay/outcome, cap200 and show truncation, and clear on seek/end/new session without storage persistence.
- [ ] T076 [US4] Extend the offline native evaluator in `backend/app/EvaluateMain.cpp` to process authorised labelled media through the real public pipeline, export detections/OCR/identities/announcements and stage timings to the explicit output directory, and prevent the online server from writing offline research mounts.
- [ ] T077 [US4] Implement metric aggregation and EvaluationRun validation in `training/road_sign_training/evaluate.py` and `training/road_sign_training/evaluation_run.py`; compute ranked mAP at floor0.001/max300 before tracking, separate runtime P/R, end-to-end/matched CER/WER, IDF1/switches/fragmentation and replay-excluded duplicates with the quickstart definitions. Keep missing samples/device/study evidence explicit instead of assigning passing scores. EvaluationRun constraints (verbatim): "runId, dataset/model/config hashes, environment, sampled counts, metrics, failures, study summary"; "Offline report; user participants identified by study codes, not session media".
- [ ] T078 [US4] Implement explicit evaluation-only browser timing/counter export in `frontend/src/modules/recognition-session/EvaluationMetricsService.ts` and backend counters in `backend/infrastructure/EvaluationMetrics.cpp`; use client monotonic capture-to-result/playback timing, separate upload/source timing, record dropped/processed frames and queue/memory peaks, and exclude images, transcripts and audio from ordinary demo logs. Exclude repeat/recovery (null source IDs) from capture-to-result/speech latency samples; do not invent capture timestamps. Implement `backend/infrastructure/RuntimeMetrics.cpp`, `backend/infrastructure/RuntimeMetricsWriter.cpp`, and `backend/tests/integration/RuntimeMetricsTest.cpp` for telemetry.md fixed cumulative counters/histograms, process/sequence sample identity, 60-second cadence, 2-KiB lines and one pending snapshot. Keep this process aggregate stream distinct from evaluation exports; test boundary arithmetic, secret/schema rejection, full/stalled writer, resets and 16-MiB daily-file bounds without blocking recognition.
- [ ] T079 [US4] Run held-out recognition/OCR/tracking and condition-stratified evaluation, recording true samples, misses, false positives and provenance in `reports/evaluation-v1/metrics.json` and `reports/evaluation-v1/summary.md`; publish incomplete coverage honestly and never tune on this held-out run.
- [ ] T080 [US4] Run the ten-minute live-camera benchmark on the documented native reference host and record p50/p95 latency, actual FPS, start/end/peak process and container memory, dropped frames, queue high-water marks and environment hashes in `reports/performance/live-camera.json`; distinguish native/emulated measurements and ingestion cap from measured throughput.
- [ ] T081 [US4] Prepare consented participant tasks and collect the five-person blind/partially sighted usability evaluation in `docs/usability-study.md` and `reports/usability/study-summary.json`; record coded participant counts, task completion and 1–5 clarity/relevance/timing ratings, and leave recruitment/evidence incomplete if participants are unavailable rather than substituting developer ratings.
- [ ] T082 [US4] Run the independent review/evaluation scenarios and assemble evidence against SC-001–SC-010 in `reports/acceptance/us4.md`; link model/data hashes and prior-story evidence, distinguish functional pass/fail from research scores and explicitly list unperformed study/device/performance checks.

**Checkpoint**: Review and research reporting exist as a separate increment. T079–T082 require real
artifacts/evidence; executable metric tooling alone does not complete the research evaluation.

---

## Phase 7: Azure, Terraform and GitHub Actions

**Purpose**: Deliver the requested controlled Azure deployment with AZ-001–AZ-008 protections,
reproducible infrastructure and a reviewed release/rollback path. No resources are provisioned by
this planning update. Workflows must remain disabled for deployment until real prerequisites exist.

- [ ] T083 Write infrastructure plan assertions and synthetic privacy fixtures in `infra/tests/test_plan_policy.py`, `infra/tests/test_log_redaction.py`, and `infra/tests/fixtures/security-cases.json`; cover private origin/no public SSH, WAF Prevention/bot/rate/DDoS settings, identity scope, secret-free configuration, SSE buffering and upload limits. Assert unsafe plans and unredacted fixtures fail; offline fixtures do not substitute for live retained-log checks in T093. Validate the typed runtime-metrics table/DCR projection, exporter disk bounds, candidate registry/digest selection and fork isolation; include malformed candidate/telemetry cases without loading real models or contacting Azure.
- [ ] T084 Implement Terraform state/bootstrap in `infra/terraform/bootstrap/main.tf`, `infra/terraform/bootstrap/identities.tf`, `infra/terraform/bootstrap/versions.tf`, and `infra/terraform/bootstrap/backend.tf`; create private state/plan containers, Entra-only auth, locking/versioning/recovery, precreated runtime user-assigned identities and separate plan/publish/apply/deploy OIDC identities. Define constrained delegation and exact environment/repository trust, document operator bootstrap/state migration in `docs/azure-operations.md`, and pass offline validation; live bootstrap/locking/recovery evidence belongs to T093.
- [ ] T085 Implement the parameterized environment root in `infra/terraform/stack/main.tf`, `infra/terraform/stack/network.tf`, `infra/terraform/stack/compute.tf`, `infra/terraform/stack/variables.tf`, `infra/terraform/stack/versions.tf`, and `infra/terraform/stack/backend.tf`; provide non-secret `infra/terraform/environments/dev.tfvars` and `infra/terraform/environments/demo.tfvars`. Pin tested CLI/providers and commit each root lockfile, validate CIDRs/region/budgets, separate backend keys, and provide gateway/private VM subnets, platform-compatible NSGs, NAT egress and Ubuntu D4s_v5 without a public VM IP; pass T083 assertions without provisioning.
- [ ] T086 Add ACR, Key Vault and role bindings to the existing bootstrap-owned identities in `infra/terraform/stack/identity.tf`; do not recreate those identities. Disable registry admin login, enforce vault RBAC/soft-delete/purge protection and constrained pull/certificate permissions; reference certificate URIs without reading values into Terraform. Document import/renewal prerequisites in `docs/azure-operations.md`, test scope restrictions offline and leave live permission-denial evidence to T093.
- [ ] T087 Implement Application Gateway WAF_v2, DNS integration and DDoS IP Protection in `infra/terraform/stack/edge.tf`; enforce tester CIDR blocking, CRS/bot rules, method/path rate limits, request/file-size ceilings, origin TLS and disabled SSE response buffering per `azure-deployment.md`. Test safe/unsafe plans, including `is_gateway_enabled=false` for initial certificate bootstrap and reviewed enablement before acceptance; defer actual through-gateway checks to T093.
- [ ] T088 Add Monitor/Log Analytics, VM data collection, Workbooks, alerts and budgets in `infra/terraform/stack/monitoring.tf`, with ingestion transformations in `infra/terraform/stack/queries/sanitize-access.kql` and `infra/terraform/stack/queries/sanitize-firewall.kql`; scrub WAF fields and transform identifiers/matched data before retention, collect only sanitized application logs and exercise T083 canaries. Set 30-day retention and exclude unsanitized duplicate exports; actual retained-log and alert evidence waits for T093. Explicitly create RoadSignRuntime_CL and its Linux JSONL DCR/VM association per telemetry.md. Add `infra/terraform/stack/queries/runtime-metrics.kql` and `infra/tests/test_runtime_metrics_queries.py` for process/sequence deduplication, cumulative deltas, histogram quantiles, resets/gaps and three-missing-sample alerts; offline tests do not substitute for T093 ingestion evidence.
- [ ] T089 Implement secret-free VM setup in `infra/cloud-init/host.yaml.tftpl`, certificate refresh in `scripts/deploy/refresh-certificate.sh`, `compose.azure.yaml`, and `config/Caddyfile.azure`; fetch certificates with managed identity into tmpfs, retain non-root/read-only/resource restrictions and disable swap/core dumps across reboot. Add cloud privacy text in `frontend/src/modules/recognition-session/RecognitionPage.tsx` and non-JSON gateway-denial regressions in `frontend/tests/e2e/AccessibleControls.spec.ts`. Test host/config/certificate failure paths locally; live TLS/rotation/reboot checks belong to T093. Enable RUNTIME_METRICS_ENABLED in the Azure profile and mount only `/var/log/road-sign/runtime/` for bounded aggregate output. Provision matching ownership, daily-file cleanup and the separate 4,000-MB AMA disk-cache quota; keep session/media paths uncollected and local exporter disabled.
- [ ] T090 Complete the incremental `.github/workflows/ci.yml` from T008/T020 and implement `.github/workflows/terraform-plan.yml`; require native/sanitizer/Python/TS/browser/Docker and Terraform checks, pinned actions, fork isolation and trusted-main OIDC plans with checksum/commit/state/lock binding and <=24-hour private artifacts. Add weekly read-only drift and workflow-permission/failure-propagation tests in `infra/tests/test_workflow_policy.py`; document required branch/environment protection in `docs/azure-operations.md`. Missing required suites or real-model release evidence cannot silently pass. Keep private-model retrieval out of fork/PR CI. Define deterministic CI separately from the mandatory trusted real-model publish gate; core CI may validate infra bootstrap before a candidate exists, but no application release is eligible until model validation passes.
- [ ] T091 Implement `.github/workflows/publish.yml`, `scripts/deploy/upload-model-candidate.sh`, selection schema in `config/model-selection.schema.json` and a consumer for reviewed `config/model-selection.json` (the operator creates that selection in T093 after upload), plus `config/release-manifest.schema.json` and `scripts/deploy/validate_release.py`. Follow model-promotion.md: operator-upload the verified package with interactive identity after ACR exists, read the selected candidate digest/hash at the tested main SHA, retrieve safely and rerun parity/real-model checks in the protected OIDC job. Publish backend/web images, fixture-free model bundle and release-manifest/evidence OCI artifacts; return the release-artifact digest. Require all Azure release fields plus candidateDigest/packageManifestSha256, and test fresh-checkout retrieval, absent/stale/mismatched artifacts, unsafe files, scope restrictions and evidence linkage in `infra/tests/test_release_manifest.py`. Do not fabricate a model-selection value or placeholder file before an actual uploaded digest exists; author/test the workflow now and keep live publication blocked until T093 supplies that input.
- [ ] T092 Implement protected `.github/workflows/deploy.yml` and `scripts/deploy/deploy-release.sh` with separate infra-apply/app-deploy/rollback modes; consume reviewed unexpired plans or T091 release-artifact digests; retrieve/verify the selected manifest through managed identity, use OIDC/VM managed-identity pulls and digest-pinned Compose, serialize maintenance/health checks and keep admission closed on failure. Add `infra/tests/test_deploy_release.py` for stale plans, no release required for infra-apply, failed initial deployment, successful previous-release restore and failed rollback; retain active/previous manifests and never restore sessions or copy old Terraform state. Validate locally before any live run.
- [ ] T093 Execute the cloud sequence on an authorised Azure environment and record `reports/validation/azure.md`: operator bootstrap/state migration, protected infra-apply with gateway disabled to create ACR/Key Vault/private compute, external certificate issuance/import, reviewed gateway enablement, T091 first publish, then T092 app-deploy. Verify AZ-001–AZ-008 with state locking/recovery, scope denial, maximum uploads/retries, 5-FPS frames, ten-minute SSE, physical-phone HTTPS/audio, safe WAF/rate/bot checks, log canaries, reboot/rotation, alert delivery and failed-deploy rollback. Record missing inputs as incomplete; do not generate DDoS traffic. Before first publish, run the operator candidate uploader, create `config/model-selection.json` from its real digest/hash and review that commit; require fresh-checkout retrieval and validate the emitted release-artifact digest. Prove runtime counters actually reach RoadSignRuntime_CL/Workbooks, including duplicates, gaps/reset identity, missing-sample alerts and bounded collector outages.
- [ ] T094 Finalize the exercised bootstrap/deployment/rollback/drift/renewal/incident/cost/teardown instructions in `docs/azure-operations.md`; record GitHub protection settings, cloud privacy boundary, cost estimate and owner, surviving state/vault retention, and links to AZ-001–AZ-008 evidence. Preserve the previous compatible release and bootstrap state; record unexecuted operational checks explicitly. Document interactive candidate upload/temporary credential cleanup, reviewed selection, protected publication reruns, registry-wide role trust, release-artifact digests and fixture exclusion. Include runtime schema/queries, exporter/AMA storage accounting and telemetry loss diagnosis.

**Independent Test**: Execute T093 against the configured tester boundary; validate approved traffic,
blocked unsafe requests, log-canary absence, streaming/upload compatibility and restoration of a
previous compatible release after a failed deployment. Offline plan tests alone cannot pass this gate.

**Checkpoint**: A validated, restricted Azure release and tested rollback exist. Missing subscription,
certificate, real model, environment approval gates or cloud evidence leaves dependent tasks open.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Verify the complete prototype and its delivery instructions without expanding scope.

- [ ] T095 [P] Add final lifecycle/privacy integration tests in `backend/tests/integration/SessionPrivacyTest.cpp` for stop during multipart spooling, nonce abuse/concurrent uploads, invalid cookies/CSRF, session replacement and container restart; inspect no-store responses, tmpfs cleanup, redacted logs and absence of user data in persistent mounts/browser storage.
- [ ] T096 [P] Add cross-module cancellation stress tests in `backend/tests/integration/CancellationStressTest.cpp` for late inference/OCR/TTS callbacks, reordered control replies, disconnect during playback and repeated start/stop; exercise both sanitizers and verify no surviving session-owned buffers or detached workers.
- [ ] T097 Run native builds/unit/contracts/integration, ASan+UBSan, separate TSan, Python checks, strict TS/lint, frontend tests and browser workflows, recording actual commands/results and any unsupported instrumentation in `reports/validation/final-checks.md`; fix failures in scoped files with regression tests and do not suppress checks to pass.
- [ ] T098 Validate both Docker build targets, Compose startup/readiness, non-root/resource bounds, proxy admission and physical-phone HTTPS trust using `Dockerfile`, `compose.yaml`, and `config/Caddyfile`; record native dependency/model hashes, host swap state and any failed deployment checks in `reports/validation/deployment.md`.
- [ ] T099 Update `README.md` and `specs/001-assistive-sign-recognition/quickstart.md` to the commands actually implemented and exercised; document dependency bootstrap, dataset/model acquisition, CA trust, privacy boundaries, measured limitations and cleanup without claiming unexecuted tests passed.
- [ ] T100 Audit every requirement and evidence gate against the constitution in `reports/validation/completion.md`; verify no production stubs, runtime Python, private cross-module includes, unexpected persistence or scope additions, leave unfinished external-data/device/study tasks unchecked and commit coherent validated increments using explicit paths while leaving the constitution uncommitted.

---

## Dependencies & Execution Order

### Phase Dependencies

```text
Setup T001–T008 -> Foundations T009–T020 -> US1 image slice T021–T044
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
                                    Azure T083–T094
                                             |
                                             v
                                    Polish T095–T100
```

The default execution is sequential by phase. Overlap is optional and restricted below; the graph
does not imply all story implementations are independent. Each story has its own observable test,
but full-stack US2 builds on the US1 image/native pipeline and completes its camera acceptance;
full US3 validation needs US2 seek/continuous behavior.
Cloud design/test authoring T083–T090 may start after foundations when explicitly scheduled; do
not deploy before the relevant application/model/accessible-control checks pass. Phase 7's default
position does not prevent earlier CI adoption. CI starts in T008, gains Docker checks in T020 and is completed for cloud delivery in T090.
T083–T092 produce code/configuration and offline tests without requiring an Azure account; their
live behaviors are proved at T093. That task applies bootstrap and the gateway-disabled stack,
installs the certificate, enables the gateway, uploads/selects the authorised candidate, runs
trusted publication and only then app-deploy by release-artifact digest. Authoring
publish/deploy workflows must not depend on an already published release. Absent external evidence
never becomes a green check.

### User Story Dependencies

- **US1**: Requires foundations and an authorised dataset plus a verified custom bundle for final
  image acceptance. Full camera acceptance waits for T058. Its native adapters and UI can be developed with test-only fixtures while data work
  is pending; do not mark T033/T044 complete without real evidence.
- **US2**: Requires US1's media/inference/OCR/speech contracts and implementation. Pure tracker and
  scheduler tests can be authored earlier against public contracts, but integration waits for US1.
- **US3**: Control tests and disjoint UI components can be prepared after US1; full validation requires
  US2 seek/streaming. T063/T064 touch existing US1 files and must not overlap their earlier owners.
- **US4**: Offline metric unit tests and study preparation can start after foundations and catalogue
  definition; actual evaluation requires the trained model, continuous pipeline and accessible
  prototype. Reports/visual review integrate after US3 in the default sequence.
- **Azure**: Reuses the Docker/API/UI contracts and requires their validated implementation for
  live deployment. Security/infra tests precede resources; delivery tests precede cloud acceptance.
- **Polish**: Requires all implemented stories and Azure delivery; completeness cannot be claimed while dataset,
  participant, native-host or device evidence remains unavailable.

### Entity and contract ownership

Every entity table row from data-model.md is quoted verbatim in its owning task: shared Frame and
SignObservation T011/T012; RecognitionSession T013; UploadReceipt T019; catalogue/dataset/model T025/T026/T029;
MediaSource T030; AcceptedInformation/LatestAcceptedDescription T035; AudioArtifact T036;
Announcement/CancellationReceipt T037; TrackedSign T048; HistoryEntry T073; EvaluationRun T077.
API lifecycle/auth/SSE belongs to T009–T020, image/audio/receipts to T021/T037–T042, video/seek to
T047/T052–T056, manual recovery to T059–T067 and visual fallback to T071/T074. Model handoff is
T022/T025–T033 with candidate promotion at T091/T093; cloud ingress/release contracts are
T083–T094, and runtime metric emission/ingestion is T078/T088/T089/T093. Use these owners when a contract
change requires callers and tests to move together.

### Within Each User Story

- Test wave first, then entities/adapters, orchestration/handlers, UI integration and acceptance.
- Each implementation task must run its paired tests plus new unit tests for uncovered behavior,
  and add the executable suite to the existing `.github/workflows/ci.yml` when it becomes available.
  A missing future suite is reported as pending; it cannot satisfy a release gate.
- T027 -> T028–T029 -> T033 is the real data/model execution chain; T030–T032 provide native parity
  dependencies for T033. Exporting a model and checking compatibility are different deliverables.
- T036–T037 -> T038–T039 -> T040–T043 -> T044 is US1's speech/API/UI integration chain. T108 follows T044 for held-out image evaluation. T058 owns held-out camera evaluation in US2.
- T048–T050 -> T051/T054 and T052 -> T053 -> T055 precede US2 integration/acceptance.
- T059–T061 -> T062–T067 -> T068–T070 precede complete accessible-control acceptance.
- T071/T072 -> T073–T078 -> T079–T081 -> T082 precede research acceptance.

### Parallel Opportunities

`[P]` means only the named wave's tasks are safe to run concurrently after its prerequisites:

| Wave | Prerequisite | Disjoint tasks |
|------|--------------|----------------|
| Setup packages/provenance | T001–T002 | T003, T004 |
| Foundation tests | Phase1 complete | T009, T010 |
| US1 contract/test design | Phase2 complete | T021, T022, T023, T024 |
| US2 policy/video test design | US1 image slice complete | T045, T046, T047 |
| US3 controls/test design | US2 complete | T059, T060, T061 |
| US4 review/metrics test design | US3 complete | T071, T072 |
| Cross-cutting integration tests | US4 and Azure delivery complete | T095, T096 |

Do not run tasks sharing configuration, composition roots or controllers in parallel merely because
they belong to different stories. Test implementation can wait for a missing dependency, but its
acceptance must not be marked complete until it executes and exhibits the intended failure/pass.

## Parallel Example: User Story 1

After foundations, author T021 HTTP contracts, T022 Python handoff tests, T023 native recognition
boundaries and T024 browser journey tests together; each writes its own named test file. Integrate
interfaces before running implementation-dependent tests. Then proceed with T025 onward.

## Parallel Example: User Story 2

After the US1 image slice, author T045 tracker tests, T046 scheduler tests and T047 uploaded-video contract tests
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
| FR-012 cancellation | T013–T016, T037–T042, T047, T053, T056, T059–T067, T096 |
| FR-013 one-session demo | T009, T013, T015, T020, T068, T095 |
| FR-014 privacy | T001, T010, T014–T020, T027, T067, T076, T078, T095 |
| FR-015 training/provenance | T004–T005, T022, T025–T029, T032–T033 |
| FR-016 evaluation | T072, T076–T082 |
| FR-017 no inferred navigation | T025, T035, T044, T100 |
| SC-001 reference scenarios | T044, T058, T070, T082 |
| SC-002 accessible tasks | T061, T068–T070 |
| SC-003 repeats/overlap/stop | T046, T056–T060, T064, T078–T079, T096 |
| SC-004, SC-005, SC-006 detection/OCR/tracking scores | T072, T076–T079 |
| SC-007 runtime performance | T057, T078, T080 |
| SC-008 intended-user study | T081 |
| SC-009 robustness breakdown | T027, T072, T077, T079 |
| SC-010 lifecycle/privacy | T009–T010, T067, T073–T075, T095 |
| AZ-001 WAF, AZ-002 rate limits, AZ-003 bot protection | T083, T087, T089, T093 |
| AZ-004 secrets and managed/federated identities | T084, T086, T089–T093 |
| AZ-005 logs and analytics | T078, T083, T088–T089, T093–T094 |
| AZ-006 DDoS protection | T083, T087–T088, T093 |
| AZ-007 Terraform | T083–T090, T092–T094 |
| AZ-008 GitHub Actions CI/CD and rollback | T090–T094 |

## Implementation Strategy

### MVP First (User Story 1)

Complete setup/foundations, then the US1 image slice with a real custom model. Finish its camera
acceptance at T058 after temporal policy is integrated. The image MVP demonstrates recognition
and speech; it is not the complete assistive prototype until US2 and US3 P1 behavior is validated.
If real dataset/model inputs are unavailable, continue independent tooling/tests and explicitly
retain the data/training/real-recognition tasks as incomplete; do not downgrade the acceptance gate.

### Incremental Delivery

1. Setup/foundations establish deterministic contracts, ownership, incremental CI and local delivery.
2. The US1 image slice delivers recognition and native speech; camera acceptance remains pending.
3. US2 completes camera acceptance and adds video, stable identities, text consensus and bounded announcements.
4. US3 completes accessible control/recovery and real-device evidence.
5. US4 adds optional review and actual research outcomes.
6. Azure delivery adds Terraform, required security/analytics and GitHub Actions with tested rollback.
7. Final validation exercises packaging, memory safety, isolation and documented run instructions.

### Parallel Team Strategy

Use only the disjoint test/setup waves above by default. If multiple implementers are assigned,
allocate ownership by domain and serialize composition-root/API changes. Data collection and
participant recruitment are external activities that require genuine access/consent; no agent can
complete them by inventing evidence. No parallel execution is launched by generating this file.

## Notes

- Every task remains unchecked until its implementation or evidence deliverable is verified.
- Frequent scoped rollback commits are authorised. Cloud deployment, participant contact and data
  collection still require their own applicable authorisation; no deployment occurs in planning.
- Do not modify the constitution or checked requirements checklist as a shortcut around failures.
- Numerical model quality remains a research report; functional behavior and privacy are test gates.
