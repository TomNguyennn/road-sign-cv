include(ExternalProject)
set(NATIVE_COMMON_ARGS
    -DCMAKE_INSTALL_PREFIX=${ROAD_SIGN_NATIVE_PREFIX}
    -DCMAKE_INSTALL_LIBDIR=lib
    -DCMAKE_INSTALL_RPATH=$ORIGIN/../lib
    -DCMAKE_PREFIX_PATH=${ROAD_SIGN_NATIVE_PREFIX}
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DBUILD_SHARED_LIBS=ON
    -DBUILD_TESTING=OFF)

function(nativeCmake name url hash)
    cmake_parse_arguments(DEP "" "" "ARGS;DEPENDS" ${ARGN})
    ExternalProject_Add(${name}
        URL "${url}" URL_HASH "SHA256=${hash}"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE TLS_VERIFY TRUE
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
            PKG_CONFIG_PATH=${ROAD_SIGN_NATIVE_PREFIX}/lib/pkgconfig
            ${CMAKE_COMMAND} -S <SOURCE_DIR> -B <BINARY_DIR> -G Ninja
            ${NATIVE_COMMON_ARGS} ${DEP_ARGS}
        DEPENDS ${DEP_DEPENDS}
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --parallel 4
        INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>)
endfunction()

nativeCmake(jsoncpp
    https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/1.9.6.tar.gz
    f93b6dd7ce796b13d02c108bc9f79812245a82e577581c4c9aabe57075c90ea2
    ARGS -DJSONCPP_WITH_TESTS=OFF -DJSONCPP_WITH_POST_BUILD_UNITTEST=OFF)
nativeCmake(trantor
    https://github.com/an-tao/trantor/archive/refs/tags/v1.5.22.tar.gz
    2f870b016a592228d617ef51eec4e9a9ab7dc56c066923af9bf6dd42fefb63de
    ARGS -DBUILD_C-ARES=OFF)
nativeCmake(drogon
    https://github.com/drogonframework/drogon/archive/refs/tags/v1.9.11.tar.gz
    f50098bb21bd0013f8da16b796313816bf79b0ecb1d74bfe33216d5400ab2002
    DEPENDS jsoncpp trantor
    ARGS -DUSE_SUBMODULE=OFF -DBUILD_CTL=OFF -DBUILD_EXAMPLES=OFF
         -DBUILD_ORM=OFF -DBUILD_BROTLI=OFF -DBUILD_YAML_CONFIG=OFF)
nativeCmake(eigen
    https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
    8586084f71f9bde545ee7fa6d00288b264a2b7ac3607b974e54d13e7162c1c72)
nativeCmake(googletest
    https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c)
nativeCmake(leptonica
    https://github.com/DanBloomberg/leptonica/releases/download/1.85.0/leptonica-1.85.0.tar.gz
    3745ae3bf271a6801a2292eead83ac926e3a9bc1bf622e9cd4dd0f3786e17205
    ARGS -DBUILD_PROG=OFF -DENABLE_GIF=OFF -DENABLE_TIFF=OFF
         -DENABLE_WEBP=OFF -DENABLE_OPENJPEG=OFF)
nativeCmake(tesseract
    https://github.com/tesseract-ocr/tesseract/archive/refs/tags/5.5.0.tar.gz
    f2fb34ca035b6d087a42875a35a7a5c4155fa9979c6132365b1e5a28ebc3fc11
    DEPENDS leptonica
    ARGS -DBUILD_TRAINING_TOOLS=OFF -DGRAPHICS_DISABLED=ON -DENABLE_NATIVE=OFF
         -DDISABLE_ARCHIVE=ON -DDISABLE_CURL=ON -DDISABLE_TIFF=ON)

# Supply Sonic explicitly so eSpeak cannot fetch its own Git dependency.
ExternalProject_Add(sonic-source
    URL https://github.com/waywardgeek/sonic/archive/fbf75c3d6d846bad3bb3d456cbc5d07d9fd8c104.tar.gz
    URL_HASH SHA256=715827b5a39b79e56e44397d7b845910df996d4cca74777b3b61629b1ddc98c1
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE TLS_VERIFY TRUE
    CONFIGURE_COMMAND "" BUILD_COMMAND "" INSTALL_COMMAND "")
