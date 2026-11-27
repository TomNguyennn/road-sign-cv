# The pinned ncnn sample relied on transitive standard-library includes.
function(addByteTrackInclude sourceName header)
    set(SOURCE_PATH "${BYTETRACK_SOURCE_DIR}/deploy/ncnn/cpp/src/${sourceName}")
    file(READ "${SOURCE_PATH}" SOURCE_CONTENT)
    string(FIND "${SOURCE_CONTENT}" "#include <${header}>" INCLUDE_POSITION)
    if(INCLUDE_POSITION EQUAL -1)
        file(WRITE "${SOURCE_PATH}" "#include <${header}>\n${SOURCE_CONTENT}")
    endif()
endfunction()

addByteTrackInclude(BYTETracker.cpp iostream)
addByteTrackInclude(utils.cpp iostream)
addByteTrackInclude(utils.cpp map)

set(TRACKER_PATH "${BYTETRACK_SOURCE_DIR}/deploy/ncnn/cpp/src/BYTETracker.cpp")
file(READ "${TRACKER_PATH}" TRACKER_SOURCE)
function(replaceTrackerText original replacement)
    string(FIND "${TRACKER_SOURCE}" "${original}" ORIGINAL_POSITION)
    string(FIND "${TRACKER_SOURCE}" "${replacement}" REPLACEMENT_POSITION)
    if(ORIGINAL_POSITION EQUAL -1 AND REPLACEMENT_POSITION EQUAL -1)
        message(FATAL_ERROR "Pinned ByteTrack source changed; review the tracker patch")
    endif()
    if(REPLACEMENT_POSITION EQUAL -1)
        string(REPLACE "${original}" "${replacement}" TRACKER_SOURCE "${TRACKER_SOURCE}")
        set(TRACKER_SOURCE "${TRACKER_SOURCE}" PARENT_SCOPE)
    endif()
endfunction()

replaceTrackerText(
    "detections_low.push_back(strack);"
    "if (score >= 0.10f) detections_low.push_back(strack);")
replaceTrackerText(
    "remove_duplicate_stracks(resa, resb, this->tracked_stracks, this->lost_stracks);"
    "if (this->removed_stracks.size() > 100)\n\t\tthis->removed_stracks.erase(this->removed_stracks.begin(), this->removed_stracks.end() - 100);\n\tremove_duplicate_stracks(resa, resb, this->tracked_stracks, this->lost_stracks);")
file(WRITE "${TRACKER_PATH}" "${TRACKER_SOURCE}")

# The domain applies temporal confirmation; a new vendor track must be visible
# even when source-time advancement has already consumed tracker frames.
set(STRACK_PATH "${BYTETRACK_SOURCE_DIR}/deploy/ncnn/cpp/src/STrack.cpp")
file(READ "${STRACK_PATH}" STRACK_SOURCE)
string(REPLACE "\r\n" "\n" STRACK_SOURCE "${STRACK_SOURCE}")
set(ORIGINAL_ACTIVATION "\tif (frame_id == 1)\n\t{\n\t\tthis->is_activated = true;\n\t}")
string(FIND "${STRACK_SOURCE}" "${ORIGINAL_ACTIVATION}" ORIGINAL_POSITION)
string(FIND "${STRACK_SOURCE}" "\tthis->is_activated = true;\n\tthis->frame_id = frame_id;" REPLACEMENT_POSITION)
if(ORIGINAL_POSITION EQUAL -1 AND REPLACEMENT_POSITION EQUAL -1)
    message(FATAL_ERROR "Pinned ByteTrack activation changed; review the tracker patch")
endif()
if(NOT ORIGINAL_POSITION EQUAL -1)
    string(REPLACE "${ORIGINAL_ACTIVATION}\n\t//this->is_activated = true;"
        "\tthis->is_activated = true;" STRACK_SOURCE "${STRACK_SOURCE}")
endif()
file(WRITE "${STRACK_PATH}" "${STRACK_SOURCE}")

