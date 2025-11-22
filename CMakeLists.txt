cmake_minimum_required(VERSION 3.28)

project(RTMPVideoStreamer VERSION 0.1.8)

option(VK_VIDEO_STREAMER "Build as VK Video Streamer" OFF)
option(YOUTUBE_LIVE_STREAMER "Build as YouTube Live Streamer" OFF)
option(ENABLE_GUI "Build with Qt based UI enabled" OFF)
option(ENABLE_BROWSER_UI "Build with browser UI enabled" ON)
option(ENABLE_SSDP "Build with SDSP enabled" ON)

if(WIN32)
    option(MICROSOFT_STORE_BUILD "Build to publish to Microsoft Store" OFF)
    set(ENABLE_GUI ON CACHE BOOL "" FORCE)
endif()

if(ENABLE_GUI)
    set(ENABLE_BROWSER_UI OFF CACHE BOOL "" FORCE)
    set(ENABLE_SSDP OFF CACHE BOOL "" FORCE)

    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(VK_VIDEO_STREAMER)
    add_definitions(-DVK_VIDEO_STREAMER=1)
elseif(YOUTUBE_LIVE_STREAMER)
    add_definitions(-DYOUTUBE_LIVE_STREAMER=1)
endif()

if(ENABLE_GUI)
    add_definitions(-DENABLE_GUI=1)
else()
    if(ENABLE_BROWSER_UI)
        add_definitions(-DENABLE_BROWSER_UI=1)
    endif()
    if(ENABLE_SSDP)
        add_definitions(-DENABLE_SSDP=1)
    endif()
endif()

if(DEFINED ENV{SNAPCRAFT_PROJECT_NAME})
    add_definitions(-DSNAPCRAFT_BUILD=1)
    set(SNAPCRAFT_BUILD YES)
endif()

set(ONVIF_SUPPORT OFF CACHE BOOL "")

if(ENABLE_GUI)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

    qt_standard_project_setup()
endif()

