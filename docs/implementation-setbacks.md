# Implementation setbacks

This log records issues that changed the implementation or delayed validation. It distinguishes local checks from checks completed on GitHub Actions.

## 2026-11-24

- **Invalid GitHub action reference:** The frontend CI job initially referenced an unavailable `actions/setup-node` commit. It was replaced with the verified action SHA; the workflow then passed according to the user's GitHub Actions run.
- **CMake cache bound to a different path:** A host build attempted to reuse cache files generated under `/workspace`, causing CMake's source-directory mismatch. Native builds must run in the Linux container or use a fresh host build directory.
- **Wrong ONNX Runtime architecture selected:** Running the generic `dev` preset inside the ARM64 container selected the x64 dependency prefix and failed at link time. The `dev-arm64` preset selects `build/arm64/native` and passed the native CTest suite.
- **Disk exhaustion:** The host filesystem reached 100% usage and refused to create the T010 test directory. Generated build artifacts were later cleared by the environment; 7.8 GiB became available. Preserve the ARM64 dependency prefix and training environment when cleaning again.
- **Stopped container and Docker socket access:** The ARM64 container was stopped between checks, and Docker socket access intermittently required elevated execution. The container was restarted and its tests passed.
- **Stale bind-mounted source:** The running container exposed a truncated `backend/shared/Frame.hpp` despite the intact host file. Copying the host file into `/workspace` restored matching SHA-256 hashes; the public-header syntax check and ARM64 CTest passed 6/6 afterward.
- **Offline npm audit:** The local audit command could not reach `registry.npmjs.org`. CI retains the audit step; local audit status is therefore unverified.
- **CI branch trigger:** CI initially ran only on pull requests and pushes to `main`. The feature branch `001-assistive-sign-recognition` was added to the push trigger so its pushes run CI.
- **Stale container source recurred:** Later edits again did not appear in the container bind view. Explicit `docker cp` synchronization was needed before ARM64 compilation; the final 8-test suite passed after syncing. Keep checking host/container hashes before treating a container build as evidence for current host files.
- **Executor compile issues:** The first bounded-worker build exposed a wrong shared-header include path and an invalid default construction of an immutable `Frame`. The include root was corrected, the work item became optional, and the ARM64 suite passed 8/8.
- **Recursive bind-mount copy:** Copying the entire session module directory into its own bind-mounted destination created nested duplicate source directories. The copies were removed in a corrective commit; future syncs use individual files and verify canonical paths before building.
- **Docker image layer commit I/O error:** The first multi-stage ARM64 backend build compiled and linked `road-sign-server`, then Docker Desktop failed to commit the image layer. `docker system df` later succeeded (3.6 GiB build cache, 2.7 GiB reclaimable); the build was retried with cache. No existing images or volumes were removed.
- **Trantor timer API mismatch:** The persistent SSE implementation initially called nonexistent `EventLoop::cancelTimer`; Trantor exposes `invalidateTimer(TimerId)`. The call was corrected; the ARM64 backend image now builds.
- **Docker rebuild exhausted host storage:** The first retry reached the ARM64 OpenCV configuration but the host filesystem hit 100% with 41 MiB free. Docker Desktop stopped responding until relaunched. No image, container, volume, or build cache was pruned; the cached retry then completed successfully.
- **ARM64 compile found missing session admission fields:** The next cached build reached application compilation and found `SessionService::finishUpload()` referenced nonce/deadline fields missing from `Entity`. The entity now owns those fields; the backend image and all eight CTest cases pass.
- **Compose ingress was on an internal network:** The internal-only network prevented host access to the published HTTPS port. Caddy now joins a dedicated bridge network and the internal network; the backend remains on the internal network only. The HTTPS lifecycle smoke passed after the change.
- **Drogon cookie header is consumed by its parser:** `HttpRequest::getHeader("cookie")` returned an empty value because Drogon stores parsed cookies separately. Session ownership now uses `getCookie("rs_session")`; the Compose lifecycle smoke passed through session creation, SSE, heartbeat, upload snapshot, and cleanup.

## Current validation boundary

The current ARM64 backend image built successfully, all eight CTest cases passed, and both backend and web images were confirmed as `linux/arm64`. A read-only/non-root backend smoke returned health 200 and missing-model readiness 503. The isolated Compose HTTPS smoke passed health, capabilities, readiness, SSE, heartbeat, upload snapshot and session cleanup. Upload admission/media validation, richer SSE state/upload events, real-model readiness, and release eligibility remain unverified or unimplemented.
