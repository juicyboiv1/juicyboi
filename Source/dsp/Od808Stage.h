#pragma once

#include "Filters.h"
#include "SoftClip.h"

namespace jb::dsp
{
/** OD-style front end, built from the general topology principles of the
    classic op-amp "feedback diode" green-overdrive family (circuit-principle
    inspired; component values are generic published starting points, not a
    product copy).

    Topology principles modelled:
      1. Input coupling HP (keeps sub-bass/DC out).
      2. Non-inverting op-amp gain stage whose ground leg is a series RC
         (~4.7k + ~47nF, ~720 Hz corner): only content ABOVE that corner gets
         the full drive gain; lows pass at ~unity. This is the source of the
         "tight low end / mid hump" into a high-gain amp.
      3. A small feedback cap in parallel with the drive network: rolls off
         the amplified path at HF; the corner drops as drive goes up.
      4. Diodes in the op-amp feedback loop: the AMPLIFIED part is limited,
         while the dry input still passes -> out = in + clip(gain * hp(in)).
         Softer and less compressed than a shunt-to-ground hard clipper.
      5. Passive ~720 Hz LP followed by an active treble control (modelled as
         a variable high-shelf blend around that LP).
      6. Output level + output coupling HP.

    All coefficient choices below are STARTING POINTS for listening/measurement
    based tuning — see README "next tuning steps". */
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

        // (2)+(3): amplified, band-limited path
        auto amp = feedbackLP.process (driveGain * gainLegHP.process (in, ch), ch);

        // (4): feedback diodes limit only the amplified component
        amp = diodeVf * shape::diodePair (amp / diodeVf, diodeKnee);
        const auto clipped = in + amp;

        // (5): tone — LP around ~720 Hz, tone knob restores treble above it
        const auto lp = toneLP.process (clipped, ch);
        const auto toned = lp + toneTreble * (clipped - lp);

        // (6)
        return outputHP.process (toned * level, ch);
    }

private:
    double fs = 44100.0;

    OnePole inputHP, gainLegHP, feedbackLP, toneLP, outputHP;

    float driveGain = 12.0f;
    float toneTreble = 0.5f;
    float level = 1.0f;

    // Signal is treated as ~1.0 FS ≈ 1 V for scaling purposes. Silicon
    // diode forward voltage ~0.5–0.6 V. Knee 2.5 ≈ soft silicon pair.
    static constexpr float diodeVf = 0.55f;
    static constexpr float diodeKnee = 2.5f;

    // Generic family values (starting points)
    static constexpr double rLeg       = 4.7e3;   // gain-leg resistor
    static constexpr double cLeg       = 0.047e-6;// gain-leg cap  -> ~720 Hz
    static constexpr double rFixed     = 51e3;    // fixed feedback R
    static constexpr double rDrivePot  = 500e3;   // drive pot
    static constexpr double cFeedback  = 51e-12;  // feedback cap (HF rolloff)
    static constexpr double toneLpHz   = 723.0;   // ~1k / 0.22uF passive LP
    static constexpr double inputHpHz  = 20.0;
    static constexpr double outputHpHz = 15.0;

    float lastDrive = -1.0f;
};
} // namespace jb::dsp
