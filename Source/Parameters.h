#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace jb::params
{
// Parameter IDs (stable — do not rename once presets exist)
namespace id
{
    inline constexpr const char* inputTrim  = "input_trim";
    inline constexpr const char* odEnable   = "od_enable";
    inline constexpr const char* odType     = "od_type"; // Boost type: 0 = OD-808, 1 = SD-1
    inline constexpr const char* odDrive    = "od_drive";
    inline constexpr const char* odTone     = "od_tone";
    inline constexpr const char* odLevel    = "od_level";
    inline constexpr const char* leadPresence = "lead_presence"; // 5150 closed-loop approximation, 0=reference
    inline constexpr const char* leadResonance = "lead_resonance"; // 5150 NFB Resonance (VR8/C12), 0=reference
    inline constexpr const char* preampModel = "preamp_model"; // 0 Rectifier, 1 5150-inspired Lead
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

// Rev2.4: the 5150 amp's own controls (appended; every legacy index unchanged).
namespace id
{
    inline constexpr const char* m5150Channel     = "m5150_channel";      // 0 = Rhythm, 1 = Lead
    inline constexpr const char* m5150Bright      = "m5150_bright";       // Rhythm Bright switch
    inline constexpr const char* m5150Crunch      = "m5150_crunch";       // Rhythm Crunch switch
    inline constexpr const char* m5150RhythmPre   = "m5150_rhythm_pre";
    inline constexpr const char* m5150LeadPre     = "m5150_lead_pre";
    inline constexpr const char* m5150Low         = "m5150_low";
    inline constexpr const char* m5150Mid         = "m5150_mid";
    inline constexpr const char* m5150High        = "m5150_high";
    inline constexpr const char* m5150RhythmPost  = "m5150_rhythm_post";
    inline constexpr const char* m5150LeadPost    = "m5150_lead_post";
}

/** Rev2.4 per-amp settings. Amp 0 = Rectifier (Red v1 / Red v2 / Orange share
    ONE set: they are channels/voicings of one amp; its legacy IDs are its set),
    amp 1 = 5150. Each slot below exists once per amp: amp 0 uses the legacy ID,
    amp 1 the same ID with the "m5150_" prefix. Amp knobs are per amp by
    construction (Rectifier: pre_gain..pre_presence, channel, red_voice, master;
    5150: the m5150_* knobs above plus lead_presence / lead_resonance).
    Global (not per amp): preamp_model, bypass, and input_trim (shared input
    calibration for both amps; no longer a per-amp head knob).
    AmpSlot::inputTrim stays in this list so m5150_input_trim keeps its host
    index, but that 5150 slot parameter is unused and hidden. See docs/per-amp-state.md. */
enum class AmpSlot
{
    inputTrim, odEnable, odType, odDrive, odTone, odLevel,
    eqEnable, eq100, eq200, eq400, eq800, eq1k6, eq3k2, eq6k4, eqLevel,
    cabEnable, cabMicBlend, cabLevel,
    gateEnable, gateThreshold, gateHold, gateRelease,
    count
};
inline constexpr int numAmps = 2;
inline constexpr int numAmpSlots = (int) AmpSlot::count;
inline constexpr const char* amp5150Prefix = "m5150_";

/** Parameter ID of a per-amp slot for amp 0 (Rectifier) or 1 (5150). */
const char* ampId (int amp, AmpSlot slot);
inline const char* ampEqBandId (int amp, int band) { return ampId (amp, (AmpSlot) ((int) AmpSlot::eq100 + band)); }
/** Root-state property keys of the cab IR file paths per amp. */
const char* ampIrKey (int amp, int slot);

inline constexpr int versionHint = 1;

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
} // namespace jb::params
