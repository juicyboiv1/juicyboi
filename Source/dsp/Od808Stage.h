#pragma once

#include "DiodeClipper.h"
#include "Filters.h"
#include "PedalTone.h"

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
      R9=10k                   tone op-amp (+) bias to Vr
      Tone 20k "G" (W taper)   strung from (+) to (-) of the tone op-amp
      R8=220 + C6=0.22uF       wiper to ground
      R10=1k                   tone op-amp feedback
      R11=1k, C7=1uF           into the Level pot (100k)
      C8=0.1uF, R13=510k       into the 2SC1815 emitter follower (R14=10k)
      R15=100, C9=10uF, R16=10k output network

    The op-amp clipping stage is SOLVED per sample (Newton-Raphson on the
    Shockley diode equation with trapezoidal capacitor companions, see
    OpAmpDiodeClipper). Rev3.1: everything after it (post LP, active tone stage,
    Level pot, output buffer) is the exact linear circuit (PedalPostStage).
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

    static PedalToneComponents toneComponents() noexcept
    {
        PedalToneComponents t;
        t.rPost = 1.0e3;  t.cPost = 0.22e-6;         // R7, C5
        t.rBias = 10.0e3;                            // R9
        t.rPot = 20.0e3;  t.taper = PedalToneComponents::Taper::wTaper; // Tone 20k "G"
        t.rWiper = 220.0; t.cWiper = 0.22e-6;        // R8, C6
        t.rFb = 1.0e3;                               // R10
        return t;
    }

    static PedalOutputComponents outputComponents() noexcept
    {
        PedalOutputComponents o;
        o.rSeries = 1.0e3;  o.cCoupling = 1.0e-6;    // R11, C7
        o.rLevel = 100e3;   o.rDrain = 510e3;        // Level 100k, JFET bias 510k
        o.rSeries2 = 0.0;   o.cBase = 0.1e-6;        // C8
        o.rBase = 510e3;    o.rEmitter = 10e3;       // R13, R14
        o.rOut = 100.0;     o.cOut = 10e-6;  o.rPulldown = 10e3; // R15, C9, R16
        return o;
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
        post.beginFrame();
    }

    /** Input coupling + solved clipping stage only (op-amp output, volts). */
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

    /** Fixed output makeup (not part of the circuit): the top 12 dB of the Level
        knob plus the circuit's in-band loss, set so the default Boost matches
        Rev3's level. See docs/pedal-tone.md. */
    static constexpr double makeup = 4.7643; // +13.56 dB

private:
    double fs = 44100.0;

    OnePole inputHP;
    OpAmpDiodeClipper clipper;
    PedalPostStage post;

    static constexpr double inputHpHz = 1.0 / (2.0 * kPi * 10e3 * 1e-6);   // C2/R5
};
} // namespace jb::dsp
