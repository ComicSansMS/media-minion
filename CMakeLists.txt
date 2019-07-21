cmake_minimum_required(VERSION 3.14)

project(media-minion)

if(WIN32)
    #set(FFMPEG_ROOT ${PROJECT_SOURCE_DIR}/external/ffmpeg CACHE PATH "ffmpeg installation root directory.")
    set(FFMPEG_ROOT $ENV{FFMPEG_ROOT})
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
        NAMES avutil-55.dll avutil-56.dll
        HINTS ${FFMPEG_ROOT}/bin
    )
    find_file(FFMPEG_SWRESAMPLE_DLL
        NAMES swresample-2.dll swresample-3.dll
        HINTS ${FFMPEG_ROOT}/bin
    )
    find_file(FFMPEG_AVCODEC_DLL
        NAMES avcodec-57.dll avcodec-58.dll
        HINTS ${FFMPEG_ROOT}/bin
    )
    find_file(FFMPEG_AVFORMAT_DLL
        NAMES avformat-57.dll avformat-58.dll
        HINTS ${FFMPEG_ROOT}/bin
    )
    set(FFMPEG_DLLS
        ${FFMPEG_AVUTIL_DLL}
        ${FFMPEG_SWRESAMPLE_DLL}
        ${FFMPEG_AVCODEC_DLL}
        ${FFMPEG_AVFORMAT_DLL}
    )
endif()

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS system)
add_definitions(-DBOOST_ALL_NO_LIB)

find_package(Qt5 REQUIRED COMPONENTS Widgets
    HINTS $ENV{QT5_ROOT}/5.12.2/msvc2017_64
)

set(GHULBUS_BASE_ROOT "" CACHE PATH "GhulbusBase installation root directory.")
if(GHULBUS_BASE_ROOT)
    list(APPEND CMAKE_PREFIX_PATH ${GHULBUS_BASE_ROOT}/cmake)
endif()
find_package(gbBase REQUIRED)

set(GHULBUS_AUDIO_ROOT "" CACHE PATH "GhulbusAudio installation root directory.")
if(GHULBUS_AUDIO_ROOT)
    list(APPEND CMAKE_PREFIX_PATH ${GHULBUS_AUDIO_ROOT}/cmake)
endif()
find_package(gbAudio REQUIRED)

add_executable(demo
    ${PROJECT_SOURCE_DIR}/src/demo.cpp
    ${PROJECT_SOURCE_DIR}/src/StreamIOContext.cpp
    ${PROJECT_SOURCE_DIR}/src/StreamIOContext.hpp
)
target_include_directories(demo PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRECTORY}
    ${Boost_INCLUDE_DIR}
)
target_link_libraries(demo PRIVATE ${FFMPEG_LIBRARIES} Ghulbus::gbBase Ghulbus::gbAudio)


set(MM_PLAYER_SOURCE_DIRECTORY ${PROJECT_SOURCE_DIR}/src/player)

set(MM_PLAYER_SOURCE_FILES
    ${MM_PLAYER_SOURCE_DIRECTORY}/mm_player.cpp
)

set(MM_PLAYER_HEADER_FILES
)

set(MM_PLAYER_QT_SOURCE_FILES
    ${MM_PLAYER_SOURCE_DIRECTORY}/ui/player_tray_icon.cpp
)

set(MM_PLAYER_QT_HEADER_FILES
)

set(MM_PLAYER_QT_MOC_HEADER_FILES
    ${MM_PLAYER_SOURCE_DIRECTORY}/ui/player_tray_icon.hpp
)
qt5_wrap_cpp(MM_PLAYER_QT_MOC_SOURCE_FILES ${MM_PLAYER_QT_MOC_HEADER_FILES})

source_group(moc FILES
    ${MM_PLAYER_QT_MOC_SOURCE_FILES}
)

add_executable(mm_player WIN32
    ${MM_PLAYER_SOURCE_FILES}
    ${MM_PLAYER_HEADER_FILES}
    ${MM_PLAYER_QT_SOURCE_FILES}
    ${MM_PLAYER_QT_HEADER_FILES}
    ${MM_PLAYER_QT_MOC_HEADER_FILES}
    ${MM_PLAYER_QT_MOC_SOURCE_FILES}
)

target_include_directories(mm_player PUBLIC ${MM_PLAYER_SOURCE_DIRECTORY})
target_link_libraries(mm_player PUBLIC
    Boost::disable_autolinking
    Boost::system
    Ghulbus::gbBase
    Ghulbus::gbAudio
    Qt5::Core
    Qt5::Widgets
)


if(WIN32)
    function(getPDBForDLL DLL_PATH OUT_VAR)
        get_filename_component(dll_dir ${DLL_PATH} DIRECTORY)
        get_filename_component(dll_we ${DLL_PATH} NAME_WE)
        set(${OUT_VAR} "${dll_dir}/${dll_we}.pdb" PARENT_SCOPE)
    endfunction()


    function(getQt5Dlls QT_TARGET OUT_VAR)
        unset(DLLS)
        get_property(tmp TARGET ${QT_TARGET} PROPERTY IMPORTED_LOCATION_DEBUG)
        list(APPEND DLLS ${tmp})
        getPDBForDLL(${tmp} tmp_pdb)
        if (EXISTS ${tmp_pdb})
            list(APPEND DLLS ${tmp_pdb})
        endif()
        get_property(tmp TARGET ${QT_TARGET} PROPERTY IMPORTED_LOCATION_RELEASE)
        list(APPEND DLLS ${tmp})
        get_property(deps TARGET ${QT_TARGET} PROPERTY IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG)
        foreach(dep ${deps})
            if(TARGET ${dep})
                getQt5Dlls(${dep} tmp)
                list(APPEND DLLS ${tmp})
            endif()
        endforeach()
        set(result ${${OUT_VAR}})
        list(APPEND result ${DLLS})
        list(REMOVE_DUPLICATES result)
        set(${OUT_VAR} ${result} PARENT_SCOPE)
    endfunction()

    getQt5Dlls(Qt5::Widgets qt_DLLS)

    file(COPY ${qt_DLLS} ${FFMPEG_DLLS} DESTINATION ${PROJECT_BINARY_DIR})

endif()
