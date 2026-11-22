#pragma once

#include "DiodeClipper.h"
#include "Filters.h"

namespace jb::dsp
{
/** SD-1–topology Boost front end — Rev2.2 component-level clipper.

    Stock SD-1 circuit (schematicheaven drawing, reference designators as
    used by the Wampler DIY write-up) with the publicly documented
    "Boutique Overdrive" mod values that juicyboi has always used for this
    voicing (Wampler DIY, "A few easy Boss SD-1 Super Overdrive Mods").
    See docs/diode-clippers.md for both columns (stock / mod).

      C2=.055uF + R4=100k      input coupling / bias HP (28.9 Hz; R4 stock)
      R6=1k + C3=0.22uF        gain leg to Vr (723 Hz)          [mod]
      R5=10k + Gain 500kA      feedback resistance (audio taper) [mod]
      (no feedback capacitor — none fitted on the SD-1 clipper)
      D4/D5/D6 1N916-class     asymmetric: 2 in series for out > in, 1 for out < in
      R7=1k + C4=0.15uF        post-clip LP (1.06 kHz)          [mod]
      C5=0.15uF                tone LP cap                       [mod]
      C6                       removed                           [mod]

    The op-amp clipping stage is SOLVED per sample (see OpAmpDiodeClipper);
    tone/level after it are the Rev2.1 behavioural stages, smoothed per sample.
    Original modelling of the published topology, not a product clone. */
class Sd1Stage
{
public:
    static ClipperComponents clipperComponents() noexcept
    {
        ClipperComponents c;
        c.rLeg = 1.0e3;  c.cLeg = 0.22e-6;           // R6, C3 (Boutique mod)
        c.rFixed = 10e3; c.rDrivePot = 500e3;        // R5, Gain 500kA (Boutique mod)
        c.cFeedback = 0.0;                           // not fitted
        c.diodesPos = 2; c.diodesNeg = 1;            // D4-D6 asymmetric
        c.is = 2.52e-9;  c.n = 1.752;                // 1N914-family model (assumption for 1N916/1S2473)
        return c;
    }

    void prepare (double sampleRate);
    void reset();

    void setParameters (float drive01, float tone01, float levelGain);
    void snap();

    inline void beginFrame() noexcept
    {
        clipper.beginFrame();
        toneTreble += smooth * (toneTarget - toneTreble);
        level      += smooth * (levelTarget - level);
    }

    inline float processClipper (float x, int ch) noexcept
    {
        return (float) clipper.process ((double) inputHP.process (x, ch), ch);
    }

    inline float process (float x, int ch) noexcept
    {
        const auto clipped = processClipper (x, ch);

        // Post-clip LP (R7/C4), then tone LP with treble restore (C6 removed)
        const auto filtered = postLP.process (clipped, ch);
        const auto lp = toneLP.process (filtered, ch);
        const auto toned = lp + toneTreble * (filtered - lp);

        return outputHP.process (toned * level * makeup, ch);
    }

    const OpAmpDiodeClipper& getClipper() const noexcept { return clipper; }

    /** Fixed output trim (not part of the circuit). See docs/diode-clippers.md. */
    static constexpr float makeup = 1.0f;

private:
    double fs = 44100.0;

    OnePole inputHP, postLP, toneLP, outputHP;
    OpAmpDiodeClipper clipper;

    float toneTreble = 0.5f, toneTarget = 0.5f;
    float level = 1.0f, levelTarget = 1.0f;
    float smooth = 1.0f;

    static constexpr double inputHpHz   = 1.0 / (2.0 * kPi * 100e3 * 0.055e-6); // C2/R4
    static constexpr double rPost       = 1.0e3;    // R7
    static constexpr double cPost       = 0.15e-6;  // C4
    static constexpr double rToneSeries = 1.0e3;    // Rev2.1 tone model
    static constexpr double cTone       = 0.15e-6;  // C5
    static constexpr double outputHpHz  = 8.0;      // Rev2.1 (not re-sourced)
};
} // namespace jb::dsp
