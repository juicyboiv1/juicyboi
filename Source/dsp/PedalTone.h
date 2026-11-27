#pragma once

// Rev3.1 — component-level post-clip network of the OD-808 / SD-1 Boosts:
// passive post-clip LP, the active tone stage (op-amp with the Tone pot strung
// between its inputs), the Level pot and the emitter-follower output buffer.
// Every stage here is linear, so each one is solved exactly as an s-domain
// transfer function of its parts (derived below) and discretised with the
// bilinear transform (= trapezoidal integration, SPICE's "trap"). Pot positions
// are smoothed per sample (20 ms) and the coefficients are recomputed every
// sample while a pot moves, so knob moves are zipper-free.
//
// 1) Post-clip LP + active tone stage (ideal op-amp: v- = v+, no input current)
//
//   clip out -- Rpost --+-- A ------------------(+)|
//                       |   |                       >---+-- Y
//                     Cpost Rbias (0 = absent)  +-(-)/  |
//                       |   |                   |       |
//                      Vr  Vr        A --[ rA | rN ]-- N --- Rf ---+
//                                         wiper W
//                                           |
//                                          Rw
//                                           |
//                                          Cw -- Vr
//
//   rA = tau * Rpot (A..W), rN = (1 - tau) * Rpot (W..N), tau = Tone pot
//   position (0 = wiper at the + end = darkest). With g0 = 1/Rpost + 1/Rbias and
//   P = rA rN + Rw Rpot, nodal analysis gives the exact biquad
//
//            (1/Rpost) [ Rpot + s Cw (P + rA Rf) ]
//   H(s) = -----------------------------------------------------------------
//          g0 Rpot + s (g0 Cw P + Cpost Rpot + Cw rN) + s^2 Cpost Cw P
//
//   (tau = 1 factors to (1 + s Cw (Rw + Rf)) / ((g0 Rpost) (1 + s Cpost/g0)
//   (1 + s Cw Rw)): the LP with a treble shelf; tau = 0 is a 2nd-order LP.)
//
// 2) Level pot and output buffer (op-amp output is an ideal source)
//
//   Y -- Rs -- C1 --+ (Level top)                  emitter follower
//                   Ru = (1 - v) Rlevel            (gain Af, input Rin)
//                   +-- wiper -- Rs2 -- C2 --+-- base ==> E -- Ro -- C3 --+-- out
//                   Rl = v Rlevel  Rdrain    Rbase || beta Re_ac         Rpd || Rload
//                   |              |
//                  Vr             Vr
//
//   With Gl = 1/Rl + 1/Rdrain, R1t = Rs + Ru, Rb2 = Rs2 + Rin:
//     Vbase/Y = s^2 C1 C2 Rin / ( Gl + s [C1 + C2 + Gl C2 Rb2 + Gl C1 R1t]
//                                   + s^2 [C1 C2 Rb2 + C1 R1t (Gl C2 Rb2 + C2)] )
//     Vout/E  = s C3 RL' / (1 + s C3 (Ro + RL')),  RL' = Rpd || Rload
//   The follower is a small-signal unity buffer: Af = Re_ac / (Re_ac + re),
//   re = Vt / Ie, Ie = (Vr - Vbe) / Re; Rin = Rbase || beta (Re || (Ro + RL')).
//
// The Level knob keeps juicyboi's dB law (-24..+12 dB). A real Level pot can only
// attenuate, so the knob sets the wiper to the unloaded divider ratio
// v = 10^((dB - 12) / 20) (an ideal log taper spanning the knob's 36 dB) and a
// fixed output makeup (not part of the circuit) restores the top 12 dB plus the
// pedal's own in-band loss. Loading by the buffer is modelled, so the law bends by
// a fraction of a dB versus the pure dB law. See docs/pedal-tone.md.

#include "Filters.h"

#include <array>
#include <cmath>
#include <algorithm>

