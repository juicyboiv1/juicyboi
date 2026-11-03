#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace jb::params
{
// Parameter IDs (stable — do not rename once presets exist)
namespace id
{
    inline constexpr const char* inputTrim  = "input_trim";
    inline constexpr const char* odEnable   = "od_enable";
    inline constexpr const char* odDrive    = "od_drive";
    inline constexpr const char* odTone     = "od_tone";
    inline constexpr const char* odLevel    = "od_level";
    inline constexpr const char* channel    = "channel";
    inline constexpr const char* gain       = "pre_gain";
    inline constexpr const char* bass       = "pre_bass";
    inline constexpr const char* mid        = "pre_mid";
    inline constexpr const char* treble     = "pre_treble";
    inline constexpr const char* presence   = "pre_presence";
    inline constexpr const char* master     = "master";
    inline constexpr const char* bypass     = "bypass";
}

inline constexpr int versionHint = 1;

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
} // namespace jb::params
