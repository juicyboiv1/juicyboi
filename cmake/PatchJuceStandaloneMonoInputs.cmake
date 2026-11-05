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
set(_juicyboi_standalone_h
    "${JUCE_DIR}/modules/juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h")

if(NOT EXISTS "${_juicyboi_standalone_h}")
    message(FATAL_ERROR "JUCE Standalone header not found: ${_juicyboi_standalone_h}")
endif()

file(READ "${_juicyboi_standalone_h}" _juicyboi_sa)
set(_juicyboi_sa_orig "${_juicyboi_sa}")
set(_juicyboi_sa_failed "")

# _juicyboi_sa_patch(<label> <from> <to>)
function(_juicyboi_sa_patch label from to)
    string(FIND "${_juicyboi_sa}" "${to}" _done)
    if(NOT _done EQUAL -1)
        return() # already applied
    endif()
    string(FIND "${_juicyboi_sa}" "${from}" _found)
    if(_found EQUAL -1)
        set(_juicyboi_sa_failed "${_juicyboi_sa_failed} ${label}" PARENT_SCOPE)
        return()
    endif()
    string(REPLACE "${from}" "${to}" _out "${_juicyboi_sa}")
    set(_juicyboi_sa "${_out}" PARENT_SCOPE)
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
_juicyboi_sa_patch(open-4-inputs
    "        return (channelConfiguration.size() > 0 ? channelConfiguration[0].numIns\n                                                : processor->getMainBusNumInputChannels());"
    "        return (channelConfiguration.size() > 0 ? channelConfiguration[0].numIns\n                                                : jmax (4, processor->getMainBusNumInputChannels()));")

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

if(NOT _juicyboi_sa_failed STREQUAL "")
    message(WARNING "juicyboi: could not apply JUCE Standalone patch(es):${_juicyboi_sa_failed}. "
                    "Standalone may pair input channels or start with the input muted "
                    "(Options > Audio/MIDI Settings > untick 'Mute audio input').")
endif()

if(_juicyboi_sa STREQUAL _juicyboi_sa_orig)
    message(STATUS "juicyboi: JUCE Standalone already patched (mono inputs, input unmuted)")
else()
    file(WRITE "${_juicyboi_standalone_h}" "${_juicyboi_sa}")
    message(STATUS "juicyboi: patched JUCE Standalone (mono inputs, input unmuted by default)")
endif()