# Use elapsed source time for both Kalman motion and the track-loss deadline.
function(replacePinnedText relativePath original replacement)
    set(SOURCE_PATH "${BYTETRACK_SOURCE_DIR}/deploy/ncnn/cpp/${relativePath}")
    file(READ "${SOURCE_PATH}" SOURCE_CONTENT)
    string(REPLACE "\r\n" "\n" SOURCE_CONTENT "${SOURCE_CONTENT}")
    string(FIND "${SOURCE_CONTENT}" "${original}" ORIGINAL_POSITION)
    string(FIND "${SOURCE_CONTENT}" "${replacement}" REPLACEMENT_POSITION)
    if(ORIGINAL_POSITION EQUAL -1 AND REPLACEMENT_POSITION EQUAL -1)
        message(FATAL_ERROR "Pinned ByteTrack ${relativePath} changed; review the time patch")
    endif()
    if(REPLACEMENT_POSITION EQUAL -1)
        string(REPLACE "${original}" "${replacement}" SOURCE_CONTENT "${SOURCE_CONTENT}")
    endif()
    file(WRITE "${SOURCE_PATH}" "${SOURCE_CONTENT}")
endfunction()

replacePinnedText(include/BYTETracker.h
    "update(const vector<Object>& objects);"
    "update(const vector<Object>& objects, double sourceTimeMs);")
replacePinnedText(include/BYTETracker.h
    "int frame_id;"
    "int frame_id;\n\tdouble first_source_time_ms;\n\tdouble last_source_time_ms;")
replacePinnedText(src/BYTETracker.cpp
    "frame_id = 0;"
    "frame_id = 0;\n\tfirst_source_time_ms = -1.0;\n\tlast_source_time_ms = -1.0;")
replacePinnedText(src/BYTETracker.cpp
    "BYTETracker::update(const vector<Object>& objects)"
    "BYTETracker::update(const vector<Object>& objects, double sourceTimeMs)")
replacePinnedText(src/BYTETracker.cpp
    "this->frame_id++;"
    "const float elapsedFrames = last_source_time_ms < 0.0 ? 1.0f :\n\t\tstatic_cast<float>((sourceTimeMs - last_source_time_ms) * 0.03);\n\tif (first_source_time_ms < 0.0) first_source_time_ms = sourceTimeMs;\n\tframe_id = 1 + static_cast<int>((sourceTimeMs - first_source_time_ms) * 0.03);\n\tlast_source_time_ms = sourceTimeMs;")
replacePinnedText(src/BYTETracker.cpp
    "STrack::multi_predict(strack_pool, this->kalman_filter);"
    "STrack::multi_predict(strack_pool, this->kalman_filter, elapsedFrames);")
replacePinnedText(include/STrack.h
    "multi_predict(vector<STrack*> &stracks, byte_kalman::KalmanFilter &kalman_filter);"
    "multi_predict(vector<STrack*> &stracks, byte_kalman::KalmanFilter &kalman_filter, float elapsedFrames);")
replacePinnedText(src/STrack.cpp
    "multi_predict(vector<STrack*> &stracks, byte_kalman::KalmanFilter &kalman_filter)"
    "multi_predict(vector<STrack*> &stracks, byte_kalman::KalmanFilter &kalman_filter, float elapsedFrames)")
replacePinnedText(src/STrack.cpp
    "kalman_filter.predict(stracks[i]->mean, stracks[i]->covariance);"
    "kalman_filter.predict(stracks[i]->mean, stracks[i]->covariance, elapsedFrames);")
replacePinnedText(include/kalmanFilter.h
    "void predict(KAL_MEAN& mean, KAL_COVA& covariance);"
    "void predict(KAL_MEAN& mean, KAL_COVA& covariance, float elapsedFrames);")
replacePinnedText(src/kalmanFilter.cpp
    "void KalmanFilter::predict(KAL_MEAN &mean, KAL_COVA &covariance)"
    "void KalmanFilter::predict(KAL_MEAN &mean, KAL_COVA &covariance, float elapsedFrames)")
replacePinnedText(src/kalmanFilter.cpp
    "KAL_MEAN mean1 = this->_motion_mat * mean.transpose();"
    "auto motionMat = _motion_mat;\n\t\tfor (int index = 0; index < 4; ++index) motionMat(index, index + 4) = elapsedFrames;\n\t\tKAL_MEAN mean1 = motionMat * mean.transpose();")
replacePinnedText(src/kalmanFilter.cpp
    "KAL_COVA covariance1 = this->_motion_mat * covariance *(_motion_mat.transpose());"
    "KAL_COVA covariance1 = motionMat * covariance *(motionMat.transpose());")
