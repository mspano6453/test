cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
endif()

project(TeaTowel VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(TeaTowel
    COMPANY_NAME "Tea Towel Audio"
    BUNDLE_ID "com.teatowelaudio.teatowel"
    PRODUCT_NAME "Tea Towel"
    PLUGIN_MANUFACTURER_CODE Ttwl
    PLUGIN_CODE Tdrm
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS VST3 AU)

file(GLOB TEATOWEL_SAMPLES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Samples/*.flac)
juce_add_binary_data(TeaTowelSamples SOURCES ${TEATOWEL_SAMPLES})
set_target_properties(TeaTowelSamples PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(TeaTowel PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UiKit.cpp)

target_compile_definitions(TeaTowel PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TeaTowel
    PRIVATE
        TeaTowelSamples
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Tape Twin - tape echo + double tracker
juce_add_plugin(TapeTwin
    COMPANY_NAME "Tea Towel Audio"
    BUNDLE_ID "com.teatowelaudio.tapetwin"
    PRODUCT_NAME "Tape Twin"
    PLUGIN_MANUFACTURER_CODE Ttwl
    PLUGIN_CODE Ttwn
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay
    FORMATS VST3 AU)

target_sources(TapeTwin PRIVATE
    TapeTwin/TapeTwinProcessor.cpp
    TapeTwin/TapeTwinEditor.cpp
    Source/UiKit.cpp)

target_compile_definitions(TapeTwin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TapeTwin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline render test: cmake -B build -DTEATOWEL_RENDER_DEMO=ON
option(TEATOWEL_RENDER_DEMO "Build the offline render test tool" OFF)
if(TEATOWEL_RENDER_DEMO)
    juce_add_console_app(RenderDemo PRODUCT_NAME "RenderDemo")
    target_sources(RenderDemo PRIVATE
        Tools/RenderDemo.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/UiKit.cpp
        TapeTwin/TapeTwinProcessor.cpp
        TapeTwin/TapeTwinEditor.cpp)
    target_compile_definitions(RenderDemo PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 TAPETWIN_NO_ENTRY=1)
    target_link_libraries(RenderDemo PRIVATE
        TeaTowelSamples
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
