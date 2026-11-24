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
