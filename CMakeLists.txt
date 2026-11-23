cmake_minimum_required(VERSION 3.22)

project(JUICYBOI VERSION 3.0.0 LANGUAGES C CXX)

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
    # Rev3 (solved triode stages) REPLACES Rev2 as the baseline, with a new
    # identity: plugin code, bundle ID, AAX identifier and LV2 URI. Hosts see a
    # different plugin, so Rev2 sessions do not load into Rev3 (docs/triode.md).
    # Rev2 = Jcb2 / "juicyboi Rev2" / com.juicyboi.juicyboi-rev2 (git tag rev2.4);
    # Rev1 = Jcb1 / "juicyboi" (tag rev1). Same manufacturer code throughout.
    PLUGIN_CODE Jcb3
    FORMATS ${JUICYBOI_FORMATS}
    PRODUCT_NAME "juicyboi Rev3"
    BUNDLE_ID com.juicyboi.juicyboi-rev3
    AAX_IDENTIFIER com.juicyboi.juicyboi-rev3
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
    LV2URI "urn:juicyboi:juicyboi-rev3"
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

# The Standalone app carries the version ("juicyboi Rev3.app" for 3.0, "juicyboi
# Rev3.1.app" for 3.1, ...) so builds are easy to tell apart. Within Rev3 the
# plugin name and IDs stay "juicyboi Rev3" / Jcb3, so each 3.x replaces the last.
if(PROJECT_VERSION_MINOR EQUAL 0)
    set(JUICYBOI_APP_NAME "juicyboi Rev${PROJECT_VERSION_MAJOR}")