ExternalProject_Get_Property(sonic-source SOURCE_DIR)
nativeCmake(espeak
    https://github.com/espeak-ng/espeak-ng/archive/refs/tags/1.52.0.tar.gz
    bb4338102ff3b49a81423da8a1a158b420124b055b60fa76cfb4b18677130a23
    DEPENDS sonic-source
    ARGS -DFETCHCONTENT_SOURCE_DIR_SONIC-GIT=${SOURCE_DIR}
         -DUSE_LIBPCAUDIO=OFF -DUSE_MBROLA=OFF)

ExternalProject_Add(ffmpeg
    URL https://ffmpeg.org/releases/ffmpeg-7.1.2.tar.xz
    URL_HASH SHA256=089bc60fb59d6aecc5d994ff530fd0dcb3ee39aa55867849a2bbc4e555f9c304
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE TLS_VERIFY TRUE
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${ROAD_SIGN_NATIVE_PREFIX}
        --cc=${CMAKE_C_COMPILER} --cxx=${CMAKE_CXX_COMPILER}
        --enable-shared --enable-rpath --disable-static --disable-programs --disable-doc
        --disable-autodetect --disable-network --disable-everything
        --enable-avcodec --enable-avformat --enable-swscale
        --enable-decoder=h264,vp8,vp9 --enable-parser=h264,vp8,vp9
        --enable-demuxer=mov,matroska --enable-protocol=file
    BUILD_COMMAND make -j4 INSTALL_COMMAND make install)
nativeCmake(opencv
    https://github.com/opencv/opencv/archive/refs/tags/4.12.0.tar.gz
    44c106d5bb47efec04e531fd93008b3fcd1d27138985c5baf4eafac0e1ec9e9d
    DEPENDS ffmpeg
    ARGS -DBUILD_LIST=core,imgproc,imgcodecs,videoio -DBUILD_TESTS=OFF
         -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_opencv_apps=OFF
         -DWITH_FFMPEG=ON -DWITH_IPP=OFF -DWITH_ITT=OFF -DWITH_OPENCL=OFF
         -DWITH_GSTREAMER=OFF -DWITH_V4L=OFF -DWITH_GTK=OFF -DWITH_QT=OFF
         -DWITH_OPENEXR=OFF -DWITH_TIFF=OFF -DWITH_WEBP=OFF
         -DWITH_OPENJPEG=OFF -DWITH_JASPER=OFF -DWITH_AVIF=OFF
         -DOPENCV_GENERATE_PKGCONFIG=ON)
ExternalProject_Add_Step(opencv verify-video
    COMMAND ${CMAKE_COMMAND} -DOPENCV_BINARY_DIR=<BINARY_DIR>
        -P ${CMAKE_CURRENT_LIST_DIR}/CheckOpenCv.cmake
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/CheckOpenCv.cmake
    DEPENDEES configure DEPENDERS build)

# Retain tracker-only sources; the upstream ncnn demo is not part of this build.
ExternalProject_Add(bytetrack-source
    URL https://github.com/FoundationVision/ByteTrack/archive/d1bf0191adff59bc8fcfeaa0b33d3d1642552a99.tar.gz
    URL_HASH SHA256=a04446567ebd13bb611849029222cd4edcc9ec009bd6f1e92fa5f4281c95359c
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE TLS_VERIFY TRUE
    PATCH_COMMAND ${CMAKE_COMMAND} -DBYTETRACK_SOURCE_DIR=<SOURCE_DIR>
        -P ${CMAKE_CURRENT_LIST_DIR}/ByteTrackIncludes.cmake
    CONFIGURE_COMMAND "" BUILD_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory
        <SOURCE_DIR>/deploy/ncnn/cpp ${ROAD_SIGN_NATIVE_PREFIX}/share/bytetrack)
ExternalProject_Add_StepDependencies(bytetrack-source patch
    ${CMAKE_CURRENT_LIST_DIR}/ByteTrackIncludes.cmake)
