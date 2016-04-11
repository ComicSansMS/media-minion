cmake_minimum_required(VERSION 3.2)

project(media-minion)

if(WIN32)
    set(FFMPEG_ROOT ${PROJECT_SOURCE_DIR}/external/ffmpeg CACHE PATH "ffmpeg installation root directory.")
    find_path(FFMPEG_INCLUDE_DIRECTORY
        NAMES libavcodec/avcodec.h
        HINTS ${FFMPEG_ROOT}/include
    )
    find_library(FFMPEG_AVUTIL_LIBRARY avutil
        HINTS ${FFMPEG_ROOT}/bin ${FFMPEG_ROOT}/lib
    )
    find_library(FFMPEG_SWRESAMPLE_LIBRARY swresample
        HINTS ${FFMPEG_ROOT}/bin ${FFMPEG_ROOT}/lib
    )
    find_library(FFMPEG_AVCODEC_LIBRARY avcodec
        HINTS ${FFMPEG_ROOT}/bin ${FFMPEG_ROOT}/lib
    )
    find_library(FFMPEG_AVFORMAT_LIBRARY avformat
        HINTS ${FFMPEG_ROOT}/bin ${FFMPEG_ROOT}/lib
    )
    set(FFMPEG_LIBRARIES
        ${FFMPEG_AVUTIL_LIBRARY}
        ${FFMPEG_SWRESAMPLE_LIBRARY}
        ${FFMPEG_AVCODEC_LIBRARY}
        ${FFMPEG_AVFORMAT_LIBRARY}
    )

    find_file(FFMPEG_AVUTIL_DLL
        NAMES avutil-55.dll
        HINTS ${FFMPEG_ROOT}/bin
    )
    find_file(FFMPEG_SWRESAMPLE_DLL
        NAMES swresample-2.dll
        HINTS ${FFMPEG_ROOT}/bin
    )
    find_file(FFMPEG_AVCODEC_DLL
        NAMES avcodec-57.dll
        HINTS ${FFMPEG_ROOT}/bin
    )
    find_file(FFMPEG_AVFORMAT_DLL
        NAMES avformat-57.dll
        HINTS ${FFMPEG_ROOT}/bin
    )
    set(FFMPEG_DLLS
        ${FFMPEG_AVUTIL_DLL}
        ${FFMPEG_SWRESAMPLE_DLL}
        ${FFMPEG_AVCODEC_DLL}
        ${FFMPEG_AVFORMAT_DLL}
    )
endif()

set(GSL_ROOT ${PROJECT_SOURCE_DIR}/external/GSL CACHE PATH "GSL: Guidelines Support Library root directory.")
find_path(GSL_INCLUDE_DIRECTORY NAMES gsl.h HINTS ${GSL_ROOT}/include)


set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED)
add_definitions(-DBOOST_ALL_NO_LIB)

set(GHULBUS_BASE_ROOT "" CACHE PATH "GhulbusBase installation root directory.")
if(GHULBUS_BASE_ROOT)
    list(APPEND CMAKE_PREFIX_PATH ${GHULBUS_BASE_ROOT}/cmake)
endif()
find_package(gbBase REQUIRED)

add_executable(demo
    ${PROJECT_SOURCE_DIR}/src/demo.cpp
    ${PROJECT_SOURCE_DIR}/src/StreamIOContext.cpp
    ${PROJECT_SOURCE_DIR}/src/StreamIOContext.hpp
)
target_include_directories(demo PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRECTORY}
    ${GSL_INCLUDE_DIRECTORY}
    ${Boost_INCLUDE_DIR}
)
target_link_libraries(demo PRIVATE ${FFMPEG_LIBRARIES} Ghulbus::gbBase)
if(WIN32)
    file(COPY ${FFMPEG_DLLS} DESTINATION ${PROJECT_BINARY_DIR})
endif()
