cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel) for Logic Pro. Must be set before project().
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

# Version can be overridden by CI from the release tag: -DVOXPILOT_VERSION=1.2.3
set(VOXPILOT_VERSION "1.0.0" CACHE STRING "Plug-in version (major.minor.patch)")
project(VoxPilot VERSION ${VOXPILOT_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout with -DJUCE_DIR=/path/to/JUCE, otherwise download it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(VOXPILOT_FORMATS AU VST3 Standalone)
    set(VOXPILOT_COPY TRUE)
else()
    set(VOXPILOT_FORMATS VST3 Standalone)
    set(VOXPILOT_COPY FALSE)
endif()

juce_add_plugin(VoxPilot
    COMPANY_NAME                "SB Audio"
    BUNDLE_ID                   "com.sbaudio.voxpilot"
    PLUGIN_MANUFACTURER_CODE    Sbau
    PLUGIN_CODE                 Vxpl
    FORMATS                     ${VOXPILOT_FORMATS}
    PRODUCT_NAME                "VoxPilot"
    DESCRIPTION                 "Automatic vocal level rider"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_SANDBOX_SAFE             TRUE
    VST3_CATEGORIES             Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD     ${VOXPILOT_COPY})

target_sources(VoxPilot PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(VoxPilot PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(VoxPilot
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
