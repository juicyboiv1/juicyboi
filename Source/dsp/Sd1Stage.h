#pragma once

#include "DiodeClipper.h"
#include "Filters.h"
#include "PedalTone.h"

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
      Tone 5k linear [mod], R8=470 + C5 wiper to ground, R9=1k feedback [mod]
      C7=22uF [mod], R10 jumper [mod], Level 100kA [mod], JFET bias 1M
      R14=1k [mod] + C8=22uF [mod] into the 2SC732 follower (1M bias, 10k emitter)
      R16=1k, C10=22uF [mod], 100k pulldown

    The op-amp clipping stage is SOLVED per sample (see OpAmpDiodeClipper).
    Rev3.1: everything after it is the exact linear circuit (PedalPostStage).
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

    static PedalToneComponents toneComponents() noexcept
    {
        PedalToneComponents t;
        t.rPost = 1.0e3;  t.cPost = 0.15e-6;         // R7, C4 [mod]
        t.rBias = 0.0;                               // none on the SD-1
        t.rPot = 5.0e3;   t.taper = PedalToneComponents::Taper::linear; // Tone 5k linear [mod]
        t.rWiper = 470.0; t.cWiper = 0.15e-6;        // R8 (stock), C5 [mod]
        t.rFb = 1.0e3;                               // R9 [mod]; C6 removed [mod]
        return t;
    }

    static PedalOutputComponents outputComponents() noexcept
    {
        PedalOutputComponents o;
        o.rSeries = 0.0;    o.cCoupling = 22e-6;     // R10 jumper, C7 [mod]
        o.rLevel = 100e3;   o.rDrain = 1.0e6;        // Level 100kA [mod], JFET bias 1M
        o.rSeries2 = 1.0e3; o.cBase = 22e-6;         // R14, C8 [mod]
        o.rBase = 1.0e6;    o.rEmitter = 10e3;       // base bias 1M, emitter 10k
        o.rOut = 1.0e3;     o.cOut = 22e-6;  o.rPulldown = 100e3; // R16, C10 [mod], 100k
        return o;
    }

    void prepare (double sampleRate);
    void reset();

    void setParameters (float drive01, float tone01, float levelGain);
    void snap();

    inline void beginFrame() noexcept
    {
        clipper.beginFrame();
        post.beginFrame();
    }

    inline float processClipper (float x, int ch) noexcept
    {
        return (float) clipper.process ((double) inputHP.process (x, ch), ch);
    }

    inline float process (float x, int ch) noexcept
    {
        return post.process (processClipper (x, ch), ch);
    }

    const OpAmpDiodeClipper& getClipper() const noexcept { return clipper; }
    PedalPostStage& getPost() noexcept { return post; }

    /** Fixed output makeup (not part of the circuit). See docs/pedal-tone.md. */
    static constexpr double makeup = 3.8637; // +11.74 dB

private:
    double fs = 44100.0;

    OnePole inputHP;
    OpAmpDiodeClipper clipper;
    PedalPostStage post;

    static constexpr double inputHpHz = 1.0 / (2.0 * kPi * 100e3 * 0.055e-6); // C2/R4
};
} // namespace jb::dsp
