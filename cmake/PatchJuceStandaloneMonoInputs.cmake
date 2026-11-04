# JUCE Standalone hard-codes stereo-paired channel checkboxes and sizes the
# input selector from the plugin's default bus (mono = 1). For a Scarlett 4i4
# we want Input 1..4 as separate mono checkboxes.
set(_juicyboi_standalone_h
    "${JUCE_DIR}/modules/juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h")

if(NOT EXISTS "${_juicyboi_standalone_h}")
    message(FATAL_ERROR "JUCE Standalone header not found: ${_juicyboi_standalone_h}")
endif()

file(READ "${_juicyboi_standalone_h}" _juicyboi_sa)

set(_juicyboi_sa_orig "${_juicyboi_sa}")

# showChannelsAsStereoPairs: true -> false (unique context near producesMidi())
string(REPLACE
    "                              (pluginHolder.processor.get() != nullptr && pluginHolder.processor->producesMidi()),\n                              true, false),"
    "                              (pluginHolder.processor.get() != nullptr && pluginHolder.processor->producesMidi()),\n                              false, false),"
    _juicyboi_sa "${_juicyboi_sa}")

# Offer at least 4 input checkboxes in the settings dialog
string(REPLACE
    "        if (auto* bus = processor->getBus (true, 0))\n            maxNumInputs = jmax (0, bus->getDefaultLayout().size());"
    "        if (auto* bus = processor->getBus (true, 0))\n            maxNumInputs = jmax (4, bus->getDefaultLayout().size());"
    _juicyboi_sa "${_juicyboi_sa}")

# Prefer opening the device with 4 input slots so any single Scarlett channel can be enabled
string(REPLACE
    "        return (channelConfiguration.size() > 0 ? channelConfiguration[0].numIns\n                                                : processor->getMainBusNumInputChannels());"
    "        return (channelConfiguration.size() > 0 ? channelConfiguration[0].numIns\n                                                : jmax (4, processor->getMainBusNumInputChannels()));"
    _juicyboi_sa "${_juicyboi_sa}")

if(_juicyboi_sa STREQUAL _juicyboi_sa_orig)
    string(FIND "${_juicyboi_sa}" "maxNumInputs = jmax (4, bus->getDefaultLayout().size());" _already)
    if(_already EQUAL -1)
        message(WARNING "juicyboi: could not patch JUCE Standalone for mono inputs; UI may still pair channels")
    else()
        message(STATUS "juicyboi: JUCE Standalone already patched for mono inputs")
    endif()
else()
    file(WRITE "${_juicyboi_standalone_h}" "${_juicyboi_sa}")
    message(STATUS "juicyboi: patched JUCE Standalone for 4 mono input checkboxes")
endif()
