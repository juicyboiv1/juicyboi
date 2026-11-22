#pragma once

#include "DiodeClipper.h"
#include "Filters.h"

namespace jb::dsp
{
/** OD-808 (TS-808-topology) Boost front end — Rev2.2 component-level clipper.

    Values from the TS-808 schematic (Steve Cerutti drawing) cross-checked
    with the ElectroSmash TS808 analysis/BOM; see docs/diode-clippers.md.

      C2=1uF + R5=10k          input coupling / bias HP (15.9 Hz)
      R4=4.7k + C3=0.047uF     gain leg to Vr (720 Hz)
      R6=51k + Drive 500kA     feedback resistance (audio taper)
      C4=51pF                  across the feedback network
      D1/D2=1N914              antiparallel pair across the feedback network
      R7=1k + C5=0.22uF        post-clip LP (723 Hz) into the tone stage

    The op-amp clipping stage is SOLVED per sample (Newton-Raphson on the
    Shockley diode equation with trapezoidal capacitor companions, see
    OpAmpDiodeClipper). The tone/level stages after it are unchanged from
    Rev2.1 (behavioural Tone blend, Level gain), now smoothed per sample.
    Original modelling of the published topology, not a product clone. */
class Od808Stage
{
public:
    static ClipperComponents clipperComponents() noexcept
    {
        ClipperComponents c;
        c.rLeg = 4.7e3;  c.cLeg = 0.047e-6;          // R4, C3
        c.rFixed = 51e3; c.rDrivePot = 500e3;        // R6, Drive 500kA
        c.cFeedback = 51e-12;                        // C4
        c.diodesPos = 1; c.diodesNeg = 1;            // D1/D2 1N914
        c.is = 2.52e-9;  c.n = 1.752;                // 1N914 SPICE model
        return c;
    }

    void prepare (double sampleRate);
    void reset();

    /** drive01 / tone01 in 0..1, levelGain is linear output gain (targets; smoothed). */
    void setParameters (float drive01, float tone01, float levelGain);
    /** Jump all smoothed parameters to their targets (prepare / offline). */
    void snap();

    /** Once per sample frame, before process() for each channel. */
    inline void beginFrame() noexcept
    {
        clipper.beginFrame();
        toneTreble += smooth * (toneTarget - toneTreble);
        level      += smooth * (levelTarget - level);
    }

    /** Input coupling + solved clipping stage only (op-amp output, volts). */
    inline float processClipper (float x, int ch) noexcept
    {
        return (float) clipper.process ((double) inputHP.process (x, ch), ch);
    }

    inline float process (float x, int ch) noexcept
    {
        const auto clipped = processClipper (x, ch);

        // R7/C5 post LP, Tone restores HF above it (20kA behaviour, Rev2.1)
        const auto lp = toneLP.process (clipped, ch);
        const auto toned = lp + toneTreble * (clipped - lp);

        return outputHP.process (toned * level * makeup, ch);
    }

    const OpAmpDiodeClipper& getClipper() const noexcept { return clipper; }

    /** Fixed output trim (not part of the circuit): keeps Rev2.2's default
        Boost level close to Rev2.1. See docs/diode-clippers.md. */
    static constexpr float makeup = 1.0f;

private:
    double fs = 44100.0;

    OnePole inputHP, toneLP, outputHP;
    OpAmpDiodeClipper clipper;

    float toneTreble = 0.5f, toneTarget = 0.5f;
    float level = 1.0f, levelTarget = 1.0f;
    float smooth = 1.0f;

    static constexpr double inputHpHz   = 1.0 / (2.0 * kPi * 10e3 * 1e-6);   // C2/R5
    static constexpr double rToneSeries = 1.0e3;                             // R7
    static constexpr double cTone       = 0.22e-6;                           // C5
    static constexpr double outputHpHz  = 15.9;                              // Rev2.1 (not re-sourced)
};
} // namespace jb::dsp
