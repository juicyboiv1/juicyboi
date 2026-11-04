cmake_minimum_required(VERSION 3.22)

project(JUICYBOI VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# JUCE location: override with -DJUCE_DIR=/path/to/JUCE
set(JUCE_DIR "/workspace/JUCE" CACHE PATH "Path to the JUCE source tree")
# Standalone: list interface inputs as mono (Scarlett 1/2/3/4), not stereo pairs
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PatchJuceStandaloneMonoInputs.cmake)

add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

# Linux: VST3 + Standalone (+ LV2). macOS: AU. macOS/Windows: AAX (Pro Tools).
# AAX uses the SDK bundled with JUCE 9 under juce_audio_plugin_client/AAX/SDK
# (override with juce_set_aax_sdk_path if you have a separate Avid SDK tree).
set(JUICYBOI_FORMATS VST3 Standalone LV2)
if(APPLE)
    list(APPEND JUICYBOI_FORMATS AU)
endif()
if(APPLE OR WIN32)
    list(APPEND JUICYBOI_FORMATS AAX)
endif()

juce_add_plugin(juicyboi
    COMPANY_NAME "juicyboi"
    PLUGIN_MANUFACTURER_CODE Jcyb
    PLUGIN_CODE Jcb1
    FORMATS ${JUICYBOI_FORMATS}
    PRODUCT_NAME "juicyboi"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "juicyboi needs microphone access to take guitar input from your audio interface."
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion
    AAX_CATEGORY Effect
    LV2URI "urn:juicyboi:juicyboi"
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(juicyboi
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Parameters.cpp
        Source/dsp/Od808Stage.cpp
        Source/dsp/PreampCascade.cpp
        Source/dsp/ToneStack.cpp
        Source/dsp/CabSim.cpp
        Source/dsp/NoiseGate.cpp)

target_include_directories(juicyboi PRIVATE Source)

target_compile_definitions(juicyboi
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(juicyboi
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional headless smoke test (links the plugin's shared-code target)
option(JUICYBOI_BUILD_TESTS "Build the headless smoke test" ON)
if(JUICYBOI_BUILD_TESTS)
    juce_add_console_app(juicyboi_smoketest PRODUCT_NAME "juicyboi_smoketest")
    target_sources(juicyboi_smoketest PRIVATE Tests/SmokeTest.cpp)
    # Reuse the plugin's JUCE module objects/includes/defines rather than
    # compiling the modules a second time.
    target_include_directories(juicyboi_smoketest PRIVATE Source
        $<TARGET_PROPERTY:juicyboi,INCLUDE_DIRECTORIES>)
    target_compile_definitions(juicyboi_smoketest PRIVATE
        $<TARGET_PROPERTY:juicyboi,COMPILE_DEFINITIONS>)
    target_link_libraries(juicyboi_smoketest PRIVATE juicyboi juce::juce_recommended_config_flags)
endif()
