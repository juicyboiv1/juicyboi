#pragma once

// Dual-channel high-gain preamp voicings derived from Dual Rectifier Rev E–era
// preamp circuit *principles* (shared cascaded 12AX7 path + per-channel passive
// tone stacks). Original coefficients — not a product clone.
//
// Channel 1 = Red path. Channel 2 = Orange path.
// Schematic cues used (RF-1F / Rev E family style):
//   - Shared cascade: V1A → A250k gain → V2A → V2B → V3A → V3B cathode follower
//   - Separate Red / Orange passive TMB stacks after the CF (LDR-switched)
//   - TMB parts (Rev2 solves the real network; values read off the RF-1F
//     preamp sheet, see redTmb / orangeTmb below and docs/tonestack.md):
//       Red:    Treble 250K, C4 680pF, R273 47K, C23 .02uF bass, C24 .02uF mid,
//               Bass 1M, Mid 25K
//       Orange: Treble 250K, C5 500pF, R274 47K, C25 .02uF, C26 .02uF, 1M, 25K
//   - Presence (Red path): 25k + ~22k + 3n ≈ 2.4 kHz NFB-shelf corner
//   - Gain pot: audio taper, 250k (always)

#include "TriodeParts.h"

#include <array>
#include <algorithm>
#include <cmath>

namespace jb::dsp
{
/** Passive Treble/Bass/Mid stack parts (ohms / farads) + fixed makeup gain.
    Solved by jb::dsp::tmb (ToneStack.h). Defaults = Red set (RF-1F sheet). */
struct TmbComponents
{
    double rt = 250e3;    // treble pot
    double rb = 1e6;      // bass pot (rheostat)
    double rm = 25e3;     // mid pot
    double rs = 47e3;     // slope resistor
    double ct = 680e-12;  // treble cap
    double cb = 20e-9;    // bass cap
    double cm = 20e-9;    // mid cap
    double makeupDb = 5.0; // after the stack: brings default-knob level back to Rev1
};

/** Speaker-load interaction parts (SpeakerLoad.h, Rev2.3). res = 0 -> bypass. */
struct SpeakerLoadParams
{
    double re   = 6.4;   // speaker DC (voice-coil) resistance, ohms
    double res  = 0.0;   // motional resistance at resonance (|Z|max - Re), ohms
    double f0   = 106.0; // in-cabinet impedance resonance, Hz
    double q    = 3.0;   // resonance (mechanical) Q of the impedance peak
    double zout = 3.7;   // amplifier output impedance at the speaker tap, ohms
};

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

    // Rev3: every stage is a solved 12AX7 (TriodeStage) with these parts. The
    // behavioural gain/bias/hardness above no longer shape the curve: they set
    // each stage's drive level (TriodeCalibration.h). All Rectifier voices
    // share the amp's one cascade (V1A -> gain pot -> V2A -> V2B -> V3A).
    std::array<triode::StageParts, maxStages> tubes { triodeparts::rectoV1A(), triodeparts::rectoV2A(),
                                                      triodeparts::rectoV2B(), triodeparts::rectoV3A() };
    std::array<float, maxStages> triodeDrive { 1.0f, 1.0f, 1.0f, 1.0f }; // per-stage drive trim (TriodeCalibration::mapStage)
    float triodeMakeup = 1.0f;  // Rev3 level trim on top of makeup (solved stages only)
    bool behavioural = false;   // tests/tools only: run the Rev2.4 static curves instead (A/B reference)

    int    gainPotAfterStage = 0;
    float  gainPotMin = 0.02f;
    float  gainPotMax = 1.0f;

    double inputTightHpHz = 60.0;
    double postLpHz = 7000.0;
    float  makeup = 0.6f;

    // Passive TMB parts (sourced schematic values) + level makeup
    TmbComponents tmb {};

    // Presence shelf corner (NFB-loop principle)
    double presenceHz = 2400.0;

    // Post-TMB recovery stage (RecoveryStage, Rev2.2): drive (0 = clean) and
    // operating-point bias of the asymmetric curve. Tuned with
    // juicyboi_harmonics so Red reaches at least Rev1's THD / harmonic count
    // at matched knobs (docs/diode-clippers.md, "Rev2.2 gain recovery").
    float recoveryDrive = 0.0f;
    float recoveryBias  = 0.40f;
    float recoveryOutGain = 1.07f; // +0.6 dB: undoes the stage's compression at default knobs

