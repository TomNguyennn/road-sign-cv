# Native ONNX Runtime from NuGet

The C++ backend uses Microsoft's `Microsoft.ML.OnnxRuntime` CPU package directly.
NuGet is the distribution format; neither the .NET SDK nor a .NET/Python runtime
is needed by the application. See the [official installation guide](https://onnxruntime.ai/docs/install/#install-onnx-runtime-cpu-1).

`cmake/OnnxRuntime.cmake` downloads version **1.22.0** over verified TLS and checks
SHA-256 `d571e63a2329baacb713f441e65ad75284de354db6e1ac435fe4bebbb417986a`
before extraction. It imports matching headers and selects the Linux amd64 or arm64
CPU library from the same package according to the CMake target processor.
This hash pins the inspected package bytes; it is not a claim of independent
publisher-signature verification or a security audit of this version.

Include the module from the future root CMake build and link the inference adapter:

```cmake
include(cmake/OnnxRuntime.cmake)
target_link_libraries(your-inference-target PRIVATE ONNXRuntime::ONNXRuntime)
```

The imported target supplies include paths and the native shared library. The
module copies the library to `libonnxruntime.so.1`, matching its ELF SONAME, and
keeps the shared provider library alongside it. CMake supplies the build-tree
runtime search path. Final Docker packaging must copy these runtime libraries,
preserve the package's licence/third-party notices, and configure the installed
loader path; build-tree success does not validate that future runtime image.

## Run the standalone dependency check

On Ubuntu 24.04 Linux amd64 with GCC13, CMake >=3.28, Ninja and CA certificates:

```sh
cmake -S cmake/tests/onnx-runtime -B build/onnx-smoke -G Ninja -DCMAKE_CXX_COMPILER=g++-13
cmake --build build/onnx-smoke
ctest --test-dir build/onnx-smoke --output-on-failure
```

The check verifies native loading, exact runtime version, C++ environment creation
and a CPU tensor round trip. It requires no model, Python or .NET. First configure
needs access to `api.nuget.org`; later builds reuse CMake's downloaded content.
The module rejects native macOS builds. On Apple Silicon, use a native Linux
ARM64 container and the `*-arm64` root presets. Copied ONNX libraries are separated
by architecture even when the extracted package cache is shared. Emulated runs
are not performance evidence.

This completes only the ONNX Runtime dependency integration within T002. Other
native dependencies, full build presets, real-model inference/parity, sanitizers
and final application packaging remain separate implementation work.

## Validation recorded 2026-11-23

- Ubuntu 24.04 Linux amd64 container, emulated on Apple Silicon; GCC 13.3.0,
  CMake 3.28.3, Ninja; C++20 with warnings treated as errors.
- Fresh CMake download/extraction, compilation and linking passed; CTest: 1/1 passed.
- Native macOS configure rejected with the intended Linux amd64 diagnostic.
- Initial smoke-project configure failed before the dependency module was added.
- `git diff --check` passed. The container's Ubuntu packages were installed from
  its current package repositories for this smoke check; a fully locked build
  environment is still part of T002.
