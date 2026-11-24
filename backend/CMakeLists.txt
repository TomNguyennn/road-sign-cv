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
target_link_libraries(runtime-foundation-test PRIVATE GTest::gtest_main)
target_compile_options(runtime-foundation-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME runtime-foundation COMMAND runtime-foundation-test)

add_library(recognition-session STATIC
    modules/recognition-session/application/SessionService.cpp
    modules/recognition-session/infrastructure/SessionExecutor.cpp)
target_include_directories(recognition-session PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/recognition-session/application"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/recognition-session/domain")
target_link_libraries(recognition-session PRIVATE uuid)
target_compile_options(recognition-session PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(session-foundation-test
    modules/recognition-session/tests/SessionFoundationTest.cpp)
target_link_libraries(session-foundation-test PRIVATE recognition-session GTest::gtest_main)
target_compile_options(session-foundation-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME session-foundation COMMAND session-foundation-test)

add_executable(session-executor-test
    modules/recognition-session/tests/SessionExecutorTest.cpp)
target_link_libraries(session-executor-test PRIVATE recognition-session GTest::gtest_main)
target_compile_options(session-executor-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME session-executor COMMAND session-executor-test)