else()
    set(JUICYBOI_APP_NAME "juicyboi Rev${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
endif()
if(TARGET juicyboi_Standalone)
    set_target_properties(juicyboi_Standalone PROPERTIES
        OUTPUT_NAME "${JUICYBOI_APP_NAME}"
        XCODE_ATTRIBUTE_PRODUCT_NAME "${JUICYBOI_APP_NAME}")
    if(APPLE)
        # JUCE writes one Info.plist per format from PRODUCT_NAME; the app's
        # CFBundleExecutable must match the renamed binary, so patch a copy.
        get_target_property(_jb_plist juicyboi_Standalone MACOSX_BUNDLE_INFO_PLIST)
        file(READ "${_jb_plist}" _jb_plist_text)
        foreach(_jb_key CFBundleExecutable CFBundleName CFBundleDisplayName)
            string(REGEX REPLACE "(<key>${_jb_key}</key>[ \t\r\n]*<string>)[^<]*(</string>)"
                   "\\1${JUICYBOI_APP_NAME}\\2" _jb_plist_text "${_jb_plist_text}")
        endforeach()
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/juicyboi_Standalone-Info.plist" "${_jb_plist_text}")
        set_target_properties(juicyboi_Standalone PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/juicyboi_Standalone-Info.plist")
    endif()
endif()

target_sources(juicyboi
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Parameters.cpp
        Source/gui/Rev1Style.cpp
        Source/gui/AmpHead.cpp
        Source/gui/Pedalboard.cpp
        Source/dsp/Od808Stage.cpp
        Source/dsp/Sd1Stage.cpp
        Source/dsp/PreampCascade.cpp
        Source/dsp/ToneStack.cpp
        Source/dsp/GraphicEq.cpp
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
    target_sources(juicyboi_smoketest PRIVATE Tests/SmokeTest.cpp Tests/PreampModelTest.cpp Tests/ModelValidation.cpp Tests/PresenceTest.cpp Tests/ResonanceTest.cpp Tests/LevelTaperTest.cpp Tests/EditorTest.cpp Tests/ToneStackTest.cpp Tests/ClipperTest.cpp Tests/SpeakerLoadTest.cpp Tests/Amp5150Test.cpp Tests/PerAmpTest.cpp Tests/PreampOutTest.cpp Tests/TriodeTest.cpp)
    # Reuse the plugin's JUCE module objects/includes/defines rather than
    # compiling the modules a second time.
    target_include_directories(juicyboi_smoketest PRIVATE Source
        $<TARGET_PROPERTY:juicyboi,INCLUDE_DIRECTORIES>)
    target_compile_definitions(juicyboi_smoketest PRIVATE
        JUICYBOI_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Tests"
        $<TARGET_PROPERTY:juicyboi,COMPILE_DEFINITIONS>)
    target_link_libraries(juicyboi_smoketest PRIVATE juicyboi juce::juce_recommended_config_flags)

    # Tone-stack response dump (plain C++, no JUCE): tools/tonestack/plot_tonestack.py
    add_executable(juicyboi_tonestack_dump Tests/ToneStackDump.cpp Source/dsp/ToneStack.cpp)
    target_include_directories(juicyboi_tonestack_dump PRIVATE Source)

    # Harmonic spectrum / THD probe (Rev A/B at matched knobs): juicyboi_harmonics [spectrum-dir]
    juce_add_console_app(juicyboi_harmonics PRODUCT_NAME "juicyboi_harmonics")
    target_sources(juicyboi_harmonics PRIVATE Tests/HarmonicsCompare.cpp)
    target_include_directories(juicyboi_harmonics PRIVATE Source
        $<TARGET_PROPERTY:juicyboi,INCLUDE_DIRECTORIES>)
    target_compile_definitions(juicyboi_harmonics PRIVATE
        $<TARGET_PROPERTY:juicyboi,COMPILE_DEFINITIONS>)
    target_link_libraries(juicyboi_harmonics PRIVATE juicyboi juce::juce_recommended_config_flags)

    # Rev2.3 low-end / palm-mute probe (Red v2 bass work): juicyboi_bassprobe [csv-dir|-] [cfg]
    juce_add_console_app(juicyboi_bassprobe PRODUCT_NAME "juicyboi_bassprobe")
    target_sources(juicyboi_bassprobe PRIVATE Tests/BassProbe.cpp)
    target_include_directories(juicyboi_bassprobe PRIVATE Source
        $<TARGET_PROPERTY:juicyboi,INCLUDE_DIRECTORIES>)
    target_compile_definitions(juicyboi_bassprobe PRIVATE
        $<TARGET_PROPERTY:juicyboi,COMPILE_DEFINITIONS>)
    target_link_libraries(juicyboi_bassprobe PRIVATE juicyboi juce::juce_recommended_config_flags)

    # Rev3 dynamics / bias-shift probe + CPU meter: juicyboi_dynamics [chord|levels|cpu|all] [csv-dir]
    juce_add_console_app(juicyboi_dynamics PRODUCT_NAME "juicyboi_dynamics")
    target_sources(juicyboi_dynamics PRIVATE Tests/DynamicsProbe.cpp)
    target_include_directories(juicyboi_dynamics PRIVATE Source
        $<TARGET_PROPERTY:juicyboi,INCLUDE_DIRECTORIES>)
    target_compile_definitions(juicyboi_dynamics PRIVATE
        $<TARGET_PROPERTY:juicyboi,COMPILE_DEFINITIONS>)
    target_link_libraries(juicyboi_dynamics PRIVATE juicyboi juce::juce_recommended_config_flags)

    # Rev3 drive-staging probe (plain C++): juicyboi_triode_tune <voice> [trims|search]
    add_executable(juicyboi_triode_tune Tests/TriodeTune.cpp Source/dsp/PreampCascade.cpp)
    target_include_directories(juicyboi_triode_tune PRIVATE Source)

    # Rev2.2 clipper dump (plain C++, no JUCE): tools/clippers/plot_clippers.py
    add_executable(juicyboi_clipper_dump Tests/ClipperDump.cpp Source/dsp/Od808Stage.cpp Source/dsp/Sd1Stage.cpp)
    target_include_directories(juicyboi_clipper_dump PRIVATE Source)

    # Rev3 triode stage dump (plain C++, no JUCE): tools/triode/plot_triode.py
    add_executable(juicyboi_triode_dump Tests/TriodeDump.cpp)
    target_include_directories(juicyboi_triode_dump PRIVATE Source)

    # Offscreen editor screenshots: juicyboi_screenshots <output-dir>
    juce_add_console_app(juicyboi_screenshots PRODUCT_NAME "juicyboi_screenshots")
    target_sources(juicyboi_screenshots PRIVATE Tests/Screenshots.cpp)
    target_include_directories(juicyboi_screenshots PRIVATE Source
        $<TARGET_PROPERTY:juicyboi,INCLUDE_DIRECTORIES>)
    target_compile_definitions(juicyboi_screenshots PRIVATE
        $<TARGET_PROPERTY:juicyboi,COMPILE_DEFINITIONS>)
    target_link_libraries(juicyboi_screenshots PRIVATE juicyboi juce::juce_recommended_config_flags)
endif()
