#include "Parameters.h"

namespace jb::params
{
using namespace juce;

static std::unique_ptr<AudioParameterFloat> knob (const char* pid, const String& name, float def)
{
    return std::make_unique<AudioParameterFloat> (ParameterID { pid, versionHint }, name,
                                                  NormalisableRange<float> (0.0f, 10.0f, 0.01f), def,
                                                  AudioParameterFloatAttributes().withLabel ("").withStringFromValueFunction (
                                                      [] (float v, int) { return String (v, 1); }));
}

static std::unique_ptr<AudioParameterFloat> decibels (const char* pid, const String& name,
                                                      float lo, float hi, float def)
{
    return std::make_unique<AudioParameterFloat> (ParameterID { pid, versionHint }, name,
                                                  NormalisableRange<float> (lo, hi, 0.1f), def,
                                                  AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
                                                      [] (float v, int) { return String (v, 1) + " dB"; }));
}

static std::unique_ptr<AudioParameterFloat> milliseconds (const char* pid, const String& name,
                                                          float lo, float hi, float def)
{
    NormalisableRange<float> range (lo, hi, 0.1f);
    range.setSkewForCentre (juce::jmin (hi, lo + 80.0f)); // finer control at short times
    return std::make_unique<AudioParameterFloat> (ParameterID { pid, versionHint }, name, range, def,
                                                  AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction (
                                                      [] (float v, int) { return String (juce::roundToInt (v)) + " ms"; }));
}

AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (decibels (id::inputTrim, "Input", -24.0f, 24.0f, 0.0f));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::odEnable, versionHint }, "Boost On", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::odType, versionHint }, "Boost Type",
                                                        StringArray { "OD-808", "SD-1" }, 0));
    layout.add (knob (id::odDrive, "Boost Drive", 2.0f));
    layout.add (knob (id::odTone,  "Boost Tone",  5.0f));
    layout.add (decibels (id::odLevel, "Boost Level", -24.0f, 12.0f, 0.0f));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::channel, versionHint }, "Channel",
                                                        StringArray { "Red (Ch1)", "Orange (Ch2)" }, 0));
    // Red-channel A/B: v1 = original tight Red, v2 = max-gain / fat Red (default)
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::redVoice, versionHint }, "Red Voice",
                                                        StringArray { "v1", "v2" }, 1));

    layout.add (knob (id::gain,     "Gain",     6.0f));
    layout.add (knob (id::bass,     "Bass",     5.0f));
    layout.add (knob (id::mid,      "Mid",      4.0f));
    layout.add (knob (id::treble,   "Treble",   5.5f));
    layout.add (knob (id::presence, "Presence", 5.0f));

    // Graphic EQ: post-ToneStack / pre-Cab. Off by default = true bypass.
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::eqEnable, versionHint }, "EQ On", false));
    {
        static const char* bandNames[] = { "EQ 100 Hz", "EQ 200 Hz", "EQ 400 Hz", "EQ 800 Hz",
                                           "EQ 1.6 kHz", "EQ 3.2 kHz", "EQ 6.4 kHz" };
        for (int b = 0; b < 7; ++b)
            layout.add (decibels (id::eqBands[b], bandNames[b], -15.0f, 15.0f, 0.0f));
    }
    layout.add (decibels (id::eqLevel, "EQ Level", -15.0f, 15.0f, 0.0f));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::cabEnable, versionHint }, "Cab On", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::cabMicBlend, versionHint }, "IR Blend",
                                                       NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (
                                                           [] (float v, int) { return String (juce::roundToInt ((1.0f - v) * 100)) + "/" + String (juce::roundToInt (v * 100)); })));
    layout.add (decibels (id::cabLevel, "Cab Level", -24.0f, 24.0f, 0.0f));

    // Gate: key = dry input (post trim), VCA after cab, before master
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::gateEnable, versionHint }, "Gate On", false));
    layout.add (decibels (id::gateThreshold, "Gate Threshold", -80.0f, -20.0f, -60.0f));
    layout.add (milliseconds (id::gateHold,    "Gate Hold",    0.0f, 500.0f, 60.0f));
    layout.add (milliseconds (id::gateRelease, "Gate Release", 1.0f, 500.0f, 60.0f));

    layout.add (decibels (id::master, "Master", -60.0f, 12.0f, -6.0f));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, versionHint }, "Bypass", false));

    // Append: preserve every legacy parameter index as well as ID/default.
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::preampModel, versionHint }, "Preamp Model",
                                                        StringArray { "Rectifier", "5150-inspired" }, 0));

    // New parameter appended after preamp_model; existing indices stay stable.
    layout.add (knob (id::leadPresence, "5150 Presence", 0.0f));

    return layout;
}
} // namespace jb::params
