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
    inline constexpr const char* redVoice   = "red_voice"; // Red (Ch1) voicing: 0 = v1, 1 = v2
    inline constexpr const char* gain       = "pre_gain";
    inline constexpr const char* bass       = "pre_bass";
    inline constexpr const char* mid        = "pre_mid";
    inline constexpr const char* treble     = "pre_treble";
    inline constexpr const char* presence   = "pre_presence";
    inline constexpr const char* master     = "master";
    inline constexpr const char* bypass     = "bypass";
    // Graphic EQ (post-ToneStack / pre-Cab): 7 bands ±15 dB + Level ±15 dB
    inline constexpr const char* eqEnable   = "eq_enable";
    inline constexpr const char* eq100      = "eq_100";
    inline constexpr const char* eq200      = "eq_200";
    inline constexpr const char* eq400      = "eq_400";
    inline constexpr const char* eq800      = "eq_800";
    inline constexpr const char* eq1k6      = "eq_1k6";
    inline constexpr const char* eq3k2      = "eq_3k2";
    inline constexpr const char* eq6k4      = "eq_6k4";
    inline constexpr const char* eqLevel    = "eq_level";
    inline constexpr const char* eqBands[] = { eq100, eq200, eq400, eq800, eq1k6, eq3k2, eq6k4 };
    inline constexpr const char* cabEnable  = "cab_enable";
    inline constexpr const char* cabMicBlend  = "cab_mic_blend"; // kept ID for session compat; UI = "IR Blend" (0 = IR A, 1 = IR B)
    inline constexpr const char* cabLevel     = "cab_level";
    inline constexpr const char* gateEnable    = "gate_enable";
    inline constexpr const char* gateThreshold = "gate_threshold";
    inline constexpr const char* gateHold      = "gate_hold";
    inline constexpr const char* gateRelease   = "gate_release";
}

inline constexpr int versionHint = 1;

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
} // namespace jb::params
