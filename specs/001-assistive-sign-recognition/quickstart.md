# Quickstart and Validation Guide

This is the intended runnable interface for the implementation phase. The repository currently
has no application, Dockerfile, trained model, or test executables; the commands below are not
claimed to work today. `$speckit-tasks` must turn each prerequisite into implementation work.
The Azure/Terraform/GitHub Actions workflow below is also a planned deliverable, not a deployed
pipeline. Local and cloud profiles share application contracts but use different TLS setup.

## Prerequisites and dataset/model preparation

- Linux amd64 reference host, >=4 CPU cores and 8 GiB RAM; Docker Engine/Compose v2.
  Docker Desktop is allowed for development but record emulation and VM conditions separately.
- Python 3.12, Node 22.12+ in the 22.x family, and the locked toolchain from plan.md. Training GPU
  use is optional and must be recorded; the inference baseline does not require a GPU.
- Authorised dataset manifest at `research-data/uk-signs-v1/manifest.json`, including source/location
  grouped splits, all eight categories, transcripts and video track IDs. Collect/annotate this data;
  there is no bundled or assumed complete public dataset.
- Immutable model bundle at `models/signs-v1`, created and verified by the commands below.
- For strict no-durable-media validation, verify swap is disabled on the Linux host/VM and core
  dumps are disabled. Do not claim this guarantee on a host whose swap/snapshot behavior is unknown.

Run from repository root after the implementation commands and lockfiles exist:

```sh
python3.12 -m venv .venv
.venv/bin/python -m pip install -r training/requirements.lock
.venv/bin/python -m pip install --no-deps -e training
.venv/bin/python -m road_sign_training.validate_dataset --manifest research-data/uk-signs-v1/manifest.json
.venv/bin/python -m road_sign_training.train --manifest research-data/uk-signs-v1/manifest.json --model yolo11n.pt --seed 42 --output reports/training-v1
.venv/bin/python -m road_sign_training.export --checkpoint reports/training-v1/best.pt --output models/signs-v1
cmake --preset dev
cmake --build --preset dev
.venv/bin/python -m road_sign_training.check_parity --bundle models/signs-v1 --fixtures resources/fixtures/parity.json --native build/dev/backend/app/road-sign-evaluate
```

Expected: validated provenance/splits, eight-class model, complete manifest and passing parity.
Missing dataset/checkpoint or failed export must stop this flow, not silently use a generic model.
Dependency bootstrap must fetch named immutable sources and checksums before CMake configure.

## Build and serve the controlled demo

```sh
docker build --target backend-runtime -t road-sign-backend:dev .
docker build --target web-runtime -t road-sign-web:dev .
docker compose --env-file config/demo.env up -d --no-build
docker compose ps
docker compose logs --tail 50 backend
```

The implementation must provide `config/demo.env.example`; copy it to ignored `config/demo.env`
and set `DEMO_HOST` to a LAN-resolvable hostname or host IP, `MODEL_BUNDLE_PATH=/models/signs-v1`,
the declared model mount path, and `MAX_UPLOAD_ATTEMPTS=3` (0 means unlimited attempts). Compose service images match the two build tags above. Only
Caddy exposes port 443; the C++ service is reachable on the internal network. Caddy `tls internal`
uses persistent certificate state only. Backend runs non-root, read-only, with 256 MiB tmpfs at
`/run/road-sign`, a 4 GiB memory limit, no core dumps, and no user-media persistent volume.

Retrieve the local CA certificate after initial startup:

```sh
docker compose cp web:/data/caddy/pki/authorities/local/root.crt /tmp/road-sign-demo-ca.crt
```

Trust this certificate explicitly on the desktop and test phone using their OS/browser trust
settings. Visit `https://<DEMO_HOST>/`; bypassing a certificate warning is not a substitute for a
trusted secure camera context. Keep phone and host on the permitted demo network. Do not publish
this single-session prototype as an unrestricted internet service.

## Azure deployment and GitHub Actions validation

Use [azure-deployment.md](azure-deployment.md) for the required services, AZ-001–AZ-008 acceptance
gates, Terraform layout, permissions, release manifest and rollback rules. These steps apply only
after the infrastructure, workflows and real custom model have been implemented and validated.

1. Supply Azure tenant/subscription, UK South quota or another recorded region, owned domain and
   certificate, tester CIDRs, budget, alert recipients and GitHub environment protection. Confirm
   the gateway/DDoS/NAT cost estimate before provisioning. No cloud credentials belong in the repo.
2. Bootstrap the protected Azure Blob state/plan containers and OIDC trust; migrate and verify
   bootstrap state. Pin Terraform/providers and use separate state keys per environment.
3. Run `terraform fmt -check -recursive infra/terraform`, initialize each root and run
   `terraform validate`; run infrastructure policy/redaction tests. Main-branch CI must also pass
   native sanitizers, Python/TS/browser checks and Docker/Compose smoke tests. Fork PR checks must
   have no cloud identity. Do not label missing real-model or physical-device evidence as passing.
