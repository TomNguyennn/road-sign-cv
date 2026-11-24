cmake_minimum_required(VERSION 3.28)
project(RoadSignCv VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
   NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|aarch64|arm64)$")
    message(FATAL_ERROR "Use an Ubuntu Linux amd64 or arm64 toolchain; macOS requires Docker.")
endif()
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13 OR
   NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
    message(FATAL_ERROR "The reference build requires GCC 13.")
endif()

option(ROAD_SIGN_DEPENDENCIES_ONLY "Build the pinned third-party installation" OFF)
set(ROAD_SIGN_NATIVE_PREFIX "${CMAKE_SOURCE_DIR}/build/native" CACHE PATH "Native dependency installation")
if(ROAD_SIGN_DEPENDENCIES_ONLY)
    include(cmake/Dependencies.cmake)
    return()
endif()

set(ROAD_SIGN_SANITIZER "none" CACHE STRING "none, address-undefined, or thread")
if(ROAD_SIGN_SANITIZER STREQUAL "address-undefined")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
elseif(ROAD_SIGN_SANITIZER STREQUAL "thread")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
elseif(NOT ROAD_SIGN_SANITIZER STREQUAL "none")
    message(FATAL_ERROR "Unknown ROAD_SIGN_SANITIZER value")
endif()

list(PREPEND CMAKE_PREFIX_PATH "${ROAD_SIGN_NATIVE_PREFIX}")
set(ENV{PKG_CONFIG_PATH} "${ROAD_SIGN_NATIVE_PREFIX}/lib/pkgconfig")
find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs videoio)
find_package(Drogon CONFIG REQUIRED)
find_package(Eigen3 3.4 CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(Tesseract CONFIG REQUIRED)
# This upstream export provides the include directory separately from its target.
set_property(TARGET Tesseract::libtesseract APPEND PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES "${Tesseract_INCLUDE_DIRS}")
find_package(PkgConfig REQUIRED)
pkg_check_modules(ESPEAK REQUIRED IMPORTED_TARGET espeak-ng)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale)
include(cmake/OnnxRuntime.cmake)

set(BYTETRACK_SOURCE_DIR "${ROAD_SIGN_NATIVE_PREFIX}/share/bytetrack")
add_library(bytetrack STATIC
    "${BYTETRACK_SOURCE_DIR}/src/BYTETracker.cpp"
    "${BYTETRACK_SOURCE_DIR}/src/STrack.cpp"
    "${BYTETRACK_SOURCE_DIR}/src/kalmanFilter.cpp"
    "${BYTETRACK_SOURCE_DIR}/src/lapjv.cpp"
    "${BYTETRACK_SOURCE_DIR}/src/utils.cpp")
target_include_directories(bytetrack SYSTEM PUBLIC "${BYTETRACK_SOURCE_DIR}/include")
target_link_libraries(bytetrack PUBLIC Eigen3::Eigen opencv_core opencv_imgproc)

enable_testing()
add_test(NAME bytetrack-include-patch COMMAND ${CMAKE_COMMAND}
    -DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/bytetrack-patch-test
    -P ${CMAKE_SOURCE_DIR}/cmake/tests/ByteTrackIncludesTest.cmake)
add_test(NAME opencv-configuration-guard COMMAND ${CMAKE_COMMAND}
    -DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}/opencv-guard-test
    -P ${CMAKE_SOURCE_DIR}/cmake/tests/CheckOpenCvTest.cmake)
add_subdirectory(cmake/tests/onnx-runtime)
add_executable(native-dependencies-test cmake/tests/NativeDependenciesTest.cpp)
target_link_libraries(native-dependencies-test PRIVATE GTest::gtest_main
    Drogon::Drogon opencv_imgcodecs opencv_videoio
    Tesseract::libtesseract PkgConfig::ESPEAK PkgConfig::FFMPEG bytetrack)
target_compile_options(native-dependencies-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME native-dependencies COMMAND native-dependencies-test)
