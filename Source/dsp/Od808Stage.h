#pragma once

#include "Filters.h"
#include "SoftClip.h"

namespace jb::dsp
{
/** OD808-family Boost front end.

    Component cues taken from the Maxon OD-808 rev.1 stripboard BOM
    (Harald Sabro / Matsumin layout) the user provided — original DSP
    implementation of those topology principles, not a product clone.

    BOM → model map:
      R12=4.7k + C2=50n     gain-leg RC (~677 Hz) — mids get drive, lows ~unity
      R11=51k + OD pot 500kA  feedback network (audio-taper drive)
      C1=50p                feedback HF roll-off (darkens as drive rises)
      D1/D2=1N914           feedback silicon diode pair
      C10/C11=220n + Tone 20kA  passive tone LP (~723 Hz w/ ~1k) + treble restore
      Balance 100k lin      output level
      C3=1uF NP etc.        input/output coupling HPs

    Topology: out ≈ in + clip(gain · hp_leg(in)), then tone LP blend, then level. */
class Od808Stage
{
public:
    void prepare (double sampleRate);
    void reset();

    /** drive01 / tone01 in 0..1, levelGain is linear output gain. */
    void setParameters (float drive01, float tone01, float levelGain);

    inline float process (float x, int ch) noexcept
    {
        const auto in = inputHP.process (x, ch);

        auto amp = feedbackLP.process (driveGain * gainLegHP.process (in, ch), ch);

        // 1N914 pair in the feedback loop — limit only the amplified leg
        amp = diodeVf * shape::diodePair (amp / diodeVf, diodeKnee);
        const auto clipped = in + amp;

        // Tone: dark LP, Tone pot restores HF above it (20kA behaviour)
        const auto lp = toneLP.process (clipped, ch);
        const auto toned = lp + toneTreble * (clipped - lp);

        return outputHP.process (toned * level, ch);
    }

private:
    double fs = 44100.0;

    OnePole inputHP, gainLegHP, feedbackLP, toneLP, outputHP;

    float driveGain = 12.0f;
    float toneTreble = 0.5f;
    float level = 1.0f;

    // 1N914-ish forward drop / soft knee
    static constexpr float diodeVf = 0.60f;
    static constexpr float diodeKnee = 2.8f;

    // OD-808 rev.1 BOM values
    static constexpr double rLeg       = 4.7e3;    // R12
    static constexpr double cLeg       = 50e-9;    // C2 (rev.1: on bias network)
    static constexpr double rFixed     = 51e3;     // R11
    static constexpr double rDrivePot  = 500e3;    // Overdrive 500k audio
    static constexpr double cFeedback  = 50e-12;   // C1
    static constexpr double rToneSeries = 1.0e3;   // series R into tone node
    static constexpr double cTone      = 220e-9;   // C10/C11
    static constexpr double inputHpHz  = 15.9;     // ~1/(2π·10k·1u) coupling
    static constexpr double outputHpHz = 15.9;

    float lastDrive = -1.0f;
};
} // namespace jb::dsp