4. Generate a trusted Terraform plan. Review its exact SHA, state key, lock hash and cost/security
   changes. Apply only that unexpired saved plan through protected infra-apply. On first setup,
   create the stack with the gateway disabled, issue/import the Key Vault certificate, then review
   and apply gateway enablement as documented in azure-deployment.md; do not reuse the local CA.
5. Follow [model-promotion.md](contracts/model-promotion.md): package the verified local bundle and
   authorised parity fixtures/framework references, upload to the provisioned ACR through interactive
   Entra authentication, then review/commit config/model-selection.json with the candidate digest and
   package-manifest hash. A fresh trusted main checkout must retrieve that exact candidate, reject
   malformed/changed files and pass parity/real-model gates before publishing. PR/fork CI stays
   credential-free. Dispatch protected deployment with the published release-artifact digest, not
   a mutable tag or local path. The VM pulls with managed identity and runs Compose with
   `--no-build`; health and synthetic recognition checks gate reopening admission.
6. Validate maximum/oversized uploads and retry after failure, 5-FPS frames, ten-minute SSE,
   cancellation/recovery, phone camera/audio TLS, blocked direct-origin access, safe WAF/bot/rate
   rejection and sanitized analytics. Verify DDoS configuration/alerts without attack traffic.
7. Exercise certificate renewal, host reboot without swap, state locking/recovery and a deliberately
   failed release followed by rollback. Record Azure end-to-end latency separately from native
   inference results in `reports/validation/azure.md`.
8. Validate [telemetry.md](contracts/telemetry.md): enable the Azure aggregate exporter and verify
   known C++ frame/queue/histogram samples reach RoadSignRuntime_CL and its Workbook through AMA/DCR.
   Test duplicate samples, process resets, missing samples/alerts and text/secret canaries. Stall the
   writer/collector and fill its bounded directory; recognition must keep working with visible
   telemetry loss. Verify 16-MiB application log rotation separately from the 4,000-MB agent cache.

Deploy/rollback interrupts the active session and requires a fresh user start. It must not replay
history or attempt to restore media. Preserve the previous successful image/model manifest until
the next release is verified. Infrastructure rollback requires a new reviewed Terraform plan.
Use a reviewed destroy plan for environment cleanup; retain state/recovery artifacts. Stopping the
VM does not remove gateway, DDoS, NAT or storage charges, and budget alerts do not cap spending.

Expected: `/api/v1/ready` reports ready, supported modes and limits are visible, no camera opens
until Start, and a phone can access the same HTTPS origin. A missing bundle gives an accessible
unavailable status, never silent fallback or indefinite loading.

## Automated checks

```sh
ctest --preset dev
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
cmake --preset tsan
cmake --build --preset tsan
ctest --preset tsan
.venv/bin/python -m pytest training/tests
npm --prefix frontend ci
npm --prefix frontend run lint
npm --prefix frontend run typecheck
npm --prefix frontend test -- --run
npm --prefix frontend run test:e2e
```

CMake presets must separate ASan+UBSan from TSan. TSan tests cover session cancellation, worker
handoff and speech serialization; record unsupported third-party instrumentation rather than
suppressing failures globally. The frontend e2e command must start a deterministic test backend
with fixed observations/audio fixtures; do not depend on random model scores for policy tests.
Run a separate integration suite against the real verified bundle.

## End-to-end scenarios and expected evidence

| Scenario | Expected result | Requirements/outcomes |
|----------|-----------------|-----------------------|
| Reference signs through image and live camera | supported meanings and accepted text spoken; no invented unreadable text | FR-001–004, SC-001/004/005 |
| Video with unchanged, occluded and similar signs | stable separate identities; no unchanged automatic repeats; expiry policy reproducible | FR-005–006, SC-003/006 |
| Warning arrives behind general information | priority selection for next slot, no overlap, stale item discarded | FR-007, SC-003 |
| Mute during synthesis/fetch/playback, stop, seek | immediate local silence and no late old-generation output | FR-008/012, SC-003 |
| Keyboard and real screen readers, visuals hidden | all tasks operable, status available, transcripts not auto-spoken twice | FR-009/010, SC-002/008 |
| Denied permission, bad file, first-image autoplay failure, connection loss | retry within upload budget; Read latest result speaks accepted text without prior playback; no backlog or auto-resume | FR-011/012, SC-001/002 |
| Second client and wrong-cookie/CSRF requests | busy or ownership error, no previous user's result exposure | FR-013/014, SC-010 |
| Stop during upload and container restart | partial media spools removed; no user data in volumes/logs/browser storage | FR-014, SC-010 |
| Dataset/export audit and native parity | provenance, grouped splits, correct eight-class shape/hash, matching predictions | FR-015/016, SC-004–009 |
| Text claiming directions or unknown class | quotes observed information only; no fabricated navigation/crossing decision | FR-017, SC-001 |

