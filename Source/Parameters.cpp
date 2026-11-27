#include "Parameters.h"
#include "LevelTaper.h"
#include <array>
#include <cmath>
#include <string>

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

/** Amp-style 0–10 level knob (Input / Master / Boost Level / Cab Level). Continuous (no snapping) so legacy dB
    sessions migrate to the exact same gain; text = knob value with one decimal. */
static std::unique_ptr<AudioParameterFloat> levelKnob (const char* pid, const String& name, float def)
{
    return std::make_unique<AudioParameterFloat> (ParameterID { pid, versionHint }, name,
                                                  NormalisableRange<float> (0.0f, 10.0f), def,
                                                  AudioParameterFloatAttributes().withLabel ("")
                                                      .withStringFromValueFunction ([] (float v, int) { return String (v, 1); })
                                                      .withValueFromStringFunction ([] (const String& t) { return jlimit (0.0f, 10.0f, t.getFloatValue()); }));
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

/** Log-scaled frequency knob (Cab HPF / LPF). */
static std::unique_ptr<AudioParameterFloat> freqHz (const char* pid, const String& name,
                                                    float lo, float hi, float def)
{
    NormalisableRange<float> range (lo, hi,
                                    [] (float start, float end, float t) { return start * std::pow (end / start, juce::jlimit (0.0f, 1.0f, t)); },
                                    [] (float start, float end, float v) { return std::log (juce::jlimit (start, end, v) / start) / std::log (end / start); });
    return std::make_unique<AudioParameterFloat> (ParameterID { pid, versionHint }, name, range, def,
                                                  AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (
                                                      [] (float v, int)
                                                      {
                                                          if (v >= 1000.0f)
                                                          {
                                                              const auto k = v / 1000.0f;
                                                              return (std::abs (k - std::round (k)) < 0.05f
                                                                          ? String (juce::roundToInt (k))
                                                                          : String (k, 1))
                                                                   + " kHz";
                                                          }
                                                          return String (juce::roundToInt (v)) + " Hz";
                                                      }));
}

namespace
{
    const char* const legacySlotIds[numAmpSlots] = {
        id::inputTrim, id::odEnable, id::odType, id::odDrive, id::odTone, id::odLevel,
        id::eqEnable, id::eq100, id::eq200, id::eq400, id::eq800, id::eq1k6, id::eq3k2, id::eq6k4, id::eqLevel,
        id::cabEnable, id::cabMicBlend, id::cabLevel,
        id::gateEnable, id::gateThreshold, id::gateHold, id::gateRelease
    };

    struct PrefixedIds
    {
        PrefixedIds()
        {
            for (int i = 0; i < numAmpSlots; ++i)
                ids[(size_t) i] = std::string (amp5150Prefix) + legacySlotIds[i];
            irKeys[0] = std::string (amp5150Prefix) + "ir_a";
            irKeys[1] = std::string (amp5150Prefix) + "ir_b";
        }
        std::array<std::string, numAmpSlots> ids;
        std::array<std::string, 2> irKeys;
    };

    const PrefixedIds& prefixed()
    {
        static const PrefixedIds p;
        return p;
    }
}

const char* ampId (int amp, AmpSlot slot)
{
    const auto i = jlimit (0, numAmpSlots - 1, (int) slot);
    return amp == 0 ? legacySlotIds[i] : prefixed().ids[(size_t) i].c_str();
}

const char* ampIrKey (int amp, int slot)
{
    static const char* legacy[] = { "ir_a", "ir_b" };
    const auto s = jlimit (0, 1, slot);
    return amp == 0 ? legacy[s] : prefixed().irKeys[(size_t) s].c_str();
}

const char* ampCabHpfId (int amp)
{
    static const std::string p5 = std::string (amp5150Prefix) + id::cabHpf;
    return amp == 0 ? id::cabHpf : p5.c_str();
}

const char* ampCabLpfId (int amp)
{
    static const std::string p5 = std::string (amp5150Prefix) + id::cabLpf;
    return amp == 0 ? id::cabLpf : p5.c_str();
}

/** The per-amp slot parameters (boost, post EQ, cab, gate) for one amp.
    AmpSlot::inputTrim is still appended for amp 1 so m5150_input_trim keeps its
    host index; it is unused (both amps use the shared input_trim calibration). */
static void addAmpSlots (AudioProcessorValueTreeState::ParameterLayout& layout, int amp)
{
    const String pre = amp == 0 ? String() : String ("5150 ");
    auto pid = [amp] (AmpSlot s) { return ampId (amp, s); };

    layout.add (levelKnob (pid (AmpSlot::inputTrim), pre + "Input (unused)", taper::inputDefaultKnob));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid (AmpSlot::odEnable), versionHint }, pre + "Boost On", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid (AmpSlot::odType), versionHint }, pre + "Boost Type",
                                                        StringArray { "OD-808", "SD-1" }, 0));
    layout.add (knob (pid (AmpSlot::odDrive), pre + "Boost Drive", 2.0f));
    layout.add (knob (pid (AmpSlot::odTone),  pre + "Boost Tone",  5.0f));
    layout.add (levelKnob (pid (AmpSlot::odLevel), pre + "Boost Level", taper::boostLevelDefaultKnob));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid (AmpSlot::eqEnable), versionHint }, pre + "EQ On", false));
    static const char* bandNames[] = { "EQ 100 Hz", "EQ 200 Hz", "EQ 400 Hz", "EQ 800 Hz", "EQ 1.6 kHz", "EQ 3.2 kHz", "EQ 6.4 kHz" };
    for (int b = 0; b < 7; ++b)
        layout.add (decibels (ampEqBandId (amp, b), pre + bandNames[b], -15.0f, 15.0f, 0.0f));
    layout.add (decibels (pid (AmpSlot::eqLevel), pre + "EQ Level", -15.0f, 15.0f, 0.0f));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid (AmpSlot::cabEnable), versionHint }, pre + "Cab On", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid (AmpSlot::cabMicBlend), versionHint }, pre + "IR Blend",
                                                       NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (
                                                           [] (float v, int) { return String (juce::roundToInt ((1.0f - v) * 100)) + "/" + String (juce::roundToInt (v * 100)); })));
    layout.add (levelKnob (pid (AmpSlot::cabLevel), pre + "Cab Level", taper::cabLevelDefaultKnob));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid (AmpSlot::gateEnable), versionHint }, pre + "Gate On", false));
    layout.add (decibels (pid (AmpSlot::gateThreshold), pre + "Gate Threshold", -80.0f, -20.0f, -60.0f));
    layout.add (milliseconds (pid (AmpSlot::gateHold),    pre + "Gate Hold",    0.0f, 500.0f, 60.0f));
    layout.add (milliseconds (pid (AmpSlot::gateRelease), pre + "Gate Release", 1.0f, 500.0f, 60.0f));
}

AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Shared input calibration (both amps): 0–10, 5 = unity (0 dB), 4.8 dB per
    // number. Not a faceplate gain knob; set once for the interface level.
    layout.add (levelKnob (id::inputTrim, "Input Cal", taper::inputDefaultKnob));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::odEnable, versionHint }, "Boost On", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::odType, versionHint }, "Boost Type",
                                                        StringArray { "OD-808", "SD-1" }, 0));
    layout.add (knob (id::odDrive, "Boost Drive", 2.0f));
    layout.add (knob (id::odTone,  "Boost Tone",  5.0f));
    // Boost Level: 0–10, 3.6 dB per number, 6.67 (20/3) = unity (default). See LevelTaper.h.
    layout.add (levelKnob (id::odLevel, "Boost Level", taper::boostLevelDefaultKnob));

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
    // Cab Level: 0–10, 5 = unity (default), 4.8 dB per number. See LevelTaper.h.
    layout.add (levelKnob (id::cabLevel, "Cab Level", taper::cabLevelDefaultKnob));

    // Gate: key = dry input (post trim), VCA after cab, before master
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::gateEnable, versionHint }, "Gate On", false));
    layout.add (decibels (id::gateThreshold, "Gate Threshold", -80.0f, -20.0f, -60.0f));
    layout.add (milliseconds (id::gateHold,    "Gate Hold",    0.0f, 500.0f, 60.0f));
    layout.add (milliseconds (id::gateRelease, "Gate Release", 1.0f, 500.0f, 60.0f));

    // Master: 0–10 audio taper, 0 = mute, 6.5 = -6 dB (default), 7.5 = unity, 10 = +12 dB.
    layout.add (levelKnob (id::master, "Master", taper::masterDefaultKnob));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::bypass, versionHint }, "Bypass", false));

    // Append: preserve every legacy parameter index as well as ID/default.
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::preampModel, versionHint }, "Preamp Model",
                                                        StringArray { "Rectifier", "5150-inspired" }, 0));

    // New parameter appended after preamp_model; existing indices stay stable.
    layout.add (knob (id::leadPresence, "5150 Presence", 0.0f));
    // Appended after lead_presence. 0 = VR8 shorted = exact pre-Resonance sound.
    layout.add (knob (id::leadResonance, "5150 Resonance", 0.0f));

    // Rev2.4 (appended): the 5150 faceplate, then the 5150's own copy of every
    // per-amp slot (boost, post EQ, cab, gate, and unused m5150_input_trim).
    // The legacy IDs above stay the Rectifier's set (input_trim is the shared
    // calibration); their indices and defaults are unchanged.
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::m5150Channel, versionHint }, "5150 Channel",
                                                        StringArray { "Rhythm", "Lead" }, 1));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::m5150Bright, versionHint }, "5150 Rhythm Bright", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::m5150Crunch, versionHint }, "5150 Rhythm Crunch", false));
    layout.add (knob (id::m5150RhythmPre,  "5150 Rhythm Pre Gain",  5.0f));
    layout.add (knob (id::m5150LeadPre,    "5150 Lead Pre Gain",    6.0f));
    layout.add (knob (id::m5150Low,        "5150 Low",              5.0f));
    layout.add (knob (id::m5150Mid,        "5150 Mid",              4.0f));
    layout.add (knob (id::m5150High,       "5150 High",             5.5f));
    layout.add (knob (id::m5150RhythmPost, "5150 Rhythm Post Gain", 5.0f));
    layout.add (knob (id::m5150LeadPost,   "5150 Lead Post Gain",   5.0f));
    addAmpSlots (layout, 1);

    // Appended: Full Amp (default) vs Preamp Out. Every earlier index stays put,
    // so Full Amp (the default) remains bit-identical to the previous build.
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::preampOut, versionHint }, "Preamp Out",
                                                        StringArray { "Full Amp", "Preamp Out" }, 0));

    // Rev3.2 (appended): tuner pedal. Defaults (off, A = 440 Hz) leave every
    // earlier session untouched (off = bit-identical audio path).
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id::tunerOn, versionHint }, "Tuner", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::tunerRef, versionHint }, "Tuner Reference A",
                                                       NormalisableRange<float> (436.0f, 445.0f, 1.0f), 440.0f,
                                                       AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (
                                                           [] (float v, int) { return "A=" + String (roundToInt (v)) + " Hz"; })));

    // Rev3.2.2 (appended after the tuner): cab HPF / LPF for each amp.
    // Defaults (20 Hz / 20 kHz) are true bypass, so older sessions are unchanged
    // and every earlier host index stays put. Ranges match CabSim::hpfMinHz..lpfMaxHz.
    for (int amp = 0; amp < numAmps; ++amp)
    {
        const String pre = amp == 0 ? String() : String ("5150 ");
        layout.add (freqHz (ampCabHpfId (amp), pre + "Cab HPF", 20.0f, 300.0f, 20.0f));
        layout.add (freqHz (ampCabLpfId (amp), pre + "Cab LPF", 2000.0f, 20000.0f, 20000.0f));
    }

    return layout;
}
} // namespace jb::params
