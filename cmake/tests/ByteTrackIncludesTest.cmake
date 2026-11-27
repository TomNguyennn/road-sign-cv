set(FIXTURE_DIR "${TEST_BINARY_DIR}/deploy/ncnn/cpp/src")
file(MAKE_DIRECTORY "${FIXTURE_DIR}")
file(MAKE_DIRECTORY "${TEST_BINARY_DIR}/deploy/ncnn/cpp/include")
foreach(SOURCE_NAME IN ITEMS BYTETracker.cpp STrack.cpp utils.cpp)
    file(WRITE "${FIXTURE_DIR}/${SOURCE_NAME}" "// original source body\n")
endforeach()
file(APPEND "${FIXTURE_DIR}/BYTETracker.cpp"
    "detections_low.push_back(strack);\nremove_duplicate_stracks(resa, resb, this->tracked_stracks, this->lost_stracks);\nframe_id = 0;\nBYTETracker::update(const vector<Object>& objects)\nthis->frame_id++;\nSTrack::multi_predict(strack_pool, this->kalman_filter);\n")
file(APPEND "${FIXTURE_DIR}/STrack.cpp"
    "\tif (frame_id == 1)\n\t{\n\t\tthis->is_activated = true;\n\t}\n\t//this->is_activated = true;\n\tthis->frame_id = frame_id;\nmulti_predict(vector<STrack*> &stracks, byte_kalman::KalmanFilter &kalman_filter)\nkalman_filter.predict(stracks[i]->mean, stracks[i]->covariance);\n")
file(WRITE "${TEST_BINARY_DIR}/deploy/ncnn/cpp/include/BYTETracker.h"
    "update(const vector<Object>& objects);\nint frame_id;\n")
file(WRITE "${TEST_BINARY_DIR}/deploy/ncnn/cpp/include/STrack.h"
    "multi_predict(vector<STrack*> &stracks, byte_kalman::KalmanFilter &kalman_filter);\n")
file(WRITE "${TEST_BINARY_DIR}/deploy/ncnn/cpp/include/kalmanFilter.h"
    "void predict(KAL_MEAN& mean, KAL_COVA& covariance);\n")
file(WRITE "${FIXTURE_DIR}/kalmanFilter.cpp"
    "void KalmanFilter::predict(KAL_MEAN &mean, KAL_COVA &covariance)\nKAL_MEAN mean1 = this->_motion_mat * mean.transpose();\nKAL_COVA covariance1 = this->_motion_mat * covariance *(_motion_mat.transpose());\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -DBYTETRACK_SOURCE_DIR=${TEST_BINARY_DIR}
    -P "${CMAKE_CURRENT_LIST_DIR}/../ByteTrackIncludes.cmake"
    COMMAND_ERROR_IS_FATAL ANY)
file(READ "${FIXTURE_DIR}/BYTETracker.cpp" TRACKER_FIRST)
file(READ "${FIXTURE_DIR}/STrack.cpp" STRACK_FIRST)
file(READ "${FIXTURE_DIR}/utils.cpp" UTILS_FIRST)
execute_process(COMMAND "${CMAKE_COMMAND}" -DBYTETRACK_SOURCE_DIR=${TEST_BINARY_DIR}
    -P "${CMAKE_CURRENT_LIST_DIR}/../ByteTrackIncludes.cmake"
    COMMAND_ERROR_IS_FATAL ANY)
file(READ "${FIXTURE_DIR}/BYTETracker.cpp" TRACKER_SECOND)
file(READ "${FIXTURE_DIR}/STrack.cpp" STRACK_SECOND)
file(READ "${FIXTURE_DIR}/utils.cpp" UTILS_SECOND)
if(NOT TRACKER_FIRST STREQUAL TRACKER_SECOND OR
   NOT STRACK_FIRST STREQUAL STRACK_SECOND OR NOT UTILS_FIRST STREQUAL UTILS_SECOND)
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
if(STRACK_SECOND MATCHES "frame_id == 1" OR
   NOT STRACK_SECOND MATCHES "this->is_activated = true;")
    message(FATAL_ERROR "New high-score tracks must be visible after time advancement")
endif()
if(NOT TRACKER_SECOND MATCHES "elapsedFrames" OR
   NOT STRACK_SECOND MATCHES "elapsedFrames")
    message(FATAL_ERROR "Tracker motion must use elapsed source time")
endif()