Inspect tmpfs ownership/cleanup, container mounts, browser storage, logs and cache headers before
and after each lifecycle case. Do not clear/delete any pre-existing authorised research dataset
while testing session cleanup. Camera decoder resources and audio buffers must be released.

## Research evaluation

The implementation must provide offline commands with these interfaces:

```sh
build/dev/backend/app/road-sign-evaluate --bundle models/signs-v1 --dataset research-data/uk-signs-v1/manifest.json --split test --output reports/evaluation-v1
.venv/bin/python -m road_sign_training.evaluate --predictions reports/evaluation-v1 --manifest research-data/uk-signs-v1/manifest.json --output reports/evaluation-v1/metrics.json
```

Record detection precision/recall/mAP50/mAP50:95 by category and overall. For ranked detection
evaluation, use confidence floor 0.001, NMS IoU 0.45 and max 300 detections/image, before tracking
or temporal acceptance. Report this truncation policy and runtime operating-point precision/recall
separately; do not reuse the runtime 0.10 floor or 100-track cap to claim unqualified mAP. OCR metrics use Unicode
NFKC, collapsed whitespace and case-insensitive scoring (preserve punctuation); include missed
text as deletions and unmatched predicted text as insertions. Report end-to-end and matched-region
CER/WER separately. Associate regions at IoU>=0.5 before transcription scoring. Use standard IDF1,
identity switches and fragmentation with a fixed documented tracking evaluator/configuration.
Duplicate announcement counts exclude isReplay=true and use annotated physical-sign encounters.

Performance: run an actual ten-minute camera session on the native reference host. Client timings
follow contracts/api.md; record p50/p95 result/speech latency, received/dropped/processed frame
counts, processed FPS, peak process RSS and peak container memory including tmpfs. Report start/end
memory and queue high-water marks; do not equate the 5 Hz ingestion cap to measured throughput.
Production demo logs remain redacted; an explicit evaluation mode may export timings/counters only,
never ordinary demo images, transcripts or audio. Trace model/config/environment hashes.

Tag real evaluation scenes by lighting, motion, angle and quality; report sample counts and
missing coverage. Perform the five-participant study in contracts/ui.md with documented tasks,
consent and ratings. Recruitment or data gaps remain incomplete outcomes, not invented results.

## Remediation regression scenarios

- Assert stateRevision is present and monotonic in creation, seek, audio-control and ending replies.
- With MAX_UPLOAD_ATTEMPTS=3, reject an invalid first upload and accept a valid second one; reject
  further /media after acceptance. Test three admitted failures and the accessible new-session
  recovery, pre-admission rejection not consuming budget, and 0 meaning unlimited attempts.
- Fence generation k on mute/seek/timeout, then acknowledge cancellationId with cutoff k while
  current generation is k+1; accept it. Ignore ordinary stale playback results. Test duplicate and
  superseded cancellation receipts and termination after missing acknowledgment.
- Reject the first image's audio activation, then use Read latest result: speak that accepted
  description once without prior started history. Reject stale description IDs, respect mute,
  coalesce repeated manual recovery clicks, and never replay an automatic queue.
- Disable local video decoding/synchronisation in a test: show text/audio and an overlay-unavailable
  explanation; do not request a nonexistent server thumbnail endpoint.
- Image-only acceptance may run before tracking; full US1 camera acceptance must wait for US2's
  tracking/temporal-consensus integration. Capture dependency hashes after package locks resolve.
- Drop a successful upload response with SSE still connected: matching upload.state or GET
  upload-status must show accepted, with no body resend. Drop a validation/abort response while
  lifecycle stays awaitingInput: wait for the terminal receipt after cleanup, then allow a fresh
  explicit attempt. Deliver old snapshots late; counters/state must not regress. For an unmatched
  or idle receipt after an uncertain request, offer end/start rather than guessing failure; bound
  polling to 1 Hz/65 seconds and reject a delayed old request after session replacement.
- Expire/remove the source track and truncate ordinary history, then repeat the last-started text
  or recover the latest accepted description. Require a new manual WAV with null source IDs,
  omitted capture time, no overlay movement and no capture-latency sample. Its offer still expires
  after five seconds if not started; a historical description is not an indefinitely valid WAV.
- Run spec.md US4 scenario 6 across applicable supported browsers with local video decode or
  synchronization disabled: preserve backend recognition, text review and spoken feedback, show an
  accessible overlay limitation and make no server-preview request. This is supported fallback,
  not a claim that an unavailable overlay was rendered successfully.

## Shutdown

```sh
docker compose down
```

Expected: no active camera/audio/session, tmpfs media gone, no user content in persistent storage.
Caddy certificate state and separately authorised offline research artifacts remain by design.
