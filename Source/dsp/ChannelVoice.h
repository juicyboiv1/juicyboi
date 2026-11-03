#pragma once

// Channel voicings for the dual-channel high-gain preamp.
//
// The two "Riff" channels follow the general principles of a classic
// two-channel, cascaded-triode high-gain American preamp design (Rev E-era
// style): both channels share the same cascaded-stage skeleton but differ in
// coupling/bypass corners (tightness), stage gain, stage bias (asymmetry),
// HF rolloff and tone-stack centre frequencies.
//
// Every number here is an ORIGINAL starting point chosen by ear-target, not a
// measured or copied product value. Replace with values derived from your own
// circuit analysis / measurements as the project progresses.

#include <array>

namespace jb::dsp
{
struct TriodeStageVoicing
{
    double couplingHpHz;  // coupling cap / cathode-bypass style low cut
    double plateLpHz;     // Miller / plate-load style HF rolloff
    float  gain;          // linear drive into the nonlinearity
    float  bias;          // operating-point offset (asymmetry, even harmonics)
    float  posHardness;   // positive half flattening (grid conduction)
    float  negHardness;   // negative half flattening (cutoff)
};

struct ChannelVoicing
{
    static constexpr int maxStages = 4;

    int numStages = 4;
    std::array<TriodeStageVoicing, maxStages> stages {};

    int    gainPotAfterStage = 0;   // stage index after which the Gain pot sits
    float  gainPotMin = 0.02f;      // pot attenuation at Gain = 0
    float  gainPotMax = 1.0f;       // pot attenuation at Gain = 10

    double inputTightHpHz = 60.0;   // pre-cascade low cut (tightness)
    double postLpHz = 7000.0;       // post-cascade fizz control
    float  makeup = 0.6f;           // output level normalisation per channel

    // Tone stack / presence centre frequencies used by ToneStack
    double bassHz = 100.0, midHz = 650.0, midQ = 0.8, trebleHz = 2800.0, presenceHz = 4000.0;
};

namespace voicings
{
/** Riff A: vintage-leaning heavy rhythm. Fatter low-mids, slightly less
    stage gain, warmer top. */
inline ChannelVoicing riffA()
{
    ChannelVoicing v;
    v.numStages = 4;
    v.stages = {{
        //  HP     LP       gain  bias   pos   neg
        {   30.0, 12000.0, 2.5f, 0.15f, 1.6f, 0.9f },
        {   80.0,  9000.0, 6.0f, 0.25f, 1.8f, 1.0f },
        {  150.0,  7000.0, 5.0f, 0.20f, 1.7f, 1.0f },
        {   20.0,  6500.0, 1.5f, 0.05f, 1.2f, 1.0f },  // softer "driver" stage
    }};
    v.gainPotAfterStage = 0;
    v.gainPotMin = 0.02f;
    v.gainPotMax = 1.0f;
    v.inputTightHpHz = 60.0;
    v.postLpHz = 6500.0;
    v.makeup = 0.7f;
    v.bassHz = 110.0; v.midHz = 600.0; v.midQ = 0.8; v.trebleHz = 2600.0; v.presenceHz = 3800.0;
    return v;
}

/** Riff B: modern-leaning tight/saturated. Higher interstage low cut, more
    gain, brighter, mid control centred a bit higher for scooped settings. */
inline ChannelVoicing riffB()
{
    ChannelVoicing v;
    v.numStages = 4;
    v.stages = {{
        //  HP     LP       gain  bias   pos   neg
        {   40.0, 14000.0, 3.0f, 0.15f, 1.6f, 0.9f },
        {  140.0, 10000.0, 8.0f, 0.30f, 1.9f, 1.0f },
        {  250.0,  8000.0, 7.0f, 0.25f, 1.8f, 1.0f },
        {   30.0,  7500.0, 2.0f, 0.08f, 1.3f, 1.0f },
    }};
    v.gainPotAfterStage = 0;
    v.gainPotMin = 0.03f;
    v.gainPotMax = 1.2f;
    v.inputTightHpHz = 110.0;
    v.postLpHz = 7500.0;
    v.makeup = 0.55f;
    v.bassHz = 90.0; v.midHz = 750.0; v.midQ = 0.7; v.trebleHz = 3200.0; v.presenceHz = 4500.0;
    return v;
}
} // namespace voicings
} // namespace jb::dsp
