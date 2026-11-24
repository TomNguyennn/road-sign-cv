# Native build setup

**Validation status (2026-11-24): ARM64 setup verified.** All native dependencies
built and installed. Development, ASan+UBSan and TSan suites each passed all four
tests in the native Linux ARM64 container. The amd64 development suite also
passed; sanitizer execution under local amd64 emulation remains unsupported by
the tested environment. Native amd64 sanitizer validation is still required for
that deployment target.

Use Ubuntu 24.04 Linux amd64, GCC13, CMake >=3.28 and Ninja. On Apple Silicon,
run an amd64 Docker container for development; the tested QEMU configuration
cannot run the sanitizer suites and cannot establish performance results.
The dependency build also needs Make, pkg-config, NASM, CA certificates and the
Ubuntu development packages for OpenSSL, zlib, UUID, PNG and JPEG.

```sh
apt-get update
apt-get install --no-install-recommends ca-certificates cmake ninja-build g++-13 \
  make pkg-config nasm libssl-dev zlib1g-dev uuid-dev libpng-dev libjpeg-dev
cmake --preset dependencies
cmake --build --preset dependencies
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The `dependencies` preset installs hash-pinned upstream sources under `build/native`.
It runs one external project at a time, using at most four compiler jobs.
Subsequent application configure steps require that installation; they fail when
a required library is missing. ONNX Runtime uses the independently hash-verified
[NuGet integration](native-onnx-runtime.md). ByteTrack compiles only the tracker
sources, without the ncnn sample. Its application-specific temporal/category and
memory-bound adaptations remain in the tracking implementation tasks.
An idempotent, tested patch adds missing `iostream` and `map` includes to the
pinned tracker sources; it does not change the tracking algorithm.
OpenCV enables JPEG/PNG image support and FFmpeg video decoding; optional AVIF,
JPEG2000, OpenEXR, TIFF and WebP image libraries are disabled.
FFmpeg is pinned to 7.1.2: 7.1.1 has an
[upstream-confirmed minimal H.264 link defect](https://ffmpeg.org/pipermail/ffmpeg-devel/2025-May/343410.html).
The build guard checks OpenCV's FFmpeg compile/link result before compiling OpenCV;
the native smoke test also checks that its video backend and required decoders exist.

```sh
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
cmake --preset tsan
cmake --build --preset tsan
ctest --preset tsan
```

ASan/UBSan and TSan have separate build directories. The prebuilt ONNX Runtime
package is downloaded/extracted once under `build/fetched` for these presets. Application and tracker
sources receive the selected instrumentation. The shared third-party installation
and prebuilt ONNX Runtime library are not instrumented by these presets; this
limits sanitizer coverage inside those binaries. A sanitizer failure must be
investigated rather than suppressed to make a release pass.

Both sanitizer presets failed with `sanitizer_allocator_primary32.h:292`, before
the native tests could execute. A separate `int main() { return 0; }` compiled
with GCC13 and each preset's sanitizer flags reproduced the same failure without
linking project dependencies. This isolates the observed startup failure to the
current execution environment; it does not establish that the application is
sanitizer-clean. Repeat both complete suites on native Ubuntu amd64 and retain
their results for deployment-target validation. No sanitizer checks have been disabled.

CMake-built dependencies use an installation RPATH of `$ORIGIN/../lib`. FFmpeg's
enabled RPATH uses its configured absolute installation prefix. Final container
packaging must preserve that prefix or explicitly configure library resolution;
runtime-image validation remains part of T020.

The dependency smoke checks require no model or OCR language data. They do not
prove recognition accuracy, speech synthesis, production packaging or readiness.
T005 records the complete provenance inventory, including OS prerequisites and
language/voice data. No repository licence is selected by this build setup.

## Native ARM64 development

Use the architecture-specific Ubuntu 24.04 manifest below to avoid binding the
same multi-platform index reference to both architectures in a classic Docker
image store. Keep the existing amd64 container and outputs.

```sh
docker run -d --platform linux/arm64 --name road-sign-native-arm64 \
  --mount type=bind,source="$PWD",target=/workspace --workdir /workspace \
  ubuntu@sha256:cd04f18ea6639206fcecf4c148bde909cd08098e1d0569117c2af59242059558 \
  sleep infinity
```

Inside this container, install the prerequisites above, then run:

```sh
cmake --preset dependencies-arm64
cmake --build --preset dependencies-arm64
cmake --preset dev-arm64
cmake --build --preset dev-arm64
ctest --preset dev-arm64
cmake --preset asan-arm64
cmake --build --preset asan-arm64
ctest --preset asan-arm64
cmake --preset tsan-arm64
cmake --build --preset tsan-arm64
ctest --preset tsan-arm64
```

These presets keep compiled dependencies and application outputs under
`build/arm64/`; the downloaded ONNX package is shared, with copied runtime
libraries separated by architecture. Use these presets only in an ARM64
container. Azure deployment remains amd64 and requires native amd64 validation.

ARM64 validation passed on 2026-11-24: the container reports `aarch64`, all
dependencies built, and dev/ASan+UBSan/TSan each passed 4/4 tests. Logs are
`build/arm64-dependencies.log`, `build/dev-arm64.log`, `build/asan-arm64.log`,
and `build/tsan-arm64.log`. Dependency-free sanitizer probes also passed.
These are dependency integration checks, not recognition or race-coverage tests
for application features that have not yet been implemented.
