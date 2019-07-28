cmake_minimum_required(VERSION 3.14)

project(media-minion)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/W4>)
add_compile_options($<$<STREQUAL:$<CXX_COMPILER_ID>,MSVC>:/permissive->)
add_compile_options($<$<STREQUAL:$<CXX_COMPILER_ID>,MSVC>:/std:c++latest>)
#add_compile_options($<$<OR:$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:Clang>>:-pedantic -Wall>)
#add_compile_options($<$<CXX_COMPILER_ID:GNU>:-pedantic -Wall>)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0601)
endif()

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
find_package(Boost REQUIRED COMPONENTS system thread)
add_definitions(-DBOOST_ALL_NO_LIB)

find_package(Qt5 REQUIRED COMPONENTS Widgets
    HINTS $ENV{QT5_ROOT}/5.12.2/msvc2017_64 $ENV{QT5_ROOT}/5.12.0/msvc2017_64
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

set(MM_INCLUDE_DIRECTORY ${PROJECT_SOURCE_DIR}/src)

#########################################################################################
#### common                                                                          ####
#########################################################################################

set(MM_COMMON_SOURCE_DIRECTORY ${PROJECT_SOURCE_DIR}/src/media_minion/common)

set(MM_COMMON_SOURCE_FILES
    ${MM_COMMON_SOURCE_DIRECTORY}/common.cpp
)

set(MM_COMMON_HEADER_FILES
    ${MM_COMMON_SOURCE_DIRECTORY}/common.hpp
)

add_library(mm_common STATIC
    ${MM_COMMON_SOURCE_FILES}
    ${MM_COMMON_HEADER_FILES}
)
target_include_directories(mm_common PUBLIC ${MM_INCLUDE_DIRECTORY})

#########################################################################################
#### server                                                                          ####
#########################################################################################

set(MM_SERVER_SOURCE_DIRECTORY ${PROJECT_SOURCE_DIR}/src/media_minion/server)
set(MM_SERVER_SOURCE_FILES
    ${MM_SERVER_SOURCE_DIRECTORY}/server.cpp
    ${MM_SERVER_SOURCE_DIRECTORY}/application.cpp
    ${MM_SERVER_SOURCE_DIRECTORY}/configuration.cpp
    ${MM_SERVER_SOURCE_DIRECTORY}/http_listener.cpp
    ${MM_SERVER_SOURCE_DIRECTORY}/http_server.cpp
    ${MM_SERVER_SOURCE_DIRECTORY}/http_session.cpp
    ${MM_SERVER_SOURCE_DIRECTORY}/websocket_session.cpp
)

set(MM_SERVER_HEADER_FILES
    ${MM_SERVER_SOURCE_DIRECTORY}/any_request.hpp
    ${MM_SERVER_SOURCE_DIRECTORY}/any_response.hpp
    ${MM_SERVER_SOURCE_DIRECTORY}/application.hpp
    ${MM_SERVER_SOURCE_DIRECTORY}/callback_return.hpp
    ${MM_SERVER_SOURCE_DIRECTORY}/configuration.hpp
    ${MM_SERVER_SOURCE_DIRECTORY}/http_listener.hpp
    ${MM_SERVER_SOURCE_DIRECTORY}/http_server.hpp
    ${MM_SERVER_SOURCE_DIRECTORY}/http_session.hpp
    ${MM_SERVER_SOURCE_DIRECTORY}/websocket_session.hpp
)

add_executable(mm_server
    ${MM_SERVER_SOURCE_FILES}
    ${MM_SERVER_HEADER_FILES}
)
target_include_directories(mm_server PUBLIC ${MM_INCLUDE_DIRECTORY})
target_link_libraries(mm_server PUBLIC
    Boost::disable_autolinking
    Boost::system
    Boost::thread
    Ghulbus::gbBase
)

#########################################################################################
#### player                                                                          ####
#########################################################################################

set(MM_PLAYER_SOURCE_DIRECTORY ${PROJECT_SOURCE_DIR}/src/media_minion/player)

set(MM_PLAYER_SOURCE_FILES
    ${MM_PLAYER_SOURCE_DIRECTORY}/player.cpp
)

set(MM_PLAYER_HEADER_FILES
)

set(MM_PLAYER_QT_SOURCE_FILES
    ${MM_PLAYER_SOURCE_DIRECTORY}/ui/tray_icon.cpp
)

set(MM_PLAYER_QT_HEADER_FILES
)

set(MM_PLAYER_QT_MOC_HEADER_FILES
    ${MM_PLAYER_SOURCE_DIRECTORY}/ui/tray_icon.hpp
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
target_include_directories(mm_player PUBLIC ${MM_INCLUDE_DIRECTORY})
target_link_libraries(mm_player PUBLIC
    Boost::disable_autolinking
    Boost::system
    Ghulbus::gbBase
    Ghulbus::gbAudio
    Qt5::Core
    Qt5::Widgets
)

#########################################################################################
#### client                                                                          ####
#########################################################################################

set(MM_CLIENT_SOURCE_DIRECTORY ${PROJECT_SOURCE_DIR}/src/media_minion/client)

set(MM_CLIENT_SOURCE_FILES
    ${MM_CLIENT_SOURCE_DIRECTORY}/client.cpp
)

set(MM_CLIENT_HEADER_FILES
)

set(MM_CLIENT_QT_SOURCE_FILES
    ${MM_CLIENT_SOURCE_DIRECTORY}/ui/client_widget.cpp
)

set(MM_CLIENT_QT_HEADER_FILES
)

set(MM_CLIENT_QT_MOC_HEADER_FILES
    ${MM_CLIENT_SOURCE_DIRECTORY}/ui/client_widget.hpp
)
qt5_wrap_cpp(MM_CLIENT_QT_MOC_SOURCE_FILES ${MM_CLIENT_QT_MOC_HEADER_FILES})

source_group(moc FILES
    ${MM_CLIENT_QT_MOC_SOURCE_FILES}
)

add_executable(mm_client #WIN32
    ${MM_CLIENT_SOURCE_FILES}
    ${MM_CLIENT_HEADER_FILES}
    ${MM_CLIENT_QT_SOURCE_FILES}
    ${MM_CLIENT_QT_HEADER_FILES}
    ${MM_CLIENT_QT_MOC_HEADER_FILES}
    ${MM_CLIENT_QT_MOC_SOURCE_FILES}
)
target_include_directories(mm_client PUBLIC ${MM_INCLUDE_DIRECTORY})
target_link_libraries(mm_client PUBLIC
    Boost::disable_autolinking
    Boost::system
    Ghulbus::gbBase
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
