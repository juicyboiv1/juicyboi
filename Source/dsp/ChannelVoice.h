#pragma once

// Dual-channel high-gain preamp voicings derived from Dual Rectifier Rev E–era
// preamp circuit *principles* (shared cascaded 12AX7 path + per-channel FMV
// tone stacks). Original coefficients — not a product clone.
//
// Channel 1 = Red path. Channel 2 = Orange path.
// Schematic cues used (RF-1F / Rev E family style):
//   - Shared cascade: V1A → A250k gain → V2A → V2B → V3A → V3B cathode follower
//   - Separate Red / Orange FMV stacks after the CF (LDR-switched)
//   - Red TMB:   Ct=680p, Cmid=Cbass=22n, Rslope=47k; pots T=250k B=1M M=25k
//   - Orange TMB: Ct=500p, same otherwise
//   - Presence (Red path): 25k + ~22k + 3n ≈ 2.4 kHz NFB-shelf corner
//   - Gain pot: audio taper, 250k (always)

#include <array>
#include <cmath>

namespace jb::dsp
{
struct TriodeStageVoicing
{
    double couplingHpHz;  // coupling / cathode-bypass style low cut
    double plateLpHz;     // Miller / plate-load HF rolloff
    float  gain;          // drive into nonlinearity
    float  bias;          // operating-point offset (even harmonics)
    float  posHardness;   // +half flattening (grid conduction)
    float  negHardness;   // -half flattening (cutoff)
};

struct ChannelVoicing
{
    static constexpr int maxStages = 4;

    int numStages = 4;
    std::array<TriodeStageVoicing, maxStages> stages {};

    int    gainPotAfterStage = 0;
    float  gainPotMin = 0.02f;
    float  gainPotMax = 1.0f;

    double inputTightHpHz = 60.0;
    double postLpHz = 7000.0;
    float  makeup = 0.6f;

    // Passive FMV component cues (ohms / farads) — used by ToneStack
    double fmvCt = 680e-12;   // treble cap
    double fmvCm = 22e-9;     // mid cap
    double fmvCb = 22e-9;     // bass cap
    double fmvRs = 47e3;      // slope resistor
    double fmvRt = 250e3;     // treble pot
    double fmvRb = 1e6;       // bass pot
    double fmvRm = 25e3;      // mid pot

    // Presence shelf corner (NFB-loop principle)
    double presenceHz = 2400.0;

    // Cached EQ centres filled by makeRed / makeOrange
    double bassHz = 100.0, midHz = 650.0, midQ = 0.75, trebleHz = 2800.0;
};

namespace voicings
{
inline void fillFmvCentres (ChannelVoicing& v)
{
    // Rough RC centres from the FMV network (starting points for the
    // placeholder biquad stack; replace later with a full bilinear FMV TF).
    const auto midCorner = 1.0 / (2.0 * 3.141592653589793 * v.fmvRm * v.fmvCm);
    const auto bassCorner = 1.0 / (2.0 * 3.141592653589793 * v.fmvRb * v.fmvCb);
    const auto trebleCorner = 1.0 / (2.0 * 3.141592653589793 * v.fmvRs * v.fmvCt);

    v.bassHz   = std::clamp (bassCorner, 40.0, 200.0);
    v.midHz    = std::clamp (midCorner,  400.0, 1200.0);
    v.midQ     = 0.75;
    // Treble shelf sits above the slope/Ct interaction band
    v.trebleHz = std::clamp (trebleCorner * 2.5, 1800.0, 4500.0);
}

/** Channel 1 — Red: modern / aggressive Dual Rec–style path.
    Brighter FMV (680p treble cap), more stage gain than Orange, with a
    fuller low end through the cascade (input tight ~65 Hz). */
inline ChannelVoicing red()
{
    ChannelVoicing v;
    v.numStages = 4;
    // Shared skeleton inspired by V1A → V2A → V2B (cold) → V3A (+ CF after)
    v.stages = {{
        //  HP     LP        gain   bias    pos    neg
        // Fattened pre-Boost: lower coupling corners pass more low end into
        // the cascade; ~20% more drive per gain stage for saturation that
        // doesn't depend on the OD808. Stage 3 keeps the highest HP so the
        // cold-cathode stage still tightens palm mutes.
        {   30.0, 13000.0,  3.4f,  0.14f,  1.6f,  0.9f }, // V1A-ish
        {   80.0, 10000.0,  9.0f,  0.28f,  1.9f,  1.0f }, // V2A-ish (after gain pot)
        {  140.0,  8200.0,  7.6f,  0.32f,  2.0f,  1.1f }, // V2B cold-cathode bite
        {   22.0,  7500.0,  2.4f,  0.08f,  1.3f,  1.0f }, // V3A → toward CF
    }};
    v.gainPotAfterStage = 0;
    v.gainPotMin = 0.025f;
    v.gainPotMax = 1.35f;
    v.inputTightHpHz = 65.0;
    v.postLpHz = 7800.0;
    v.makeup = 0.58f;

    v.fmvCt = 680e-12;
    v.fmvCm = 22e-9;
    v.fmvCb = 22e-9;
    v.fmvRs = 47e3;
    v.fmvRt = 250e3;
    v.fmvRb = 1e6;
    v.fmvRm = 25e3;
    v.presenceHz = 2400.0;
    fillFmvCentres (v);
    return v;
}

/** Channel 2 — Orange: vintage / looser Dual Rec–style path.
    Softer FMV (500p treble cap), fatter low-mids, slightly less gain. */
inline ChannelVoicing orange()
{
    ChannelVoicing v;
    v.numStages = 4;
    v.stages = {{
        {   28.0, 12000.0,  2.4f,  0.14f,  1.55f, 0.9f },
        {   85.0,  9200.0,  6.2f,  0.24f,  1.75f, 1.0f },
        {  160.0,  7200.0,  5.2f,  0.22f,  1.7f,  1.0f },
        {   20.0,  6500.0,  1.8f,  0.06f,  1.25f, 1.0f },
    }};
    v.gainPotAfterStage = 0;
    v.gainPotMin = 0.02f;
    v.gainPotMax = 1.0f;
    v.inputTightHpHz = 55.0;
    v.postLpHz = 6400.0;
    v.makeup = 0.68f;

    v.fmvCt = 500e-12;
    v.fmvCm = 22e-9;
    v.fmvCb = 22e-9;
    v.fmvRs = 47e3;
    v.fmvRt = 250e3;
    v.fmvRb = 1e6;
    v.fmvRm = 25e3;
    v.presenceHz = 2200.0;
    fillFmvCentres (v);
    return v;
}

// Compatibility aliases used by the processor (A = ch1 Red, B = ch2 Orange)
inline ChannelVoicing riffA() { return red(); }
inline ChannelVoicing riffB() { return orange(); }
} // namespace voicings
} // namespace jb::dsp