    // Power-amp / 4x12 load interaction after the recovery stage (Rev2.3).
    // Default = bypass; only Red v2 sets it (rectoSpeakerLoad()).
    SpeakerLoadParams speakerLoad {};
};

namespace voicings
{
/** Rectifier passive TMB component sets, read off the Mesa/Boogie Dual
    Rectifier preamp sheet "RF-1F" (6-93), Red and Orange stacks:
      Red:    TRBL 250K, C4 680pF (treble), R273 47K (slope), C23 .02uF (bass),
              C24 .02uF (mid), BASS 1M, MID 25K
      Orange: TRBL 250K, C5 500pF, R274 47K, C25 .02uF, C26 .02uF, BASS 1M, MID 25K
    The sheet prints the bass/mid caps as ".02" (20 nF); earlier juicyboi notes
    said 22n. Pot tapers are NOT printed on that sheet: Bass = audio and
    Treble/Mid = linear are assumptions (matching the tapers printed on the
    5150 sheet). The LDR8/LDR9 channel switching, R215 1M bridge and the 1M
    master load on the wiper are not modelled (ideal source, unloaded output).
    makeupDb only restores the Rev1 level at default knobs (Bass 5, Mid 4,
    Treble 5.5); it does not change the network. */
inline TmbComponents redTmb()
{
    return { 250e3, 1e6, 25e3, 47e3, 680e-12, 20e-9, 20e-9, 5.00 };
}

inline TmbComponents orangeTmb()
{
    return { 250e3, 1e6, 25e3, 47e3, 500e-12, 20e-9, 20e-9, 6.00 };
}

/** Rev2.3 Red v2 low end: tube power amp into a closed-back V30 4x12.
    Values (nominal; the amp's own output impedance was not measured):
      f0 = 106 Hz  impedance resonance of a Rectifier Oversized 4x12 with
                   Vintage 30s (Redwirez impedance sweep, Fractal Audio forum
                   thread "LF Resonant Frequency"; Recto 4x12s measure ~106-111 Hz)
      Re = 6.4 ohm, |Z|max ~31 ohm (Res = 25), Q = 3: typical 8-ohm V30 4x12
                   closed-box impedance peak (nominal, not a measured curve)
      Zo = 3.7 ohm closed-loop output impedance (damping factor ~2.2 at the
                   8-ohm tap) for Red VINTAGE, which has power-amp NFB: the
                   switching matrix turns LDR19 ON in "RD VINT" (OFF in "RD
                   MDRN"), closing FB -> .1uF -> R275 47K into R372 4.7K at the
                   phase-inverter tail (Dual Rectifier power-amp sheet).
                   Chosen as a moderate damping factor: +3.0 dB at f0.
    Result: +0.8 dB at 40 Hz, +2.6 dB at 80 Hz, +3.0 dB at 100-106 Hz,
    +1.5 dB at 200 Hz, +0.07 dB at 1 kHz. See docs/red-v2-bass.md. */
inline SpeakerLoadParams rectoSpeakerLoad()
{
    return { 6.4, 25.0, 106.0, 3.0, 3.7 };
}

/** Channel 1 — Red, voicing v1 (original juicyboi v1.x Red).
    Red TMB (680pF treble cap), tight cascade (input tight 95 Hz,
    steep interstage high-passes), moderate stage gain. Kept verbatim so the
    Red v1/v2 switch is a true A/B against the pre-ef9fd35 sound. */
inline ChannelVoicing redV1()
{
    ChannelVoicing v;
    v.numStages = 4;
    // Shared skeleton inspired by V1A → V2A → V2B (cold) → V3A (+ CF after)
    v.stages = {{
        //  HP     LP        gain   bias    pos    neg
        {   35.0, 13000.0,  2.8f,  0.14f,  1.6f,  0.9f }, // V1A-ish
        {  120.0, 10000.0,  7.5f,  0.28f,  1.9f,  1.0f }, // V2A-ish (after gain pot)
        {  220.0,  8500.0,  6.5f,  0.32f,  2.0f,  1.1f }, // V2B cold-cathode bite
        {   25.0,  7500.0,  2.2f,  0.08f,  1.3f,  1.0f }, // V3A → toward CF
    }};
    v.gainPotAfterStage = 0;
    v.gainPotMin = 0.025f;
    v.gainPotMax = 1.15f;
    v.inputTightHpHz = 95.0;
    v.postLpHz = 7800.0;
    v.makeup = 0.58f;
    v.triodeMakeup = 2.48f; // Rev3: +7.9 dB restores the Rev2.4 cascade level at Gain 7-10
    v.triodeDrive = { 1.087f, 0.592f, 1.681f, 2.82f }; // Rev3 drive staging (juicyboi_triode_tune red1 search)
    v.recoveryDrive = 2.0f; // Red: Rectifier FX-loop recovery / PI stand-in (Rev2.2)

    v.tmb = redTmb();
    v.presenceHz = 2400.0;
    return v;
}

/** Channel 1 — Red, voicing v2 (default): max-gain / fat-bottom Red.
    Same TMB set as v1. Builds on ef9fd35 (eased high-passes, more drive) and
    pushes the cascade further: ~12-17% more stage drive, a hotter Gain pot
    ceiling, harder grid-conduction knees and a touch more cold-cathode bias
    on V2A/V2B for denser saturation. To stay musical rather than fuzzy, the
    V2B coupling HP is nudged *up* (palm-mute tightness) and the plate / post
    low-passes come down a little to keep the extra drive from fizzing.
    Rev2.3: the cascade is unchanged. The real amp's V2A->V2B coupling is
    5.4 Hz (C22 .02u, R242 470K + R212 1M) with an unbypassed V2B cathode, but
    restoring it here adds only +0.6 dB of output bass and costs palm-mute
    tightness (no blocking dynamics in this static model). The 150 Hz corner
    is therefore a deliberate deviation; see docs/red-v2-bass.md. The extra low
    end comes from speakerLoad, applied after the recovery stage. */
inline ChannelVoicing redV2()
{
    ChannelVoicing v;
    v.numStages = 4;
    v.stages = {{
        //  HP     LP        gain    bias    pos    neg
        {   30.0, 13000.0,  3.8f,  0.14f,  1.6f,  0.9f }, // V1A-ish
        {   80.0,  9500.0, 10.5f,  0.30f,  2.1f,  1.0f }, // V2A-ish (after gain pot)
        {  150.0,  7800.0,  8.6f,  0.35f,  2.2f,  1.1f }, // V2B cold-cathode bite (tight)
        {   22.0,  7200.0,  2.6f,  0.08f,  1.3f,  1.0f }, // V3A → toward CF
    }};
    v.gainPotAfterStage = 0;
    v.gainPotMin = 0.025f;
    v.gainPotMax = 1.6f;
    v.inputTightHpHz = 65.0;
    v.postLpHz = 7400.0;
    v.makeup = 0.49f; // ~-1.5 dB vs v1: rough level-match so the A/B is about tone, not loudness
    v.triodeMakeup = 2.0f;  // Rev3: +6 dB restores the Rev2.4 cascade level at Gain 7-10
    v.triodeDrive = { 2.82f, 0.42f, 1.54f, 2.0f }; // Rev3 drive staging (juicyboi_triode_tune red2 search)
    v.recoveryDrive = 2.0f; // Red: Rectifier FX-loop recovery / PI stand-in (Rev2.2)
    v.speakerLoad = rectoSpeakerLoad(); // Rev2.3: power-amp / 4x12 low-end interaction

    v.tmb = redTmb();
    v.presenceHz = 2400.0;
    return v;
}

/** Channel 1 — Red (default voicing = v2). */
inline ChannelVoicing red() { return redV2(); }

/** Channel 2 — Orange: vintage / looser Dual Rec–style path.
    Softer TMB (500pF treble cap), fatter low-mids, slightly less gain. */
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
    v.triodeMakeup = 1.44f; // Rev3: +3.2 dB restores the Rev2.4 cascade level at Gain 7-10
    v.triodeDrive = { 1.09f, 0.42f, 2.18f, 0.844f }; // Rev3 drive staging (juicyboi_triode_tune orange search)

    v.tmb = orangeTmb();
    v.presenceHz = 2200.0;
    return v;
}

// Compatibility aliases used by the processor (A = ch1 Red, B = ch2 Orange)
inline ChannelVoicing riffA() { return red(); }
inline ChannelVoicing riffAv1() { return redV1(); }
inline ChannelVoicing riffB() { return orange(); }
} // namespace voicings
} // namespace jb::dsp
