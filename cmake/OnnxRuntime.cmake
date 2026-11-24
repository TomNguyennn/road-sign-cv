include_guard(GLOBAL)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
   NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|aarch64|arm64)$" OR
   NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Use a 64-bit Linux amd64 or arm64 toolchain for ONNX Runtime.")
endif()

include(FetchContent)
FetchContent_Declare(
    onnxruntime
    URL https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/1.22.0/microsoft.ml.onnxruntime.1.22.0.nupkg
    URL_HASH SHA256=d571e63a2329baacb713f441e65ad75284de354db6e1ac435fe4bebbb417986a
    DOWNLOAD_NAME onnxruntime-1.22.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    TLS_VERIFY TRUE
)
FetchContent_MakeAvailable(onnxruntime)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(ONNX_RUNTIME_ARCH arm64)
else()
    set(ONNX_RUNTIME_ARCH x64)
endif()
set(ONNX_RUNTIME_NATIVE_DIR "${onnxruntime_SOURCE_DIR}/runtimes/linux-${ONNX_RUNTIME_ARCH}/native")
set(ONNX_RUNTIME_LIBRARY_DIR "${onnxruntime_BINARY_DIR}/lib/${ONNX_RUNTIME_ARCH}")
file(MAKE_DIRECTORY "${ONNX_RUNTIME_LIBRARY_DIR}")
# NuGet omits the filename matching the ELF SONAME needed by the Linux loader.
configure_file("${ONNX_RUNTIME_NATIVE_DIR}/libonnxruntime.so"
               "${ONNX_RUNTIME_LIBRARY_DIR}/libonnxruntime.so.1" COPYONLY)
configure_file("${ONNX_RUNTIME_NATIVE_DIR}/libonnxruntime_providers_shared.so"
               "${ONNX_RUNTIME_LIBRARY_DIR}/libonnxruntime_providers_shared.so" COPYONLY)

add_library(ONNXRuntime::ONNXRuntime SHARED IMPORTED GLOBAL)
set_target_properties(ONNXRuntime::ONNXRuntime PROPERTIES
    IMPORTED_LOCATION "${ONNX_RUNTIME_LIBRARY_DIR}/libonnxruntime.so.1"
    INTERFACE_INCLUDE_DIRECTORIES "${onnxruntime_SOURCE_DIR}/build/native/include"
)
