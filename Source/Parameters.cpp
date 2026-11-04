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

AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (decibels (id::inputTrim, "Input", -24.0f, 24.0f, 0.0f));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::odEnable, versionHint }, "Boost On", false));
    layout.add (knob (id::odDrive, "Boost Drive", 2.0f));
    layout.add (knob (id::odTone,  "Boost Tone",  5.0f));
    layout.add (decibels (id::odLevel, "Boost Level", -24.0f, 12.0f, 0.0f));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::channel, versionHint }, "Channel",
                                                        StringArray { "Red (Ch1)", "Orange (Ch2)" }, 0));

    layout.add (knob (id::gain,     "Gain",     6.0f));
    layout.add (knob (id::bass,     "Bass",     5.0f));
    layout.add (knob (id::mid,      "Mid",      4.0f));
    layout.add (knob (id::treble,   "Treble",   5.5f));
    layout.add (knob (id::presence, "Presence", 5.0f));

    layout.add (decibels (id::master, "Master", -60.0f, 12.0f, -6.0f));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, versionHint }, "Bypass", false));

    return layout;
}
} // namespace jb::params
