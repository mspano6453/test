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
    COMPANY_NAME "DEMOCRAZY"
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
    Source/UiKit.cpp Source/BandBus.cpp)

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
    COMPANY_NAME "DEMOCRAZY"
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
    Source/UiKit.cpp Source/BandBus.cpp)

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

# ---------------------------------------------------------------------------
# Lathe - mastering suite
juce_add_plugin(Lathe
    COMPANY_NAME "DEMOCRAZY"
    BUNDLE_ID "com.teatowelaudio.lathe"
    PRODUCT_NAME "Lathe"
    PLUGIN_MANUFACTURER_CODE Ttwl
    PLUGIN_CODE Lthe
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Mastering
    FORMATS VST3 AU)

target_sources(Lathe PRIVATE
    Lathe/LatheProcessor.cpp
    Lathe/LatheEditor.cpp
    Source/UiKit.cpp Source/BandBus.cpp)

target_compile_definitions(Lathe PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Lathe
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Schaltwerk - electronic rhythm box, robot synth/vocoder, Strumchord
juce_add_plugin(Schaltwerk
    COMPANY_NAME "DEMOCRAZY"
    BUNDLE_ID "com.teatowelaudio.schaltwerk"
    PRODUCT_NAME "Schaltwerk"
    PLUGIN_MANUFACTURER_CODE Ttwl
    PLUGIN_CODE Schw
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth Drum
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS VST3 AU)

target_sources(Schaltwerk PRIVATE
    Schaltwerk/SchaltwerkProcessor.cpp
    Schaltwerk/SchaltwerkEditor.cpp
    Source/UiKit.cpp Source/BandBus.cpp)

target_compile_definitions(Schaltwerk PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Schaltwerk
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Loon - ambient generator that listens to your guitar
juce_add_plugin(Loon
    COMPANY_NAME "DEMOCRAZY"
    BUNDLE_ID "com.teatowelaudio.loon"
    PRODUCT_NAME "Loon"
    PLUGIN_MANUFACTURER_CODE Ttwl
    PLUGIN_CODE Loon
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Generator Reverb
    FORMATS VST3 AU)

target_sources(Loon PRIVATE
    Loon/LoonProcessor.cpp
    Loon/LoonEditor.cpp
    Source/UiKit.cpp Source/BandBus.cpp)

target_compile_definitions(Loon PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Loon
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Carousel - stereo looper
juce_add_plugin(Carousel
    COMPANY_NAME "DEMOCRAZY"
    BUNDLE_ID "com.teatowelaudio.carousel"
    PRODUCT_NAME "Carousel"
    PLUGIN_MANUFACTURER_CODE Ttwl
    PLUGIN_CODE Crsl
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Tools
    FORMATS VST3 AU)

target_sources(Carousel PRIVATE
    Carousel/CarouselProcessor.cpp
    Carousel/CarouselEditor.cpp
    Source/UiKit.cpp Source/BandBus.cpp)

target_compile_definitions(Carousel PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Carousel
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Undertow - post-punk bass player (real picked-bass samples, follows your guitar)
file(GLOB UNDERTOW_SAMPLES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Undertow/Samples/*.flac")
juce_add_binary_data(UndertowSamples NAMESPACE UndertowData HEADER_NAME UndertowData.h SOURCES ${UNDERTOW_SAMPLES})
set_target_properties(UndertowSamples PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(Undertow
    COMPANY_NAME "DEMOCRAZY"
    BUNDLE_ID "com.teatowelaudio.undertow"
    PRODUCT_NAME "Undertow"
    PLUGIN_MANUFACTURER_CODE Ttwl
    PLUGIN_CODE Undr
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS VST3 AU)

target_sources(Undertow PRIVATE
    Undertow/UndertowProcessor.cpp
    Undertow/UndertowEditor.cpp
    Source/UiKit.cpp Source/BandBus.cpp)

target_compile_definitions(Undertow PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Undertow
    PRIVATE
        UndertowSamples
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Parlour - keys player (real piano / vibes / glockenspiel samples + modelled electric keys)
file(GLOB PARLOUR_SAMPLES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Parlour/Samples/*.flac")
juce_add_binary_data(ParlourSamples NAMESPACE ParlourData HEADER_NAME ParlourData.h SOURCES ${PARLOUR_SAMPLES})
set_target_properties(ParlourSamples PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(Parlour
    COMPANY_NAME "DEMOCRAZY"
    BUNDLE_ID "com.teatowelaudio.parlour"
    PRODUCT_NAME "Parlour"
    PLUGIN_MANUFACTURER_CODE Ttwl
    PLUGIN_CODE Prlr
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS VST3 AU)

target_sources(Parlour PRIVATE
    Parlour/ParlourProcessor.cpp
    Parlour/ParlourEditor.cpp
    Source/UiKit.cpp Source/BandBus.cpp)

target_compile_definitions(Parlour PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Parlour
    PRIVATE
        ParlourSamples
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Ether - early electronic instruments (all modelled) that play melodies over the band
juce_add_plugin(Ether
    COMPANY_NAME "DEMOCRAZY"
    BUNDLE_ID "com.teatowelaudio.ether"
    PRODUCT_NAME "Ether"
    PLUGIN_MANUFACTURER_CODE Ttwl
    PLUGIN_CODE Ethr
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS VST3 AU)

target_sources(Ether PRIVATE
    Ether/EtherProcessor.cpp
    Ether/EtherEditor.cpp
    Source/UiKit.cpp Source/BandBus.cpp)

target_compile_definitions(Ether PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Ether
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Chamber - 60s orchestral arranger (real strings / brass / woodwinds, VSCO 2 CE, CC0)
file(GLOB CHAMBER_SAMPLES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Chamber/Samples/*.flac")
juce_add_binary_data(ChamberSamples NAMESPACE ChamberData HEADER_NAME ChamberData.h SOURCES ${CHAMBER_SAMPLES})
set_target_properties(ChamberSamples PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(Chamber
    COMPANY_NAME "DEMOCRAZY"
    BUNDLE_ID "com.teatowelaudio.chamber"
    PRODUCT_NAME "Chamber"
    PLUGIN_MANUFACTURER_CODE Ttwl
    PLUGIN_CODE Chmb
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS VST3 AU)

target_sources(Chamber PRIVATE
    Chamber/ChamberProcessor.cpp
    Chamber/ChamberEditor.cpp
    Source/UiKit.cpp Source/BandBus.cpp)

target_compile_definitions(Chamber PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Chamber
    PRIVATE
        ChamberSamples
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Bandleader - runs the song for the whole band (no sound of its own)
juce_add_plugin(Bandleader
    COMPANY_NAME "DEMOCRAZY"
    BUNDLE_ID "com.teatowelaudio.bandleader"
    PRODUCT_NAME "Bandleader"
    PLUGIN_MANUFACTURER_CODE Ttwl
    PLUGIN_CODE Bndl
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS VST3 AU)

target_sources(Bandleader PRIVATE
    Bandleader/BandleaderProcessor.cpp
    Bandleader/BandleaderEditor.cpp
    Source/UiKit.cpp Source/BandBus.cpp)

target_compile_definitions(Bandleader PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Bandleader
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Jangle - a rhythm guitar player (real steel acoustic, clean and hollow-body electric samples, all CC0)
file(GLOB JANGLE_SAMPLES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Jangle/Samples/*.flac")
juce_add_binary_data(JangleSamples NAMESPACE JangleData HEADER_NAME JangleData.h SOURCES ${JANGLE_SAMPLES})
set_target_properties(JangleSamples PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(Jangle
    COMPANY_NAME "DEMOCRAZY"
    BUNDLE_ID "com.teatowelaudio.jangle"
    PRODUCT_NAME "Jangle"
    PLUGIN_MANUFACTURER_CODE Ttwl
    PLUGIN_CODE Jngl
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS VST3 AU)

target_sources(Jangle PRIVATE
    Jangle/JangleProcessor.cpp
    Jangle/JangleEditor.cpp
    Source/UiKit.cpp Source/BandBus.cpp)

target_compile_definitions(Jangle PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Jangle
    PRIVATE
        JangleSamples
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Harmonia - backing vocals (modelled voices: no samples)
juce_add_plugin(Harmonia
    COMPANY_NAME "DEMOCRAZY"
    BUNDLE_ID "com.teatowelaudio.harmonia"
    PRODUCT_NAME "Harmonia"
    PLUGIN_MANUFACTURER_CODE Ttwl
    PLUGIN_CODE Hrmn
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS VST3 AU)

target_sources(Harmonia PRIVATE
    Harmonia/HarmoniaProcessor.cpp
    Harmonia/HarmoniaEditor.cpp
    Source/UiKit.cpp Source/BandBus.cpp)

target_compile_definitions(Harmonia PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Harmonia
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Swirl - psychedelic studio tricks: backwards, phaser, rotary speaker, tape flange
juce_add_plugin(Swirl
    COMPANY_NAME "DEMOCRAZY"
    BUNDLE_ID "com.teatowelaudio.swirl"
    PRODUCT_NAME "Swirl"
    PLUGIN_MANUFACTURER_CODE Ttwl
    PLUGIN_CODE Swrl
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Modulation
    AU_MAIN_TYPE kAudioUnitType_Effect
    FORMATS VST3 AU)
target_sources(Swirl PRIVATE Swirl/SwirlProcessor.cpp Swirl/SwirlEditor.cpp Source/UiKit.cpp Source/BandBus.cpp)
target_compile_definitions(Swirl PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Swirl PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Offline render test: cmake -B build -DTEATOWEL_RENDER_DEMO=ON
option(TEATOWEL_RENDER_DEMO "Build the offline render test tool" OFF)
if(TEATOWEL_RENDER_DEMO)
    juce_add_console_app(RenderDemo PRODUCT_NAME "RenderDemo")
    target_sources(RenderDemo PRIVATE
        Tools/RenderDemo.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/UiKit.cpp Source/BandBus.cpp
        TapeTwin/TapeTwinProcessor.cpp
        TapeTwin/TapeTwinEditor.cpp)
    target_compile_definitions(RenderDemo PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 TAPETWIN_NO_ENTRY=1)
    target_link_libraries(RenderDemo PRIVATE
        TeaTowelSamples
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_recommended_config_flags)

    juce_add_console_app(LatheTest PRODUCT_NAME "LatheTest")
    target_sources(LatheTest PRIVATE Tools/LatheTest.cpp Lathe/LatheProcessor.cpp Lathe/LatheEditor.cpp Source/UiKit.cpp Source/BandBus.cpp)
    target_compile_definitions(LatheTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 LATHE_NO_ENTRY=1)
    target_link_libraries(LatheTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

    juce_add_console_app(SchaltwerkTest PRODUCT_NAME "SchaltwerkTest")
    target_sources(SchaltwerkTest PRIVATE Tools/SchaltwerkTest.cpp Schaltwerk/SchaltwerkProcessor.cpp Schaltwerk/SchaltwerkEditor.cpp Source/UiKit.cpp Source/BandBus.cpp)
    target_compile_definitions(SchaltwerkTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 SCHALTWERK_NO_ENTRY=1)
    target_link_libraries(SchaltwerkTest PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)

    juce_add_console_app(LoonTest PRODUCT_NAME "LoonTest")
    target_sources(LoonTest PRIVATE Tools/LoonTest.cpp Loon/LoonProcessor.cpp Loon/LoonEditor.cpp Source/UiKit.cpp Source/BandBus.cpp)
    target_compile_definitions(LoonTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 LOON_NO_ENTRY=1)
    target_link_libraries(LoonTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

    juce_add_console_app(CarouselTest PRODUCT_NAME "CarouselTest")
    target_sources(CarouselTest PRIVATE Tools/CarouselTest.cpp Carousel/CarouselProcessor.cpp Carousel/CarouselEditor.cpp Source/UiKit.cpp Source/BandBus.cpp)
    target_compile_definitions(CarouselTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 CAROUSEL_NO_ENTRY=1)
    target_link_libraries(CarouselTest PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)

    juce_add_console_app(KitTest PRODUCT_NAME "KitTest")
    target_sources(KitTest PRIVATE Tools/KitTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/UiKit.cpp Source/BandBus.cpp)
    target_compile_definitions(KitTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 TEATOWEL_NO_ENTRY=1)
    target_link_libraries(KitTest PRIVATE TeaTowelSamples juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

    juce_add_console_app(UndertowTest PRODUCT_NAME "UndertowTest")
    target_sources(UndertowTest PRIVATE Tools/UndertowTest.cpp Undertow/UndertowProcessor.cpp Undertow/UndertowEditor.cpp Source/UiKit.cpp Source/BandBus.cpp)
    target_compile_definitions(UndertowTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 UNDERTOW_NO_ENTRY=1)
    target_link_libraries(UndertowTest PRIVATE UndertowSamples juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

    juce_add_console_app(ParlourTest PRODUCT_NAME "ParlourTest")
    target_sources(ParlourTest PRIVATE Tools/ParlourTest.cpp Parlour/ParlourProcessor.cpp Parlour/ParlourEditor.cpp Source/UiKit.cpp Source/BandBus.cpp)
    target_compile_definitions(ParlourTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 PARLOUR_NO_ENTRY=1)
    target_link_libraries(ParlourTest PRIVATE ParlourSamples juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

    juce_add_console_app(EtherTest PRODUCT_NAME "EtherTest")
    target_sources(EtherTest PRIVATE Tools/EtherTest.cpp Ether/EtherProcessor.cpp Ether/EtherEditor.cpp Source/UiKit.cpp Source/BandBus.cpp)
    target_compile_definitions(EtherTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 ETHER_NO_ENTRY=1)
    target_link_libraries(EtherTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

    juce_add_console_app(ChamberTest PRODUCT_NAME "ChamberTest")
    target_sources(ChamberTest PRIVATE Tools/ChamberTest.cpp Chamber/ChamberProcessor.cpp Chamber/ChamberEditor.cpp Source/UiKit.cpp Source/BandBus.cpp)
    target_compile_definitions(ChamberTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 CHAMBER_NO_ENTRY=1)
    target_link_libraries(ChamberTest PRIVATE ChamberSamples juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

    juce_add_console_app(BandTest PRODUCT_NAME "BandTest")
    target_sources(BandTest PRIVATE Tools/BandTest.cpp
        Bandleader/BandleaderProcessor.cpp Bandleader/BandleaderEditor.cpp
        Undertow/UndertowProcessor.cpp Undertow/UndertowEditor.cpp
        Source/PluginProcessor.cpp Source/PluginEditor.cpp
        Source/UiKit.cpp Source/BandBus.cpp)
    target_compile_definitions(BandTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 UNDERTOW_NO_ENTRY=1 BANDLEADER_NO_ENTRY=1 TEATOWEL_NO_ENTRY=1)
    target_link_libraries(BandTest PRIVATE TeaTowelSamples UndertowSamples juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

    juce_add_console_app(SuiteTest PRODUCT_NAME "SuiteTest")
    target_sources(SuiteTest PRIVATE Tools/SuiteTest.cpp
        Bandleader/BandleaderProcessor.cpp Bandleader/BandleaderEditor.cpp
        Source/PluginProcessor.cpp Source/PluginEditor.cpp
        Loon/LoonProcessor.cpp Loon/LoonEditor.cpp
        Schaltwerk/SchaltwerkProcessor.cpp Schaltwerk/SchaltwerkEditor.cpp
        Carousel/CarouselProcessor.cpp Carousel/CarouselEditor.cpp
        TapeTwin/TapeTwinProcessor.cpp TapeTwin/TapeTwinEditor.cpp
        Lathe/LatheProcessor.cpp Lathe/LatheEditor.cpp
        Source/UiKit.cpp Source/BandBus.cpp)
    target_compile_definitions(SuiteTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 BANDLEADER_NO_ENTRY=1 TEATOWEL_NO_ENTRY=1
        LOON_NO_ENTRY=1 SCHALTWERK_NO_ENTRY=1 CAROUSEL_NO_ENTRY=1 TAPETWIN_NO_ENTRY=1 LATHE_NO_ENTRY=1)
    target_link_libraries(SuiteTest PRIVATE TeaTowelSamples juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

    juce_add_console_app(LiveSim PRODUCT_NAME "LiveSim")
    target_sources(LiveSim PRIVATE Tools/LiveSim.cpp
        Bandleader/BandleaderProcessor.cpp Bandleader/BandleaderEditor.cpp
        Source/PluginProcessor.cpp Source/PluginEditor.cpp
        Undertow/UndertowProcessor.cpp Undertow/UndertowEditor.cpp
        Loon/LoonProcessor.cpp Loon/LoonEditor.cpp
        Schaltwerk/SchaltwerkProcessor.cpp Schaltwerk/SchaltwerkEditor.cpp
        Carousel/CarouselProcessor.cpp Carousel/CarouselEditor.cpp
        TapeTwin/TapeTwinProcessor.cpp TapeTwin/TapeTwinEditor.cpp
        Lathe/LatheProcessor.cpp Lathe/LatheEditor.cpp
        Source/UiKit.cpp Source/BandBus.cpp)
    target_compile_definitions(LiveSim PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 BANDLEADER_NO_ENTRY=1 TEATOWEL_NO_ENTRY=1 UNDERTOW_NO_ENTRY=1
        LOON_NO_ENTRY=1 SCHALTWERK_NO_ENTRY=1 CAROUSEL_NO_ENTRY=1 TAPETWIN_NO_ENTRY=1 LATHE_NO_ENTRY=1)
    target_link_libraries(LiveSim PRIVATE TeaTowelSamples UndertowSamples juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

    juce_add_console_app(StyleTest PRODUCT_NAME "StyleTest")
    target_sources(StyleTest PRIVATE Tools/StyleTest.cpp
        Bandleader/BandleaderProcessor.cpp Bandleader/BandleaderEditor.cpp
        Source/PluginProcessor.cpp Source/PluginEditor.cpp
        Undertow/UndertowProcessor.cpp Undertow/UndertowEditor.cpp
        Loon/LoonProcessor.cpp Loon/LoonEditor.cpp
        Schaltwerk/SchaltwerkProcessor.cpp Schaltwerk/SchaltwerkEditor.cpp
        TapeTwin/TapeTwinProcessor.cpp TapeTwin/TapeTwinEditor.cpp
        Lathe/LatheProcessor.cpp Lathe/LatheEditor.cpp
        Parlour/ParlourProcessor.cpp Parlour/ParlourEditor.cpp
        Ether/EtherProcessor.cpp Ether/EtherEditor.cpp
        Chamber/ChamberProcessor.cpp Chamber/ChamberEditor.cpp
        Jangle/JangleProcessor.cpp Jangle/JangleEditor.cpp
        Harmonia/HarmoniaProcessor.cpp Harmonia/HarmoniaEditor.cpp
        Source/UiKit.cpp Source/BandBus.cpp)
    target_compile_definitions(StyleTest PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 BANDLEADER_NO_ENTRY=1 TEATOWEL_NO_ENTRY=1 UNDERTOW_NO_ENTRY=1
        LOON_NO_ENTRY=1 SCHALTWERK_NO_ENTRY=1 TAPETWIN_NO_ENTRY=1 LATHE_NO_ENTRY=1 PARLOUR_NO_ENTRY=1 ETHER_NO_ENTRY=1 CHAMBER_NO_ENTRY=1 JANGLE_NO_ENTRY=1 HARMONIA_NO_ENTRY=1)
    target_link_libraries(StyleTest PRIVATE TeaTowelSamples UndertowSamples ParlourSamples ChamberSamples JangleSamples juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

    # the song form, learning a song, rendering it to MIDI files
    juce_add_console_app(FormTest PRODUCT_NAME "FormTest")
    target_sources(FormTest PRIVATE Tools/FormTest.cpp
        Bandleader/BandleaderProcessor.cpp Bandleader/BandleaderEditor.cpp
        Source/PluginProcessor.cpp Source/PluginEditor.cpp
        Undertow/UndertowProcessor.cpp Undertow/UndertowEditor.cpp
        Parlour/ParlourProcessor.cpp Parlour/ParlourEditor.cpp
        Chamber/ChamberProcessor.cpp Chamber/ChamberEditor.cpp
        Jangle/JangleProcessor.cpp Jangle/JangleEditor.cpp
        Harmonia/HarmoniaProcessor.cpp Harmonia/HarmoniaEditor.cpp
        Source/UiKit.cpp Source/BandBus.cpp)
    target_compile_definitions(FormTest PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 BANDLEADER_NO_ENTRY=1 TEATOWEL_NO_ENTRY=1 UNDERTOW_NO_ENTRY=1
        PARLOUR_NO_ENTRY=1 CHAMBER_NO_ENTRY=1 JANGLE_NO_ENTRY=1 HARMONIA_NO_ENTRY=1)
    target_link_libraries(FormTest PRIVATE TeaTowelSamples UndertowSamples ParlourSamples ChamberSamples JangleSamples juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