namespace jb::dsp
{
struct PedalToneComponents
{
    double rPost  = 1.0e3;    // post-clip series R (R7)
    double cPost  = 0.22e-6;  // post-clip shunt C (C5 / C4)
    double rBias  = 10.0e3;   // + input bias to Vr (R9 on the TS-808; 0 = not fitted)
    double rPot   = 20.0e3;   // Tone pot
    double rWiper = 220.0;    // wiper series R (R8)
    double cWiper = 0.22e-6;  // wiper series C (C6 / C5)
    double rFb    = 1.0e3;    // tone op-amp feedback R (R10 / R9)
    enum class Taper { linear, wTaper } taper = Taper::linear;
};

struct PedalOutputComponents
{
    double rSeries   = 1.0e3;   // tone op-amp out -> coupling cap (R11; 0 = jumper)
    double cCoupling = 1.0e-6;  // C7
    double rLevel    = 100e3;   // Level pot
    double rDrain    = 510e3;   // bypass-JFET bias resistor at the wiper (to Vr)
    double rSeries2  = 0.0;     // wiper -> base cap series R (SD-1 R14; 0 = none)
    double cBase     = 0.1e-6;  // C8
    double rBase     = 510e3;   // output buffer base bias (R13)
    double beta      = 300.0;   // 2SC1815 / 2SC732 typical hFE
    double rEmitter  = 10e3;    // R14
    double vr        = 4.5;     // bias reference
    double vbe       = 0.65;
    double rOut      = 100.0;   // R15
    double cOut      = 10e-6;   // C9
    double rPulldown = 10e3;    // R16
    double rLoad     = 1.0e6;   // amp input
};

/** Exact s-domain biquad H(s) = (n0 + n1 s + n2 s^2) / (d0 + d1 s + d2 s^2), discretised
    with trapezoidal integration (= bilinear transform, K = 2 fs, no prewarp).
    Second-order sections run as a trapezoidal state-variable filter (Simper's
    TPT SVF: the states are the two integrator "capacitor" currents), which stays
    well-behaved while the pot-dependent coefficients change every sample; a
    first-order section (n2 = d2 = 0) runs as a TPT one-pole. Both are exactly
    the bilinear transform of H(s) for fixed coefficients. */
class SBiquad
{
public:
    inline void set (double n0, double n1, double n2, double d0, double d1, double d2, double K) noexcept
    {
        an0 = n0; an1 = n1; an2 = n2; ad0 = d0; ad1 = d1; ad2 = d2; kk = K;
        firstOrder = ! (std::abs (d2) > 0.0);
        if (firstOrder)
        {
            // (n0 + n1 s) / (d0 + d1 s) = n1/d1 + (n0 - n1 d0/d1) / d1 * 1/(s + w),  w = d0/d1
            const auto w = d0 / d1;
            g = w / K;
            G1 = g / (1.0 + g);
            cHp = n1 / d1;                               // direct (high-pass) part
            cLp = (n0 / d1 - cHp * w) / w;               // times the unity-DC LP w/(s+w)
            return;
        }
        const auto w2 = d0 / d2, w = std::sqrt (w2);
        g = w / K;
        k = d1 / (d2 * w);                               // 1/Q
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
        mHp = n2 / d2;
        mBp = n1 / (d2 * w);
        mLp = n0 / (d2 * w2);
    }

    void reset() noexcept { ic1.fill (0.0); ic2.fill (0.0); }

    inline double process (double x, int ch) noexcept
    {
        auto& s1 = ic1[(size_t) ch];
        if (firstOrder)
        {
            const auto v = G1 * (x - s1);
            const auto lp = v + s1;
            s1 = lp + v;
            return cHp * x + cLp * lp;
        }
        auto& s2 = ic2[(size_t) ch];
        const auto v3 = x - s2;
        const auto v1 = a1 * s1 + a2 * v3;
        const auto v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0 * v1 - s1;
        s2 = 2.0 * v2 - s2;
        const auto hp = x - k * v1 - v2;
        return mHp * hp + mBp * v1 + mLp * v2;
    }

    /** Digital response at f (Hz) for sample rate fs: H(s) at s = K (1 - z^-1) / (1 + z^-1). */
    template <typename Complex>
    Complex response (double f, double fs) const
    {
        const auto w = 2.0 * kPi * f / fs;
        const Complex zi (std::cos (w), -std::sin (w));
        const Complex s = kk * (1.0 - zi) / (1.0 + zi);
        return (an0 + an1 * s + an2 * s * s) / (ad0 + ad1 * s + ad2 * s * s);
    }

private:
    double an0 = 1, an1 = 0, an2 = 0, ad0 = 1, ad1 = 0, ad2 = 0, kk = 1;
    bool firstOrder = true;
    double g = 0, G1 = 0, cHp = 0, cLp = 1, k = 1, a1 = 1, a2 = 0, a3 = 0, mHp = 0, mBp = 0, mLp = 1;
    std::array<double, kMaxChannels> ic1 {}, ic2 {};
};

class PedalPostStage
{
public:
    void prepare (double sampleRate, const PedalToneComponents& t, const PedalOutputComponents& o, double makeupGain)
    {
        fs = sampleRate; K = 2.0 * fs; tc = t; oc = o; makeup = makeupGain;
        smooth = 1.0 - std::exp (-1.0 / (0.020 * fs));

        // Output buffer small-signal constants (frequency independent)
        const auto rlp = 1.0 / (1.0 / oc.rPulldown + 1.0 / oc.rLoad);
        const auto reAc = 1.0 / (1.0 / oc.rEmitter + 1.0 / (oc.rOut + rlp));
        const auto ie = (oc.vr - oc.vbe) / oc.rEmitter;
        const auto re = 0.025851 / ie;
        af = reAc / (reAc + re);
        rin = 1.0 / (1.0 / oc.rBase + 1.0 / (oc.beta * reAc));
        // Output coupling C3 into Rpd || Rload (fixed)
        outHP.set (0.0, oc.cOut * rlp, 0.0, 1.0, oc.cOut * (oc.rOut + rlp), 0.0, K);
        updateTone (tau);
        updateLevel (wiper);
        reset();
    }

    void reset() noexcept { tone.reset(); level.reset(); outHP.reset(); }

