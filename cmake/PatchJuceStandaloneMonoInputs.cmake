# Patches JUCE's Standalone wrapper (juce_StandaloneFilterWindow.h) for juicyboi.
#
# 1) Mono input checkboxes: JUCE hard-codes stereo-paired channel checkboxes and
#    sizes the input selector from the plugin's default bus (mono = 1). For a
#    Scarlett 4i4 we want Input 1..4 as separate mono checkboxes.
# 2) Input NOT muted by default: stock JUCE Standalone starts with
#    "Mute audio input" (Feedback Loop) ON whenever the plugin has ins + outs,
#    and persists that as "shouldMuteInput" in the app's settings file. With an
#    interface like the Scarlett (headphones, not open speakers) this just
#    silences the guitar: device/channel routing looks right, but the plugin
#    receives zeros. We default it OFF and store it under a new key, so a stale
#    "muted" value from earlier builds can't keep the input silenced.
#
# Each replacement is applied independently, so an already-patched JUCE tree
# (from an older juicyboi build) still picks up any newer patches.
# Unmute patches are REQUIRED — configure fails if they cannot be applied, so a
# Mini rebuild cannot silently keep the stock mute-on-default.
set(_juicyboi_standalone_h
    "${JUCE_DIR}/modules/juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h")
set(_juicyboi_standalone_cpp
    "${JUCE_DIR}/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp")

if(NOT EXISTS "${_juicyboi_standalone_h}")
    message(FATAL_ERROR "JUCE Standalone header not found: ${_juicyboi_standalone_h}")
endif()

file(READ "${_juicyboi_standalone_h}" _juicyboi_sa)
set(_juicyboi_sa_orig "${_juicyboi_sa}")
set(_juicyboi_sa_failed "")
set(_juicyboi_sa_applied "")
set(_juicyboi_sa_skipped "")

# _juicyboi_sa_patch(<label> <from> <to>)
function(_juicyboi_sa_patch label from to)
    string(FIND "${_juicyboi_sa}" "${to}" _done)
    if(NOT _done EQUAL -1)
        set(_juicyboi_sa_skipped "${_juicyboi_sa_skipped} ${label}" PARENT_SCOPE)
        return() # already applied
    endif()
    string(FIND "${_juicyboi_sa}" "${from}" _found)
    if(_found EQUAL -1)
        set(_juicyboi_sa_failed "${_juicyboi_sa_failed} ${label}" PARENT_SCOPE)
        return()
    endif()
    string(REPLACE "${from}" "${to}" _out "${_juicyboi_sa}")
    set(_juicyboi_sa "${_out}" PARENT_SCOPE)
    set(_juicyboi_sa_applied "${_juicyboi_sa_applied} ${label}" PARENT_SCOPE)
endfunction()

# --- 1) mono input checkboxes -------------------------------------------------
# showChannelsAsStereoPairs: true -> false (unique context near producesMidi())
_juicyboi_sa_patch(mono-pairs
    "                              (pluginHolder.processor.get() != nullptr && pluginHolder.processor->producesMidi()),\n                              true, false),"
    "                              (pluginHolder.processor.get() != nullptr && pluginHolder.processor->producesMidi()),\n                              false, false),")

# Offer at least 4 input checkboxes in the settings dialog
_juicyboi_sa_patch(max-inputs
    "        if (auto* bus = processor->getBus (true, 0))\n            maxNumInputs = jmax (0, bus->getDefaultLayout().size());"
    "        if (auto* bus = processor->getBus (true, 0))\n            maxNumInputs = jmax (4, bus->getDefaultLayout().size());")

# Prefer opening the device with 4 input slots so any single Scarlett channel can be enabled
# (getNumInputChannels used by reloadAudioDeviceState / initialise)
_juicyboi_sa_patch(open-4-inputs
    "        return (channelConfiguration.size() > 0 ? channelConfiguration[0].numIns\n                                                : processor->getMainBusNumInputChannels());"
    "        return (channelConfiguration.size() > 0 ? channelConfiguration[0].numIns\n                                                : jmax (4, processor->getMainBusNumInputChannels()));")

