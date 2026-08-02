cmake_minimum_required(VERSION 3.22)
project(TouchMe3 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
message(STATUS "Fetching JUCE...")
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
)
FetchContent_MakeAvailable(juce)

juce_add_gui_app(TouchMe3
    PRODUCT_NAME "TouchMe3"
    VERSION "1.0.0"
    BUNDLE_ID "com.touchme3.vj"
)

juce_generate_juce_header(TouchMe3)

# --- Fetch foleys_video_engine ---
FetchContent_Declare(
    foleys_video_engine
    GIT_REPOSITORY https://github.com/ffAudio/foleys_video_engine.git
    GIT_TAG master
    SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/foleys_video_engine
)
FetchContent_MakeAvailable(foleys_video_engine)
juce_add_module(${foleys_video_engine_SOURCE_DIR})

# --- Fetch FFmpeg prebuilt shared binaries on Windows ---
if(WIN32)
    FetchContent_Declare(
        ffmpeg_prebuilt
        URL "https://github.com/GyanD/codexffmpeg/releases/download/5.1.2/ffmpeg-5.1.2-full_build-shared.zip"
    )
    FetchContent_MakeAvailable(ffmpeg_prebuilt)

    # BtbN zip extracts to a subdirectory inside the source folder. Locate it.
    file(GLOB FFMPEG_SUBDIR LIST_DIRECTORIES true "${ffmpeg_prebuilt_SOURCE_DIR}/ffmpeg-*")
    if (FFMPEG_SUBDIR)
        set(FFMPEG_DIR ${FFMPEG_SUBDIR})
    else()
        set(FFMPEG_DIR ${ffmpeg_prebuilt_SOURCE_DIR})
    endif()

    set(FFMPEG_INCLUDE_DIR ${FFMPEG_DIR}/include)
    set(FFMPEG_LIB_DIR ${FFMPEG_DIR}/lib)
    set(FFMPEG_BIN_DIR ${FFMPEG_DIR}/bin)
endif()

target_sources(TouchMe3 PRIVATE
    Source/Main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp
    Source/ClipGridComponent.h
    Source/ClipGridComponent.cpp
    Source/PropertiesComponent.h
    Source/PropertiesComponent.cpp
    Source/MonitorComponent.h
    Source/MonitorComponent.cpp
    Source/MediaEngine.h
    Source/MediaEngine.cpp
    Source/AdvancedOutputWindow.cpp
    Source/NodeGraph.h
    Source/NodeGraph.cpp
    Source/NodeEditorComponent.h
    Source/NodeEditorComponent.cpp
    Source/NodePropertiesComponent.h
    Source/NodePropertiesComponent.cpp
    Source/MappingManager.h
    Source/MappingManager.cpp
    Source/SettingsManager.h
    Source/SettingsManager.cpp
    Source/SettingsWindow.h
    Source/SettingsWindow.cpp
)

target_compile_definitions(TouchMe3 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CAMERA=0
    JUCE_DIRECT2D=1
    FOLEYS_USE_FFMPEG=1
)

if (APPLE)
    target_include_directories(TouchMe3 PRIVATE /opt/homebrew/opt/ffmpeg@4/include)
    target_link_directories(TouchMe3 PRIVATE /opt/homebrew/opt/ffmpeg@4/lib)
    target_link_libraries(TouchMe3 PRIVATE
        avcodec
        avformat
        avutil
        swscale
        swresample
    )
endif()

target_link_libraries(TouchMe3 PRIVATE
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_events
    juce::juce_core
    juce::juce_video
    juce::juce_opengl
    juce::juce_audio_devices
    juce::juce_osc
    foleys_video_engine
)

if(MSVC)
    target_link_options(TouchMe3 PRIVATE "/MAP")
endif()

if(WIN32)
    target_include_directories(TouchMe3 PRIVATE ${FFMPEG_INCLUDE_DIR})
    target_link_directories(TouchMe3 PRIVATE ${FFMPEG_LIB_DIR})
    target_link_libraries(TouchMe3 PRIVATE
        avcodec
        avformat
        avutil
        swscale
        swresample
    )

    # Post-build copy step to copy FFmpeg DLLs next to executable
    file(GLOB FFMPEG_DLLS "${FFMPEG_BIN_DIR}/*.dll")
    add_custom_command(TARGET TouchMe3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FFMPEG_DLLS} $<TARGET_FILE_DIR:TouchMe3>
    )
endif()

if(MSVC)
    target_link_options(TouchMe3 PRIVATE "/SUBSYSTEM:WINDOWS")
endif()
