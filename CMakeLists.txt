cmake_minimum_required(VERSION 3.10)

project(RTMPVideoStreamer)

option(VK_VIDEO_STREAMER "Build as VK Video Streamer" OFF)
option(YOUTUBE_LIVE_STREAMER "Build as YouTube Live Streamer" OFF)
option(ENABLE_BROWSER_UI "Build with browser UI enabled" ON)
option(ENABLE_SSDP "Build with SDSP enabled" ON)

if(VK_VIDEO_STREAMER)
    add_definitions(-DVK_VIDEO_STREAMER=1)
elseif(YOUTUBE_LIVE_STREAMER)
    add_definitions(-DYOUTUBE_LIVE_STREAMER=1)
endif()

if(ENABLE_BROWSER_UI)
    add_definitions(-DENABLE_BROWSER_UI=1)
endif()
if(ENABLE_SSDP)
    add_definitions(-DENABLE_SSDP=1)
endif()


if(DEFINED ENV{SNAPCRAFT_PROJECT_NAME})
    add_definitions(-DSNAPCRAFT_BUILD=1)
    set(SNAPCRAFT_BUILD YES)
endif()

set(ONVIF_SUPPORT OFF CACHE BOOL "")

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(GLIB REQUIRED glib-2.0)
pkg_search_module(SPDLOG REQUIRED spdlog)
pkg_search_module(LIBCONFIG REQUIRED libconfig)
pkg_search_module(GSTREAMER REQUIRED gstreamer-1.0)
if(ENABLE_BROWSER_UI)
    pkg_search_module(JANSSON REQUIRED jansson)
endif()
if(ENABLE_SSDP)
    pkg_search_module(GSSDP REQUIRED gssdp-1.6)
endif()

if(ENABLE_BROWSER_UI)
    add_subdirectory(WebRTSP)
    add_subdirectory(www)
else()
    add_subdirectory(WebRTSP/RtStreaming/deps/CxxPtr)
endif()

set(SOURCES
    Defines.h
    Log.h
    Log.cpp
    Config.h
    Config.cpp
    ConfigHelpers.h
    ConfigHelpers.cpp
    ReStreamer.h
    ReStreamer.cpp
    main.cpp
    StreamerMain.h
    StreamerMain.cpp
    rtmp-streamer.conf.sample
    live-streamer.conf.sample
    vk-streamer.conf.sample
    README.md
)
if(ENABLE_BROWSER_UI)
    set(BROWSER_UI_SRC
        RestApi.h
        RestApi.cpp
    )
endif()
if(ENABLE_SSDP)
    set(SSDP_SRC
        SSDP.h
        SSDP.cpp
    )
endif()
file(GLOB_RECURSE SNAP_SRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    snap/*)

add_executable(${PROJECT_NAME} ${SOURCES} ${BROWSER_UI_SRC} ${SSDP_SRC} ${SNAP_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${GLIB_INCLUDE_DIRS}
    ${SPDLOG_INCLUDE_DIRS}
    ${LIBCONFIG_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${GLIB_LDFLAGS}
    ${SPDLOG_LDFLAGS}
    ${LIBCONFIG_LDFLAGS}
    ${GSTREAMER_LDFLAGS}
    CxxPtr
    Threads::Threads
)
if(ENABLE_BROWSER_UI)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${JANSSON_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${JANSSON_LDFLAGS}
        Http
        Signalling
        RtStreaming
    )
endif()
if(ENABLE_SSDP)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${GSSDP_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${GSSDP_LDFLAGS}
    )
endif()

if(VK_VIDEO_STREAMER)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "VKVideoStreamer")
elseif(YOUTUBE_LIVE_STREAMER)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "YouTubeLiveStreamer")
endif()

if(SNAPCRAFT_BUILD)
    install(TARGETS ${PROJECT_NAME} DESTINATION bin)
    if(VK_VIDEO_STREAMER)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/vk-streamer.conf.sample DESTINATION etc)
    elseif(YOUTUBE_LIVE_STREAMER)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/live-streamer.conf.sample DESTINATION etc)
    else()
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/rtmp-streamer.conf.sample DESTINATION etc)
    endif()
endif()