# Constructor's audioInputRequired channel count (different indent than getNumInputChannels)
_juicyboi_sa_patch(ctor-4-inputs
    "        auto inChannels = (channelConfiguration.size() > 0 ? channelConfiguration[0].numIns\n                                                           : processor->getMainBusNumInputChannels());"
    "        auto inChannels = (channelConfiguration.size() > 0 ? channelConfiguration[0].numIns\n                                                           : jmax (4, processor->getMainBusNumInputChannels()));")

# --- 2) input not muted by default -------------------------------------------
_juicyboi_sa_patch(unmute-ctor
    "        shouldMuteInput.addListener (this);\n        shouldMuteInput = ! isInterAppAudioConnected();"
    "        shouldMuteInput.addListener (this);\n        shouldMuteInput = false; // juicyboi: guitar input live by default")

_juicyboi_sa_patch(unmute-atomic
    "    std::atomic<bool> muteInput { true };"
    "    std::atomic<bool> muteInput { false }; // juicyboi")

_juicyboi_sa_patch(unmute-load
    "shouldMuteInput.setValue (settings->getBoolValue (\"shouldMuteInput\", true));"
    "shouldMuteInput.setValue (settings->getBoolValue (\"juicyboiMuteInput\", false));")

_juicyboi_sa_patch(unmute-save
    "settings->setValue (\"shouldMuteInput\", (bool) shouldMuteInput.getValue());"
    "settings->setValue (\"juicyboiMuteInput\", (bool) shouldMuteInput.getValue());")

# Unmute is required for Scarlett Standalone. Mono patches are best-effort.
set(_juicyboi_unmute_required "unmute-ctor unmute-atomic unmute-load unmute-save")
foreach(_req IN ITEMS unmute-ctor unmute-atomic unmute-load unmute-save)
    string(FIND "${_juicyboi_sa_failed}" " ${_req}" _req_fail)
    if(NOT _req_fail EQUAL -1)
        message(FATAL_ERROR
            "juicyboi: REQUIRED JUCE Standalone unmute patch '${_req}' failed.\n"
            "  JUCE_DIR=${JUCE_DIR}\n"
            "  Header=${_juicyboi_standalone_h}\n"
            "  Applied:${_juicyboi_sa_applied}\n"
            "  Skipped (already present):${_juicyboi_sa_skipped}\n"
            "  Failed:${_juicyboi_sa_failed}\n"
            "  Stock JUCE 9.x strings may have changed, or this header is not the Standalone wrapper.\n"
            "  Fix: restore that header from your JUCE checkout and re-run cmake configure.")
    endif()
endforeach()

if(NOT _juicyboi_sa_failed STREQUAL "")
    message(WARNING "juicyboi: optional JUCE Standalone patch(es) failed:${_juicyboi_sa_failed}. "
                    "Input checkboxes may still be stereo-paired.")
endif()

if(NOT _juicyboi_sa STREQUAL _juicyboi_sa_orig)
    file(WRITE "${_juicyboi_standalone_h}" "${_juicyboi_sa}")
    message(STATUS "juicyboi: patched JUCE Standalone (applied:${_juicyboi_sa_applied}; skipped:${_juicyboi_sa_skipped})")
    # Force the Standalone translation unit to rebuild against the new header.
    if(EXISTS "${_juicyboi_standalone_cpp}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E touch "${_juicyboi_standalone_cpp}")
    endif()
else()
    message(STATUS "juicyboi: JUCE Standalone patches already present (skipped:${_juicyboi_sa_skipped})")
endif()

# Verify final header state (catches "already patched" lies if someone hand-edited).
file(READ "${_juicyboi_standalone_h}" _juicyboi_sa_verify)
string(FIND "${_juicyboi_sa_verify}" "juicyboiMuteInput" _has_key)
string(FIND "${_juicyboi_sa_verify}" "muteInput { false }" _has_atomic)
string(FIND "${_juicyboi_sa_verify}" "guitar input live by default" _has_ctor)
if(_has_key EQUAL -1 OR _has_atomic EQUAL -1 OR _has_ctor EQUAL -1)
    message(FATAL_ERROR
        "juicyboi: JUCE Standalone header is missing unmute markers after patch.\n"
        "  Need: juicyboiMuteInput, muteInput { false }, guitar input live by default\n"
        "  Header: ${_juicyboi_standalone_h}")
endif()