if(WIN32)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
        cmake_policy(SET CMP0135 NEW)
    endif()

    include(FetchContent)

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.3
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(spdlog)

    FetchContent_Declare(
        libconfig
        GIT_REPOSITORY https://github.com/hyperrealm/libconfig.git
        GIT_TAG v1.8.1
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(libconfig)

    set(GSTREAMER_ROOT_DIR "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}")
    set(GSTREAMER_INCLUDE_DIRS
        "${GSTREAMER_ROOT_DIR}/include/glib-2.0"
        "${GSTREAMER_ROOT_DIR}/lib/glib-2.0/include"
        "${GSTREAMER_ROOT_DIR}/include/gstreamer-1.0"
    )
    set(GSTREAMER_LIBRARIES
        "${GSTREAMER_ROOT_DIR}/lib/glib-2.0.lib"
        "${GSTREAMER_ROOT_DIR}/lib/gobject-2.0.lib"
        "${GSTREAMER_ROOT_DIR}/lib/gstreamer-1.0.lib"
    )

    add_subdirectory(WebRTSP/RtStreaming/deps/CxxPtr)
else()
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
endif()

set(SOURCES
    Defines.h
    Log.h
    Log.cpp
    Types.h
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
if(NOT WIN32)
    file(GLOB_RECURSE SNAP_SRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        snap/*)
endif()

if(ENABLE_GUI)
    set(GUI_SOURCES
        gui/GuiMain.h
        gui/GuiMain.cpp
        gui/NotificationProxy.h
        gui/NotificationProxy.cpp
        gui/Theme.h
        gui/Theme.cpp
        gui/StreamerEditDialog.h
        gui/StreamerEditDialog.cpp
        gui/StreamerEditDialog.ui
        gui/main.qrc
    )
    if(YOUTUBE_LIVE_STREAMER)
        set(GUI_RESOURCES
            gui/YouTubeLiveStreamer.rc
        )
    else()
        set(GUI_RESOURCES
            gui/app.rc
        )
    endif()

    file(GLOB_RECURSE ICONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        icons/*)

    if(WIN32)
        file(GLOB_RECURSE INSTALLER_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
            installer/*)
    endif()

    qt_add_executable(${PROJECT_NAME}
        WIN32 MACOSX_BUNDLE
        ${GUI_SOURCES}
        ${GUI_RESOURCES}
        ${SOURCES}
        ${ICONS}
        ${INSTALLER_FILES}
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt::Core
        Qt::Widgets
    )
else()
    add_executable(${PROJECT_NAME}
        ${SOURCES}
        ${BROWSER_UI_SRC}
        ${SSDP_SRC}
        ${SNAP_SRC})
endif()

if(WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${GSTREAMER_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${GSTREAMER_LIBRARIES}
        spdlog
        libconfig
    )
else()
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
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE
    CxxPtr
)

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
elseif(WIN32 AND ENABLE_GUI)
    file(TO_CMAKE_PATH "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/bin" GSTREAMER_BIN_DIR)
    file(TO_CMAKE_PATH "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib/gstreamer-1.0" GSTREAMER_LIB_DIR)

    set(GSTREAMER_BIN_FILES
        ffi-7.dll
        gio-2.0-0.dll
        glib-2.0-0.dll
        gmodule-2.0-0.dll
        gobject-2.0-0.dll
        gstaudio-1.0-0.dll
        gstbase-1.0-0.dll
        gstnet-1.0-0.dll
        gstpbutils-1.0-0.dll
        gstreamer-1.0-0.dll
        gstriff-1.0-0.dll
        gstrtp-1.0-0.dll
        gstrtsp-1.0-0.dll
        gstsdp-1.0-0.dll
        gsttag-1.0-0.dll
        gstvideo-1.0-0.dll
        intl-8.dll
        libcrypto-3-x64.dll
        libssl-3-x64.dll
        orc-0.4-0.dll
        pcre2-8-0.dll
        rtmp-1.dll
        z-1.dll
    )
    list(TRANSFORM GSTREAMER_BIN_FILES PREPEND "${GSTREAMER_BIN_DIR}/")

    set(GSTREAMER_LIB_FILES
        gstasf.dll
        gstaudiotestsrc.dll
        gstcoreelements.dll
        gstflv.dll
        gstplayback.dll
        gstrtmp.dll
        gstrtp.dll
        gstrtpmanager.dll
        gstrtsp.dll
        gstudp.dll
    )
    list(TRANSFORM GSTREAMER_LIB_FILES PREPEND "${GSTREAMER_LIB_DIR}/")

    if(MICROSOFT_STORE_BUILD)
        if(YOUTUBE_LIVE_STREAMER)
            set(INSTALLER_FILES
                youtube-44.png
                youtube-50.png
                youtube-150.png
            )
            configure_file(
                ${CMAKE_CURRENT_SOURCE_DIR}/installer/YouTube-Live-Streamer-AppxManifest.xml.template
                ${CMAKE_CURRENT_BINARY_DIR}/installer/YouTube-Live-Streamer-AppxManifest.xml
            )
            set(INSTALLER_MANIFEST_FILE "${CMAKE_CURRENT_BINARY_DIR}/installer/YouTube-Live-Streamer-AppxManifest.xml")
        else()
            set(INSTALLER_FILES
                video-44.png
                video-50.png
                video-150.png
            )
            configure_file(
                ${CMAKE_CURRENT_SOURCE_DIR}/installer/AppxManifest.xml.template
                ${CMAKE_CURRENT_BINARY_DIR}/installer/AppxManifest.xml
            )
            set(INSTALLER_MANIFEST_FILE "${CMAKE_CURRENT_BINARY_DIR}/installer/AppxManifest.xml")
        endif()
        list(TRANSFORM INSTALLER_FILES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/installer/")
    endif()

    #include(InstallRequiredSystemLibraries)
    qt_generate_deploy_app_script(
        TARGET ${PROJECT_NAME}
        OUTPUT_SCRIPT DEPLOY_SCRIPT
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${DEPLOY_SCRIPT})
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
    )
    install(FILES
        ${GSTREAMER_BIN_FILES}
        DESTINATION bin)
    install(FILES
        ${GSTREAMER_LIB_FILES}
        DESTINATION lib/gstreamer-1.0)

    if(MICROSOFT_STORE_BUILD)
        install(FILES
            ${INSTALLER_FILES}
            DESTINATION .)
        install(FILES
            ${INSTALLER_MANIFEST_FILE}
            DESTINATION .
            RENAME AppxManifest.xml)

        if(YOUTUBE_LIVE_STREAMER)
            set(CPACK_PACKAGE_NAME "YouTubeLiveStreamer")
        endif()
        set(CPACK_GENERATOR "External")
        set(CPACK_EXTERNAL_ENABLE_STAGING TRUE)
        set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/installer/MakeAppX.cmake")
        include(CPack)
    endif()
endif()
