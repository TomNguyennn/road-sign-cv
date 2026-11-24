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

## Current validation boundary

The verified ARM64 suite contained six tests, including the runtime foundation test. This does not establish lifecycle/API/SSE behavior, model readiness, Docker runtime safety, or release eligibility. Those remain open until the corresponding implementation tasks and tests pass.
