file(MAKE_DIRECTORY "${TEST_BINARY_DIR}")
foreach(CHECK_VALUE IN ITEMS TRUE FALSE missing)
    file(WRITE "${TEST_BINARY_DIR}/CMakeCache.txt" "__VALID_FFMPEG:INTERNAL=${CHECK_VALUE}\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -DOPENCV_BINARY_DIR=${TEST_BINARY_DIR}
            -P "${CMAKE_CURRENT_LIST_DIR}/../CheckOpenCv.cmake"
        RESULT_VARIABLE CHECK_RESULT OUTPUT_QUIET ERROR_QUIET)
    if(CHECK_VALUE STREQUAL "TRUE")
        if(NOT CHECK_RESULT EQUAL 0)
            message(FATAL_ERROR "Valid FFmpeg integration was rejected")
        endif()
    elseif(CHECK_RESULT EQUAL 0)
        message(FATAL_ERROR "Invalid FFmpeg integration was accepted: ${CHECK_VALUE}")
    endif()
endforeach()
