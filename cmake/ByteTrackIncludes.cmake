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
