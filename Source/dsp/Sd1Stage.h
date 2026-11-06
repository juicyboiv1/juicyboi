#pragma once

#include "Filters.h"
#include "SoftClip.h"

namespace jb::dsp
{
/** SD-1–family Boost front end (Boutique Overdrive Mod RC cues).

    Behavioral model inspired by the publicly documented "Boutique Overdrive"
    RC set for SD-1–topology pedals (fatter coupling, 500kA drive range,
    retuned mid clip/LP, C6 removed for an open top). Original DSP of those
    topology principles — not a product clone. No commercial branding.

    BOM → model map (Boutique OD cues):
      R6=1k + C3=0.22u     gain-leg RC (~723 Hz) — mids get drive
      R5=10k + OD pot 500kA feedback / drive range (audio taper)
      D4/D5/D6             asymmetric 3-diode clip (1 vs 2 silicon)
      R7=1k + C4=0.15u     post-clip LP (~1.06 kHz)
      C5=0.15u + Tone 5k   passive tone LP + treble restore
      C6                   omitted (open top — no recovery HF clamp)
      Level 100kA          output level
      C1=1u / C2=.055u     fatter input coupling HPs

    Topology: out ≈ in + asym_clip(gain · hp_leg(in)), then post LP, tone, level.
    Voiced slightly hot for surplus thickness into Dual Rec–style high gain. */
class Sd1Stage
{
public:
    void prepare (double sampleRate);
    void reset();

    /** drive01 / tone01 in 0..1, levelGain is linear output gain. */
    void setParameters (float drive01, float tone01, float levelGain);

    inline float process (float x, int ch) noexcept
    {
        const auto in = inputHP.process (x, ch);

        auto amp = driveGain * gainLegHP.process (in, ch);

        // Asymmetric 3-diode clip on the amplified leg (Boutique keeps stock diodes)
        amp = diodeVf * shape::diodeAsym3 (amp / diodeVf, 1.0f, 2.0f, diodeKnee);
        const auto clipped = in + amp;

        // Post-clip LP (R7/C4), then tone LP with treble restore (C6 omitted → open top)
        const auto filtered = postLP.process (clipped, ch);
        const auto lp = toneLP.process (filtered, ch);
        const auto toned = lp + toneTreble * (filtered - lp);

        return outputHP.process (toned * level, ch);
    }

private:
    double fs = 44100.0;

    OnePole inputHP, gainLegHP, postLP, toneLP, outputHP;

    float driveGain = 20.0f;
    float toneTreble = 0.5f;
    float level = 1.0f;

    static constexpr float diodeVf = 0.60f;
    static constexpr float diodeKnee = 2.6f;

    // Boutique Overdrive Mod RC cues
    static constexpr double rLeg        = 1.0e3;    // R6
    static constexpr double cLeg        = 0.22e-6;  // C3
    static constexpr double rFixed      = 10e3;     // R5
    static constexpr double rDrivePot   = 500e3;    // Gain 500k audio
    static constexpr double rPost       = 1.0e3;    // R7
    static constexpr double cPost       = 0.15e-6;  // C4
    static constexpr double rToneSeries = 1.0e3;    // into tone node
    static constexpr double cTone       = 0.15e-6;  // C5
    // Fatter coupling than stock / OD-808 (C1=1u, C2=.055u class)
    static constexpr double inputHpHz   = 8.0;
    static constexpr double outputHpHz  = 8.0;
    // Slight makeup so A/B with OD-808 at matched Level feels thick into Dual Rec
    static constexpr float  thicknessGain = 1.18f;

    float lastDrive = -1.0f;
};
} // namespace jb::dsp
