include_guard(GLOBAL)

function(add_road_sign_module module_name)
    cmake_parse_arguments(MODULE "" "" "SOURCES;PUBLIC_DEPENDENCIES" ${ARGN})
    if(NOT MODULE_SOURCES)
        message(FATAL_ERROR "Module ${module_name} must define its sources")
    endif()

    set(module_root "${CMAKE_CURRENT_SOURCE_DIR}/modules/${module_name}")
    if(NOT IS_DIRECTORY "${module_root}/public")
        message(FATAL_ERROR "Module ${module_name} must have a public interface directory")
    endif()

    add_library("${module_name}" STATIC ${MODULE_SOURCES})
    target_include_directories("${module_name}" PUBLIC "${module_root}/public")
    target_include_directories("${module_name}" PRIVATE "${module_root}")
    foreach(dependency IN LISTS MODULE_PUBLIC_DEPENDENCIES)
        if(NOT TARGET "${dependency}")
            message(FATAL_ERROR "Module ${module_name} depends on unknown module ${dependency}")
        endif()
    endforeach()
    target_link_libraries("${module_name}" PUBLIC ${MODULE_PUBLIC_DEPENDENCIES})
endfunction()

set(ROAD_SIGN_MODULE_DEPENDENCIES
    "" CACHE STRING "Semicolon-separated public target dependencies for module-boundary checks")
if(ROAD_SIGN_MODULE_DEPENDENCIES)
    target_link_libraries(road-sign-backend INTERFACE ${ROAD_SIGN_MODULE_DEPENDENCIES})
endif()

add_executable(runtime-foundation-test tests/integration/RuntimeFoundationTest.cpp)
target_include_directories(runtime-foundation-test PRIVATE "${CMAKE_SOURCE_DIR}/backend")
add_library(road-sign-runtime-config STATIC
    infrastructure/RuntimeConfig.cpp
    infrastructure/Logger.cpp)
target_include_directories(road-sign-runtime-config PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(road-sign-runtime-config PUBLIC Drogon::Drogon)
target_compile_options(road-sign-runtime-config PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(runtime-foundation-test PRIVATE GTest::gtest_main road-sign-runtime-config)
target_compile_options(runtime-foundation-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME runtime-foundation COMMAND runtime-foundation-test)

add_library(media-processing STATIC
    modules/media-processing/infrastructure/OpenCvImageClient.cpp)
target_include_directories(media-processing PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(media-processing PUBLIC opencv_core opencv_imgproc opencv_imgcodecs)
target_compile_options(media-processing PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_library(recognition-session STATIC
    modules/recognition-session/application/SessionService.cpp
    modules/recognition-session/infrastructure/SessionExecutor.cpp
    modules/recognition-session/infrastructure/SessionEventBuffer.cpp
    modules/recognition-session/infrastructure/SessionController.cpp
    modules/recognition-session/infrastructure/MediaAdmissionController.cpp
    modules/recognition-session/infrastructure/SessionEventsController.cpp)
target_include_directories(recognition-session PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/recognition-session/application"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/recognition-session/domain")
target_link_libraries(recognition-session PRIVATE uuid opencv_videoio media-processing PUBLIC Drogon::Drogon)
target_compile_options(recognition-session PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(session-foundation-test
    modules/recognition-session/tests/SessionFoundationTest.cpp)
target_link_libraries(session-foundation-test PRIVATE recognition-session GTest::gtest_main)
target_compile_options(session-foundation-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME session-foundation COMMAND session-foundation-test)

add_executable(session-event-buffer-test
    modules/recognition-session/tests/SessionEventBufferTest.cpp)
target_link_libraries(session-event-buffer-test PRIVATE recognition-session GTest::gtest_main)
target_compile_options(session-event-buffer-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME session-event-buffer COMMAND session-event-buffer-test)

add_executable(session-executor-test
    modules/recognition-session/tests/SessionExecutorTest.cpp)
target_link_libraries(session-executor-test PRIVATE recognition-session GTest::gtest_main)
target_compile_options(session-executor-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME session-executor COMMAND session-executor-test)

add_executable(native-recognition-test tests/integration/NativeRecognitionTest.cpp)
target_link_libraries(native-recognition-test PRIVATE media-processing GTest::gtest_main)
target_compile_options(native-recognition-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME native-recognition COMMAND native-recognition-test)

add_library(announcements STATIC
    modules/announcements/domain/AcceptImageInformationUseCase.cpp
    modules/announcements/domain/ConsolidateInformationUseCase.cpp
    modules/announcements/application/AnnouncementService.cpp)
target_include_directories(announcements PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(announcements PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(obstacle-announcement-test
    modules/announcements/tests/ObstacleAnnouncementTest.cpp)
target_link_libraries(obstacle-announcement-test PRIVATE announcements GTest::gtest_main)
target_compile_options(obstacle-announcement-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME obstacle-announcement COMMAND obstacle-announcement-test)
add_executable(consolidate-information-test
    modules/announcements/tests/ConsolidateInformationUseCaseTest.cpp)
target_link_libraries(consolidate-information-test PRIVATE announcements GTest::gtest_main)
target_compile_options(consolidate-information-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME consolidate-information COMMAND consolidate-information-test)
add_executable(announcement-policy-test
    modules/announcements/tests/AnnouncementPolicyTest.cpp)
target_link_libraries(announcement-policy-test PRIVATE announcements GTest::gtest_main)
target_compile_options(announcement-policy-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME announcement-policy COMMAND announcement-policy-test)

add_library(sign-recognition STATIC
    modules/sign-recognition/domain/TrackedSign.cpp
    modules/sign-recognition/infrastructure/ByteTrackClient.cpp)
target_include_directories(sign-recognition PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(sign-recognition PRIVATE bytetrack)
target_compile_options(sign-recognition PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(tracking-test
    modules/sign-recognition/tests/TrackingTest.cpp)
target_link_libraries(tracking-test PRIVATE sign-recognition GTest::gtest_main)
target_compile_options(tracking-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME tracking COMMAND tracking-test)
add_executable(bytetrack-client-test
    modules/sign-recognition/tests/ByteTrackClientTest.cpp)
target_link_libraries(bytetrack-client-test PRIVATE sign-recognition GTest::gtest_main)
target_compile_options(bytetrack-client-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME bytetrack-client COMMAND bytetrack-client-test)

add_executable(road-sign-server app/Main.cpp)
target_include_directories(road-sign-server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(road-sign-server PRIVATE recognition-session road-sign-runtime-config Drogon::Drogon)
target_compile_options(road-sign-server PRIVATE -Wall -Wextra -Wpedantic -Werror)
