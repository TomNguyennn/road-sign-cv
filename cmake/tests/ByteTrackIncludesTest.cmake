set(FIXTURE_DIR "${TEST_BINARY_DIR}/deploy/ncnn/cpp/src")
file(MAKE_DIRECTORY "${FIXTURE_DIR}")
foreach(SOURCE_NAME IN ITEMS BYTETracker.cpp utils.cpp)
    file(WRITE "${FIXTURE_DIR}/${SOURCE_NAME}" "// original source body\n")
endforeach()
file(APPEND "${FIXTURE_DIR}/BYTETracker.cpp"
    "detections_low.push_back(strack);\nremove_duplicate_stracks(resa, resb, this->tracked_stracks, this->lost_stracks);\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -DBYTETRACK_SOURCE_DIR=${TEST_BINARY_DIR}
    -P "${CMAKE_CURRENT_LIST_DIR}/../ByteTrackIncludes.cmake"
    COMMAND_ERROR_IS_FATAL ANY)
file(READ "${FIXTURE_DIR}/BYTETracker.cpp" TRACKER_FIRST)
file(READ "${FIXTURE_DIR}/utils.cpp" UTILS_FIRST)
execute_process(COMMAND "${CMAKE_COMMAND}" -DBYTETRACK_SOURCE_DIR=${TEST_BINARY_DIR}
    -P "${CMAKE_CURRENT_LIST_DIR}/../ByteTrackIncludes.cmake"
    COMMAND_ERROR_IS_FATAL ANY)
file(READ "${FIXTURE_DIR}/BYTETracker.cpp" TRACKER_SECOND)
file(READ "${FIXTURE_DIR}/utils.cpp" UTILS_SECOND)
if(NOT TRACKER_FIRST STREQUAL TRACKER_SECOND OR NOT UTILS_FIRST STREQUAL UTILS_SECOND)
    message(FATAL_ERROR "Applying the tracker patch twice changed source content")
endif()
foreach(CONTENT IN ITEMS "${TRACKER_SECOND}" "${UTILS_SECOND}")
    if(NOT CONTENT MATCHES "#include <iostream>" OR NOT CONTENT MATCHES "// original source body")
        message(FATAL_ERROR "The patch must add iostream and preserve the original source")
    endif()
endforeach()
if(NOT UTILS_SECOND MATCHES "#include <map>")
    message(FATAL_ERROR "Tracker utilities require map")
endif()
if(NOT TRACKER_SECOND MATCHES "score >= 0.10f" OR
   NOT TRACKER_SECOND MATCHES "removed_stracks.size\\(\\) > 100")
    message(FATAL_ERROR "Tracker patch must filter low scores and bound removed history")
endif()
