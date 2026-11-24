if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED TEST_BINARY_DIR OR NOT DEFINED TEST_CXX_COMPILER)
    message(FATAL_ERROR "SOURCE_ROOT, TEST_BINARY_DIR and TEST_CXX_COMPILER are required")
endif()

file(REMOVE_RECURSE "${TEST_BINARY_DIR}")
file(MAKE_DIRECTORY
    "${TEST_BINARY_DIR}/modules/recognition-session/public"
    "${TEST_BINARY_DIR}/modules/recognition-session/infrastructure"
    "${TEST_BINARY_DIR}/modules/media-processing/public")
file(WRITE "${TEST_BINARY_DIR}/modules/media-processing/public/MediaSource.hpp"
    "#pragma once\nstruct MediaSource {};\n")
file(WRITE "${TEST_BINARY_DIR}/modules/media-processing/infrastructure/PrivateAdapter.hpp"
    "#pragma once\nstruct PrivateAdapter {};\n")
file(WRITE "${TEST_BINARY_DIR}/modules/recognition-session/public/SessionApi.hpp"
    "#pragma once\n#include <MediaSource.hpp>\n")
file(WRITE "${TEST_BINARY_DIR}/modules/recognition-session/infrastructure/PublicConsumer.cpp"
    "#include <SessionApi.hpp>\nMediaSource makeSource() { return {}; }\n")
file(WRITE "${TEST_BINARY_DIR}/modules/recognition-session/infrastructure/PrivateConsumer.cpp"
    "#include <PrivateAdapter.hpp>\nint main() { return sizeof(PrivateAdapter); }\n")
file(WRITE "${TEST_BINARY_DIR}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.28)\nproject(ModuleBoundaryProbe LANGUAGES CXX)\n"
    "add_library(media-processing INTERFACE)\n"
    "target_include_directories(media-processing INTERFACE modules/media-processing/public)\n"
    "add_library(recognition-public OBJECT modules/recognition-session/infrastructure/PublicConsumer.cpp)\n"
    "target_include_directories(recognition-public PRIVATE modules/recognition-session/public)\n"
    "target_link_libraries(recognition-public PRIVATE media-processing)\n"
    "add_library(recognition-private OBJECT modules/recognition-session/infrastructure/PrivateConsumer.cpp)\n"
    "target_include_directories(recognition-private PRIVATE modules/recognition-session/public)\n")

execute_process(COMMAND "${CMAKE_COMMAND}" -S "${TEST_BINARY_DIR}" -B "${TEST_BINARY_DIR}/build"
    -DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}
    RESULT_VARIABLE configure_result OUTPUT_VARIABLE configure_output ERROR_VARIABLE configure_error)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "Boundary fixture configuration failed: ${configure_output}${configure_error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${TEST_BINARY_DIR}/build" --target recognition-public
    RESULT_VARIABLE public_result OUTPUT_VARIABLE public_output ERROR_VARIABLE public_error)
if(NOT public_result EQUAL 0)
    message(FATAL_ERROR "Public module interface was not consumable: ${public_output}${public_error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${TEST_BINARY_DIR}/build" --target recognition-private
    RESULT_VARIABLE private_result OUTPUT_VARIABLE private_output ERROR_VARIABLE private_error)
if(private_result EQUAL 0 OR NOT "${private_output}${private_error}" MATCHES "PrivateAdapter.hpp")
    message(FATAL_ERROR "Private infrastructure import was not rejected as expected")
endif()
