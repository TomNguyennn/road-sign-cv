file(STRINGS "${OPENCV_BINARY_DIR}/CMakeCache.txt" FFMPEG_LINK_CHECK
    REGEX "^__VALID_FFMPEG:INTERNAL=(TRUE|1)$")
if(NOT FFMPEG_LINK_CHECK)
    message(FATAL_ERROR "OpenCV's FFmpeg compile/link check failed; video support is required.")
endif()