    /** tone01 knob 0..1; levelGain = juicyboi Level knob as linear gain (-24..+12 dB). */
    void setTargets (float tone01, float levelGain) noexcept
    {
        tauTarget = toneTaper ((double) std::clamp (tone01, 0.0f, 1.0f));
        wiperTarget = std::clamp ((double) levelGain / kLevelTop, 1.0e-5, 1.0);
    }

    void snap() noexcept { tau = tauTarget; wiper = wiperTarget; updateTone (tau); updateLevel (wiper); }

    inline void beginFrame() noexcept
    {
        if (std::abs (tauTarget - tau) > 1.0e-7)
        {
            tau += smooth * (tauTarget - tau);
            updateTone (tau);
        }
        if (std::abs (wiperTarget - wiper) > 1.0e-7 * wiperTarget)
        {
            wiper += smooth * (wiperTarget - wiper);
            updateLevel (wiper);
        }
    }

    inline float process (float x, int ch) noexcept
    {
        const auto y = tone.process ((double) x, ch);
        const auto b = level.process (y, ch);
        return (float) (makeup * outHP.process (af * b, ch));
    }

    /** Tone-stage-only output (Rpost..tone op-amp output), for validation. */
    inline double processToneOnly (double x, int ch) noexcept { return tone.process (x, ch); }

    double toneTaper (double x) const noexcept
    {
        if (tc.taper == PedalToneComponents::Taper::linear) return x;
        // W / "G" taper: two mirrored audio halves (15 % at 25 % rotation)
        const auto half = [] (double u) { return (std::pow (10.0, 1.5 * u) - 1.0) / (std::pow (10.0, 1.5) - 1.0); };
        return x < 0.5 ? 0.5 * half (2.0 * x) : 1.0 - 0.5 * half (2.0 * (1.0 - x));
    }

    /** Digital response of the current (snapped) coefficients: tone stage only
        (clip out -> tone op-amp out) or the whole post network incl. makeup. */
    template <typename Complex>
    Complex digitalTone (double f) const { return tone.template response<Complex> (f, fs); }
    template <typename Complex>
    Complex digitalPost (double f, bool withMakeup = true) const
    {
        return tone.template response<Complex> (f, fs) * level.template response<Complex> (f, fs)
               * outHP.template response<Complex> (f, fs) * (af * (withMakeup ? makeup : 1.0));
    }

    double getAf() const noexcept { return af; }
    double getRin() const noexcept { return rin; }

    /** Level knob top (+12 dB) = Level pot fully up. */
    static constexpr double kLevelTop = 3.9810717055349722; // 10^(12/20)

    /** Exact analog response (for tests / validation): tone stage only, or the whole post network. */
    template <typename Complex>
    static Complex toneResponse (const PedalToneComponents& c, double tau, Complex s)
    {
        const auto rA = tau * c.rPot, rN = (1.0 - tau) * c.rPot;
        const auto g0 = 1.0 / c.rPost + (c.rBias > 0.0 ? 1.0 / c.rBias : 0.0);
        const auto P = rA * rN + c.rWiper * c.rPot;
        const Complex num = (c.rPot + s * c.cWiper * (P + rA * c.rFb)) / c.rPost;
        const Complex den = g0 * c.rPot + s * (g0 * c.cWiper * P + c.cPost * c.rPot + c.cWiper * rN)
                            + s * s * c.cPost * c.cWiper * P;
        return num / den;
    }

private:
    inline void updateTone (double t) noexcept
    {
        const auto rA = t * tc.rPot, rN = (1.0 - t) * tc.rPot;
        const auto g0 = 1.0 / tc.rPost + (tc.rBias > 0.0 ? 1.0 / tc.rBias : 0.0);
        const auto P = rA * rN + tc.rWiper * tc.rPot;
        tone.set (tc.rPot / tc.rPost, tc.cWiper * (P + rA * tc.rFb) / tc.rPost, 0.0,
                  g0 * tc.rPot, g0 * tc.cWiper * P + tc.cPost * tc.rPot + tc.cWiper * rN,
                  tc.cPost * tc.cWiper * P, K);
    }

    inline void updateLevel (double v) noexcept
    {
        const auto ru = (1.0 - v) * oc.rLevel, rl = v * oc.rLevel;
        const auto gl = 1.0 / rl + 1.0 / oc.rDrain;
        const auto r1t = oc.rSeries + ru, rb2 = oc.rSeries2 + rin;
        const auto c1 = oc.cCoupling, c2 = oc.cBase;
        level.set (0.0, 0.0, c1 * c2 * rin,
                   gl, c1 + c2 + gl * c2 * rb2 + gl * c1 * r1t,
                   c1 * c2 * rb2 + c1 * r1t * (gl * c2 * rb2 + c2), K);
    }

    double fs = 192000.0, K = 384000.0;
    PedalToneComponents tc;
    PedalOutputComponents oc;
    SBiquad tone, level, outHP;
    double af = 1.0, rin = 1.0e6, makeup = 1.0;
    double tau = 0.5, tauTarget = 0.5, wiper = 0.25, wiperTarget = 0.25;
    double smooth = 1.0;
};
} // namespace jb::dsp
