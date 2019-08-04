cmake_minimum_required(VERSION 3.14)

project(media-minion)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/W4>)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/permissive->)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/std:c++latest>)
add_compile_definitions($<$<CXX_COMPILER_ID:MSVC>:_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING>)
add_compile_options($<$<CXX_COMPILER_ID:GNU,AppleClang,Clang>:-Wall>)
add_compile_options($<$<CXX_COMPILER_ID:GNU,AppleClang,Clang>:-pedantic>)
add_compile_options($<$<CXX_COMPILER_ID:Clang>:-stdlib=libc++>)
add_link_options($<$<CXX_COMPILER_ID:Clang>:-stdlib=libc++>)

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
find_package(Boost 1.66 REQUIRED COMPONENTS system thread)
add_compile_definitions(BOOST_ALL_NO_LIB)
if(Boost_VERSION GREATER_EQUAL 107000)
    add_compile_definitions(BOOST_BEAST_SEPARATE_COMPILATION)
endif()

find_package(Qt5 REQUIRED COMPONENTS Widgets
    HINTS $ENV{QT5_ROOT}/5.12.2/msvc2017_64 $ENV{QT5_ROOT}/5.12.0/msvc2017_64
)

find_package(RapidJSON REQUIRED)
add_library(RapidJSON INTERFACE)
target_include_directories(RapidJSON INTERFACE ${RAPIDJSON_INCLUDE_DIRS})
target_compile_definitions(RapidJSON INTERFACE RAPIDJSON_HAS_STDSTRING=1)

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
    ${MM_COMMON_SOURCE_DIRECTORY}/beast_compile.cpp
    ${MM_COMMON_SOURCE_DIRECTORY}/logging.cpp
)

set(MM_COMMON_HEADER_FILES
    ${MM_COMMON_SOURCE_DIRECTORY}/common.hpp
    ${MM_COMMON_SOURCE_DIRECTORY}/coroutine_support/awaitables.hpp
    ${MM_COMMON_SOURCE_DIRECTORY}/logging.hpp
)

add_library(mm_common STATIC
    ${MM_COMMON_SOURCE_FILES}
    ${MM_COMMON_HEADER_FILES}
)
target_include_directories(mm_common PUBLIC ${MM_INCLUDE_DIRECTORY})
target_link_libraries(mm_common PUBLIC
    Boost::disable_autolinking
    Boost::headers
    Boost::system
    Ghulbus::gbBase
    RapidJSON
)
target_compile_options(mm_common PUBLIC $<$<CXX_COMPILER_ID:Clang>:-fcoroutines-ts>)
target_compile_options(mm_common PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/await>)

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
    Boost::thread
    mm_common
)

file(COPY ${PROJECT_SOURCE_DIR}/config/server_config.json DESTINATION ${PROJECT_BINARY_DIR})

#########################################################################################
#### player                                                                          ####
#########################################################################################

set(MM_PLAYER_SOURCE_DIRECTORY ${PROJECT_SOURCE_DIR}/src/media_minion/player)

set(MM_PLAYER_SOURCE_FILES
    ${MM_PLAYER_SOURCE_DIRECTORY}/player.cpp
    ${MM_PLAYER_SOURCE_DIRECTORY}/audio_player.cpp
    ${MM_PLAYER_SOURCE_DIRECTORY}/configuration.cpp
    ${MM_PLAYER_SOURCE_DIRECTORY}/wav_stream.cpp
)

set(MM_PLAYER_HEADER_FILES
    ${MM_PLAYER_SOURCE_DIRECTORY}/audio_player.hpp
    ${MM_PLAYER_SOURCE_DIRECTORY}/configuration.hpp
    ${MM_PLAYER_SOURCE_DIRECTORY}/wav_stream.hpp
)

set(MM_PLAYER_QT_SOURCE_FILES
    ${MM_PLAYER_SOURCE_DIRECTORY}/ui/player_application.cpp
    ${MM_PLAYER_SOURCE_DIRECTORY}/ui/tray_icon.cpp
)

set(MM_PLAYER_QT_HEADER_FILES
)

set(MM_PLAYER_QT_MOC_HEADER_FILES
    ${MM_PLAYER_SOURCE_DIRECTORY}/ui/player_application.hpp
    ${MM_PLAYER_SOURCE_DIRECTORY}/ui/tray_icon.hpp
)
qt5_wrap_cpp(MM_PLAYER_QT_MOC_SOURCE_FILES ${MM_PLAYER_QT_MOC_HEADER_FILES})

source_group(moc FILES
    ${MM_PLAYER_QT_MOC_SOURCE_FILES}
)

qt5_add_resources(MM_PLAYER_QT_RESOURCE_FILES ${PROJECT_SOURCE_DIR}/resources/mm_player.qrc)
source_group(resources FILES
    ${MM_PLAYER_QT_RESOURCE_FILES}
)


add_executable(mm_player WIN32
    ${MM_PLAYER_SOURCE_FILES}
    ${MM_PLAYER_HEADER_FILES}
    ${MM_PLAYER_QT_SOURCE_FILES}
    ${MM_PLAYER_QT_HEADER_FILES}
    ${MM_PLAYER_QT_MOC_HEADER_FILES}
    ${MM_PLAYER_QT_MOC_SOURCE_FILES}
    ${MM_PLAYER_QT_RESOURCE_FILES}
)
target_include_directories(mm_player PUBLIC ${MM_INCLUDE_DIRECTORY})
target_link_libraries(mm_player PUBLIC
    Ghulbus::gbAudio
    Qt5::Core
    Qt5::Widgets
    mm_common
)

file(COPY ${PROJECT_SOURCE_DIR}/config/player_config.json DESTINATION ${PROJECT_BINARY_DIR})

#########################################################################################
#### client                                                                          ####
#########################################################################################

set(MM_CLIENT_SOURCE_DIRECTORY ${PROJECT_SOURCE_DIR}/src/media_minion/client)

set(MM_CLIENT_SOURCE_FILES
    ${MM_CLIENT_SOURCE_DIRECTORY}/client.cpp
    ${MM_CLIENT_SOURCE_DIRECTORY}/websocket.cpp
)

set(MM_CLIENT_HEADER_FILES
    ${MM_CLIENT_SOURCE_DIRECTORY}/websocket.hpp
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
    Qt5::Core
    Qt5::Widgets
    mm_common
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
