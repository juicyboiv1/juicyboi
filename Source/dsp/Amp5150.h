#pragma once

#include "Filters.h"
#include "SoftClip.h"
#include "RecoveryStage.h"
#include "SpeakerLoad.h"
#include "LeadPresence.h"
#include "LeadResonance.h"

#include <array>
#include <cmath>
#include <complex>
#include <algorithm>

namespace jb::dsp
{
/** Rev2.4 5150 preamp (Rhythm + Lead channels), power-section stand-in and
    speaker-load interaction. Values and their schematic sources: docs/5150.md.

    Signal flow (both channels share every tube, as on the real amp; the
    channel relay only swaps networks):
      V1A -> Pre Gain network (Lead: R29/C17 into VR2 + R41, bright C19/R40;
             Rhythm: K4 Crunch coupling network into VA1 + R21, bright C27 [+C10])
          -> V1B (Rhythm: C26 22uF extra cathode bypass)
          -> V2A cold clipper (Rhythm: R20 220K shunt at its grid divider)
          -> V2B -> V5B -> V5A -> shared passive TMB (Lead5150Tone)
          -> per-channel Post Gain -> Power5150 (recovery / power-section gain)
      then, at the base rate after the 5150 EQ: Presence + Resonance (one NFB
      loop) and the 4x12 load interaction whose output impedance comes from
      the same loop, so Resonance acts at the cabinet peak.
    Stage low-pass corners are the real Miller / RC corners (see docs/5150.md);
    the coupling high-passes stay behavioural (they stand in for grid blocking
    and bias shift that this static model does not simulate). */

/** Two-decade audio taper: fraction of the track below the wiper (0..1). */
inline double audioTaper (double knob) noexcept
{
    const auto p = std::clamp (knob, 0.0, 10.0) / 10.0;
    return (std::pow (10.0, 2.0 * p) - 1.0) / 99.0;
}

//==============================================================================
/** Parts of the network between V1A's plate and V1B's grid.
    Topology (node names in the comments of PreGainNetwork):
      Vin -[rs]- n0 -[cc]- n1 -[rSer || cSer]- T -[pot upper || rTop || cTop]- W -[pot lower]- gnd
      T -[rSh]- gnd,   T -[rSh2]- n4 -[cSh2]- gnd                                   */
struct PreNetworkParts
{
    double rs   = 48.0e3;   // V1A plate source impedance: R2 220K || rp ~62K
    double cc   = 22.0e-9;  // C6 .022u coupling
    double rSer = 470.0e3, cSer = 2.2e-9;     // Lead: R29 470K || C17 .0022u
    double rSh  = 1.0e6;                      // Lead: R41 1M across the pot
    double rSh2 = 1.0e12, cSh2 = 1.0e-15;     // optional series R-C shunt (absent)
    double pot  = 1.0e6;                      // VR2 / VA1 1M audio
    double rTop = 1.0e6;                      // Lead: R40 1M top->wiper (0 = absent)
    double cTop = 1.0e-9;                     // Lead: C19 .001u top->wiper
};

/** Lead Pre Gain network, straight from the sheet. */
inline PreNetworkParts leadPreParts() noexcept { return {}; }

/** Rhythm Pre Gain network (interpretation, docs/5150.md "Rhythm channel"):
      Crunch off: R43 2.2M || C13 470p in series, K4B branch C63 .1u + R23 39K
                  as a shunt at the pot top (heavy, bass-cutting attenuation)
      Crunch on:  R19 470K || C51 270p in series, shunt branch lifted
      always:     R21 1M at the pot top, VA1 1M audio, C27 180p top->wiper
      Bright:     adds C10 470p top->wiper (sheet value, not calibrated): +7.9 dB at
                  2 kHz at Pre 5, +6 dB (the manual's figure) near Pre 6.5, none at 10
    `bright` and `crunch` are 0..1 so switching morphs the parts (click-free). */
inline PreNetworkParts rhythmPreParts (double bright, double crunch) noexcept
{
    PreNetworkParts p;
    const auto c = std::clamp (crunch, 0.0, 1.0);
    p.rSer = std::exp (std::log (2.2e6) + c * (std::log (470.0e3) - std::log (2.2e6)));
    p.cSer = 470.0e-12 + c * (270.0e-12 - 470.0e-12);
    p.rSh  = 1.0e6;
    p.rSh2 = 39.0e3 / std::max (1.0e-7, 1.0 - c);  // lifted to ~open with Crunch
    p.cSh2 = 0.1e-6;
    p.rTop = 0.0;
    p.cTop = 180.0e-12 + std::clamp (bright, 0.0, 1.0) * 470.0e-12;
    return p;
}

/** Linear 5-node nodal (MNA) solve of PreNetworkParts, trapezoidal companion
    models for the four capacitors, double precision. The inverse matrix is
    rebuilt only when the pot or the parts move (every 8 frames while slewing). */
class PreGainNetwork
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        slewFrames = std::max (1, (int) std::round (0.020 * fs));
        knobNow = knobTarget;
        rebuild();
        reset();
    }

    void reset() noexcept { for (auto& ch : st) for (auto& c : ch) c = {}; }

    void setKnob (double knob) noexcept { knobTarget = std::clamp (knob, 0.0, 10.0); }
    void setParts (const PreNetworkParts& p) noexcept { parts = p; partsDirty = true; }
    void snap() noexcept { knobNow = knobTarget; rebuild(); }

    /** Once per sample frame. */
    void beginFrame() noexcept
    {
        if (++counter < 8) return;
        counter = 0;
        bool dirty = partsDirty;
        if (std::abs (knobNow - knobTarget) > 1.0e-9)
        {
            const auto stepMax = 10.0 * 8.0 / (double) slewFrames; // full sweep in 20 ms
            knobNow += std::clamp (knobTarget - knobNow, -stepMax, stepMax);
            dirty = true;
        }
        if (dirty) rebuild();
    }

    inline float process (float xIn, int ch) noexcept
    {
        auto& s = st[(size_t) ch];
        double b[N] {};
        b[0] = gs * (double) xIn;
        double ieq[NC];
        for (int k = 0; k < NC; ++k)
        {
            ieq[k] = gc[k] * s[k].v + s[k].i;
            if (capA[k] >= 0) b[capA[k]] += ieq[k];
            if (capB[k] >= 0) b[capB[k]] -= ieq[k];
        }
        double v[N];
        for (int r = 0; r < N; ++r)
        {
            double acc = 0.0;
            for (int c = 0; c < N; ++c) acc += inv[r][c] * b[c];
            v[r] = acc;
        }
        for (int k = 0; k < NC; ++k)
        {
            const auto va = capA[k] >= 0 ? v[capA[k]] : 0.0, vb = capB[k] >= 0 ? v[capB[k]] : 0.0;
            s[k].v = va - vb;
            s[k].i = gc[k] * s[k].v - ieq[k];
        }
        return (float) v[3];
    }

    /** Analytic |H(f)| of the network (same nodal equations, complex), for tests/docs. */
    static double magnitude (const PreNetworkParts& p, double knob, double f) noexcept
    {
        using C = std::complex<double>;
        const C s (0.0, 2.0 * kPi * f);
        C y[N][N] {};
        auto add = [&] (int a, int b, C g)
        {
            if (a >= 0) y[a][a] += g;
            if (b >= 0) y[b][b] += g;
            if (a >= 0 && b >= 0) { y[a][b] -= g; y[b][a] -= g; }
        };
        const auto alpha = audioTaper (knob);
        add (0, -1, 1.0 / p.rs);
        add (0, 1, s * p.cc);
        add (1, 2, 1.0 / p.rSer + s * p.cSer);
        add (2, -1, 1.0 / p.rSh);
        add (2, 4, 1.0 / p.rSh2);
        add (4, -1, s * p.cSh2);
        add (2, 3, 1.0 / std::max (10.0, (1.0 - alpha) * p.pot) + (p.rTop > 0.0 ? 1.0 / p.rTop : 0.0) + s * p.cTop);
        add (3, -1, 1.0 / std::max (potEndR, alpha * p.pot));
        for (int i = 0; i < N; ++i) y[i][i] += leak;
        C rhs[N] {}; rhs[0] = 1.0 / p.rs;
        // Gaussian elimination (complex, tiny system)
        for (int c = 0; c < N; ++c)
        {
            int piv = c;
            for (int r = c + 1; r < N; ++r) if (std::abs (y[r][c]) > std::abs (y[piv][c])) piv = r;
            for (int k = 0; k < N; ++k) std::swap (y[c][k], y[piv][k]);
            std::swap (rhs[c], rhs[piv]);
            for (int r = 0; r < N; ++r)
            {
                if (r == c) continue;
                const auto m = y[r][c] / y[c][c];
                for (int k = 0; k < N; ++k) y[r][k] -= m * y[c][k];
                rhs[r] -= m * rhs[c];
            }
        }
        return std::abs (rhs[3] / y[3][3]);
    }

private:
    static constexpr int N = 5, NC = 4;
    static constexpr double leak = 1.0e-12;
    static constexpr double potEndR = 1000.0; // carbon-track end resistance: Pre 0 is very quiet, not dead
    // capacitor k between capA[k] and capB[k] (-1 = ground): C6, cSer, cSh2, cTop
    static constexpr int capA[NC] { 0, 1, 4, 2 };
    static constexpr int capB[NC] { 1, 2, -1, 3 };

    void rebuild() noexcept
    {
        partsDirty = false;
        const auto alpha = audioTaper (knobNow);
        const auto T2 = 2.0 * fs;
        gc[0] = T2 * parts.cc; gc[1] = T2 * parts.cSer; gc[2] = T2 * parts.cSh2; gc[3] = T2 * parts.cTop;
        gs = 1.0 / parts.rs;
        double y[N][N] {};
        auto add = [&] (int a, int b, double g)
        {
            if (a >= 0) y[a][a] += g;
            if (b >= 0) y[b][b] += g;
            if (a >= 0 && b >= 0) { y[a][b] -= g; y[b][a] -= g; }
        };
        add (0, -1, gs);
        add (1, 2, 1.0 / parts.rSer);
        add (2, -1, 1.0 / parts.rSh);
        add (2, 4, 1.0 / parts.rSh2);
        add (2, 3, 1.0 / std::max (10.0, (1.0 - alpha) * parts.pot) + (parts.rTop > 0.0 ? 1.0 / parts.rTop : 0.0));
        add (3, -1, 1.0 / std::max (potEndR, alpha * parts.pot));
        for (int k = 0; k < NC; ++k) add (capA[k], capB[k], gc[k]);
        for (int i = 0; i < N; ++i) y[i][i] += leak;
        // Gauss-Jordan inverse
        double a[N][2 * N] {};
        for (int r = 0; r < N; ++r) { for (int c = 0; c < N; ++c) a[r][c] = y[r][c]; a[r][N + r] = 1.0; }
        for (int c = 0; c < N; ++c)
        {
            int piv = c;
            for (int r = c + 1; r < N; ++r) if (std::abs (a[r][c]) > std::abs (a[piv][c])) piv = r;
            if (piv != c) for (int k = 0; k < 2 * N; ++k) std::swap (a[c][k], a[piv][k]);
            const auto d = a[c][c];
            for (int k = 0; k < 2 * N; ++k) a[c][k] /= d;
            for (int r = 0; r < N; ++r)
            {
                if (r == c) continue;
                const auto m = a[r][c];
                if (m == 0.0) continue;
                for (int k = 0; k < 2 * N; ++k) a[r][k] -= m * a[c][k];
            }
        }
        for (int r = 0; r < N; ++r) for (int c = 0; c < N; ++c) inv[r][c] = a[r][N + c];
    }

    struct Cap { double v = 0.0, i = 0.0; };
    PreNetworkParts parts;
    double fs = 176400.0, gs = 1.0 / 48.0e3;
    double gc[NC] {};
    double inv[N][N] {};
    double knobTarget = 6.0, knobNow = 6.0;
    int slewFrames = 3528, counter = 0;
    bool partsDirty = true;
    std::array<std::array<Cap, NC>, kMaxChannels> st {};
};

//==============================================================================
/** First-order shelf H(s) = (s + wn)/(s + wd): 1 at HF, wn/wd at DC.
    Used for the Rhythm channel's extra V1B cathode bypass (C26 22uF). */
class ShelfOnePole
{
public:
    void setup (double fn, double fd, double fs) noexcept
    {
        // bilinear: s = c (1 - z^-1)/(1 + z^-1), c = 2 fs (corners far below fs/2)
        const auto c = 2.0 * fs, wn = 2.0 * kPi * fn, wd = 2.0 * kPi * fd;
        const auto a0 = c + wd;
        b0 = (float) ((c + wn) / a0); b1 = (float) ((wn - c) / a0); a1 = (float) ((wd - c) / a0);
    }
    void reset() noexcept { x1.fill (0.0f); y1.fill (0.0f); }
    inline float process (float x, int ch) noexcept
    {
        const auto y = b0 * x + b1 * x1[(size_t) ch] - a1 * y1[(size_t) ch];
        x1[(size_t) ch] = x; y1[(size_t) ch] = y;
        return y;
    }
private:
    float b0 = 1.0f, b1 = 0.0f, a1 = 0.0f;
    std::array<float, kMaxChannels> x1 {}, y1 {};
};

//==============================================================================
/** One triode stage: input coupling HP, input low-pass(es) (Miller / RC at the
    grid), static asymmetric curve, inverting, normalised attenuation. */
struct Stage5150
{
    double hp, lp, lp2;      // lp2 = 0 -> single pole
    float drive, bias, positive, negative, attenuation;
};

/** All tunables of the 5150 model in one place (docs/5150.md has the derivations). */
// Rev2.4 tuned values (docs/5150.md "Tuning"): the stage hp corners are tuned stand-ins for the
// cathode-bypass / coupling shelves, set so 80-110 Hz and the 40 Hz difference tone match Red v2.
struct Voicing5150
{
    std::array<Stage5150, 6> stages {{
        //  hp     lp(real)  lp2      drive  bias   pos    neg    atten
        {  25.0, 13000.0,    0.0,     1.4f, 0.04f, 1.10f, 0.92f, 0.72f }, // V1A: R22 68K+22K jack, ~136 pF Miller
        {  45.0,  2300.0,    0.0,     7.0f, 0.05f, 1.15f, 0.90f, 0.48f }, // V1B: ~720K (R82 470K + pot) x ~100 pF Miller
        {  55.0, 30000.0,    0.0,     4.2f, 0.32f, 3.20f, 0.72f, 0.68f }, // V2A cold (39K unbypassed): ~340K x ~12 pF
        {  70.0,  2100.0, 4400.0,     7.0f, 0.07f, 1.20f, 0.90f, 0.42f }, // V2B: C46 1nF on ~75K, then R7 220K x ~120 pF
        {  65.0, 22700.0,    0.0,     4.4f,-0.15f, 1.10f, 0.95f, 0.60f }, // V5B: R32/R101 ~91K x ~77 pF
        {  25.0,  2200.0,    0.0,     4.5f, 0.25f, 1.05f, 0.95f, 1.00f }  // V5A: R87/R88/R91 ~1.5M x ~47 pF
    }};
    double outLpHz = 12000.0;    // power-section / OT bandwidth stand-in (not a preamp part)
    float  outGain = 0.72f;
    float  preGainTrim = 3.0f;   // level into V1B after the Pre Gain network

    // Rhythm-only differences (channel relay K1, docs/5150.md)
    double rhythmV1bShelfN = 187.0, rhythmV1bShelfD = 87.0; // C26 22uF: undoes the 1uF bypass shelf
    float  rhythmV2aGridLoss = 0.408f; // R20 220K shunt: (1M||220K)/(470K+1M||220K) / (1M/1.47M)
    float  rhythmTrim = 3.5f;   // Rhythm clean at Pre 3-5, crunchy from ~7 (Post 5)

    // Power section (Post Gain -> RecoveryStage)
    float postScale = 17.0f;     // Post knob gain (audio taper) x postScale into the stage
    float powerDrive = 2.0f, powerBias = -0.40f, powerPositive = 1.30f, powerNegative = 1.00f;
    float powerOut = 0.33f;      // fixed level after the stage
    double powerHpHz = 12.0;

    // 4x12 load (SpeakerLoad) with closed-loop output impedance from the NFB loop
    double spkRe = 6.4, spkRes = 35.0, spkF0 = 100.0, spkQ = 3.0;
    double zOpen = 24.0;         // open-loop output impedance at the speaker tap, ohms
};

inline const Voicing5150& defaultVoicing5150() noexcept
{
    static const Voicing5150 v;
    return v;
}

/** The six-stage cascade with its Pre Gain network. One instance per channel. */
class Cascade5150
{
public:
    void configure (const Voicing5150& v, bool rhythmChannel) noexcept { voicing = v; rhythm = rhythmChannel; }

    void prepare (double fs) noexcept
    {
        for (size_t i = 0; i < 6; ++i)
        {
            const auto& s = voicing.stages[i];
            hp[i].setup (OnePole::Type::highPass, s.hp, fs);
            lp[i].setup (OnePole::Type::lowPass, s.lp, fs);
            lp2[i].setup (OnePole::Type::lowPass, s.lp2 > 0.0 ? s.lp2 : 20000.0, fs);
            hp[i].reset(); lp[i].reset(); lp2[i].reset();
        }
        shelf.setup (voicing.rhythmV1bShelfN, voicing.rhythmV1bShelfD, fs); shelf.reset();
        out.setup (OnePole::Type::lowPass, voicing.outLpHz, fs); out.reset();
        dc.setup (OnePole::Type::highPass, 12.0, fs); dc.reset();
        if (! rhythm) net.setParts (leadPreParts());
        net.prepare (fs);
        smoothing = (float) (1.0 - std::exp (-1.0 / (0.030 * fs)));
        crunchNow = crunchTarget; brightNow = brightTarget;
        if (rhythm) net.setParts (rhythmPreParts (brightNow, crunchNow));
        net.snap();
    }

    void setPreGain (float knob) noexcept { net.setKnob ((double) knob); }
    void setSwitches (bool bright, bool crunch) noexcept { brightTarget = bright ? 1.0f : 0.0f; crunchTarget = crunch ? 1.0f : 0.0f; }
    void snap() noexcept
    {
        crunchNow = crunchTarget; brightNow = brightTarget;
        if (rhythm) net.setParts (rhythmPreParts (brightNow, crunchNow));
        net.snap();
    }

    /** Once per sample frame. */
    void beginFrame() noexcept
    {
        if (rhythm && (std::abs (crunchNow - crunchTarget) > 1.0e-5f || std::abs (brightNow - brightTarget) > 1.0e-5f))
        {
            crunchNow += smoothing * (crunchTarget - crunchNow);
            brightNow += smoothing * (brightTarget - brightNow);
            if (std::abs (crunchNow - crunchTarget) <= 1.0e-5f) crunchNow = crunchTarget;
            if (std::abs (brightNow - brightTarget) <= 1.0e-5f) brightNow = brightTarget;
            if (++morphCounter >= 8) { morphCounter = 0; net.setParts (rhythmPreParts (brightNow, crunchNow)); }
            if (crunchNow == crunchTarget && brightNow == brightTarget) net.setParts (rhythmPreParts (brightNow, crunchNow));
        }
        net.beginFrame();
    }

    float process (float x, int ch) noexcept
    {
        const auto& st = voicing.stages;
        // V2A grid divider: Rhythm has R20 220K in shunt; Crunch lifts it.
        const auto v2aLoss = rhythm ? voicing.rhythmV2aGridLoss + crunchNow * (1.0f - voicing.rhythmV2aGridLoss) : 1.0f;
        for (size_t i = 0; i < 6; ++i)
        {
            const auto& s = st[i];
            x = hp[i].process (x, ch);
            x = lp[i].process (x, ch);
            if (s.lp2 > 0.0) x = lp2[i].process (x, ch);
            if (i == 1 && rhythm) x = shelf.process (x, ch);
            x = -shape::triode (x * s.drive, s.bias, s.positive, s.negative) * s.attenuation;
            if (i == 0) x = net.process (x, ch) * voicing.preGainTrim * (rhythm ? voicing.rhythmTrim : 1.0f);
            if (i == 1) x *= v2aLoss;
        }
        // Six inversions: same nominal polarity as the input.
        return dc.process (out.process (x, ch) * voicing.outGain, ch);
    }

private:
    Voicing5150 voicing;
    bool rhythm = false;
    std::array<OnePole, 6> hp, lp, lp2;
    ShelfOnePole shelf;
    OnePole out, dc;
    PreGainNetwork net;
    float smoothing = 0.001f, crunchNow = 0.0f, crunchTarget = 0.0f, brightNow = 0.0f, brightTarget = 0.0f;
    int morphCounter = 0;
};

//==============================================================================
/** Power section stand-in after the Post Gain pot: the 5150's FX-return / V3A
    driver, phase inverter and power tubes, as one asymmetric static curve
    (RecoveryStage, NOT mirrored, so it balances the cascade's even-order
    excess instead of adding to it). The Post knob sets how hard it is driven,
    as on the real amp; there is no Master on the 5150 faceplate. */
class Power5150
{
public:
    void prepare (double fs, const Voicing5150& v) noexcept
    {
        voicing = v;
        RecoveryStage::Settings s;
        s.drive = v.powerDrive; s.bias = v.powerBias; s.positive = v.powerPositive; s.negative = v.powerNegative;
        s.mirror = false; s.outGain = 1.0f; s.hpHz = v.powerHpHz;
        stage.prepare (fs, s);
    }
    inline float process (float x, int ch) noexcept { return stage.process (x * voicing.postScale, ch) * voicing.powerOut; }

    /** Post knob (0..10) -> pot gain (two-decade audio taper; 0 = silent). */
    static float postGain (float knob) noexcept { return (float) audioTaper ((double) knob); }

private:
    Voicing5150 voicing;
    RecoveryStage stage;
};

//==============================================================================
/** Closed-loop output impedance of the 5150 power amp at frequency f, from the
    same declared NFB loop as LeadPresence/LeadResonance (R56 39K, R57 4.7K,
    VR9/C8||C62, VR8||C12, A = 20): Zcl = Zopen / |1 + A*beta(jw)|. */
inline double closedLoopZout (double zOpen, double resonanceR, double presenceR, double f) noexcept
{
    using C = std::complex<double>;
    const C s (0.0, 2.0 * kPi * f);
    const C rf = LeadResonance::feedbackR + (resonanceR > 0.0 ? resonanceR / (1.0 + s * resonanceR * LeadResonance::resonanceC) : C (0.0));
    const C zp = presenceR + 1.0 / (s * LeadResonance::presenceC);
    const C z = (LeadResonance::tailR * zp) / (LeadResonance::tailR + zp);
    const C aBeta = LeadResonance::forwardGain * z / (rf + z);
    return zOpen / std::abs (1.0 + aBeta);
}

/** 4x12 load interaction with a slowly updated output impedance (Resonance and
    Presence move it). Base rate, after Presence/Resonance. Coefficients update
    every 32 frames while the impedance slews (no state reset). */
class SpeakerLoad5150
{
public:
    void prepare (double sampleRate, const Voicing5150& v) noexcept
    {
        fs = sampleRate; voicing = v;
        zNow = zTarget;
        apply();
        load.reset();
    }
    void setLoop (double resonanceR, double presenceR) noexcept
    {
        zTarget = closedLoopZout (voicing.zOpen, resonanceR, presenceR, voicing.spkF0);
    }
    void beginFrame() noexcept
    {
        if (++counter < 32) return;
        counter = 0;
        if (std::abs (zNow - zTarget) > 1.0e-6)
        {
            zNow += 0.25 * (zTarget - zNow);
            if (std::abs (zNow - zTarget) < 1.0e-4) zNow = zTarget;
            apply();
        }
    }
    float process (float x, int ch) noexcept { return load.process (x, ch); }
    SpeakerLoadParams params() const noexcept { return { voicing.spkRe, voicing.spkRes, voicing.spkF0, voicing.spkQ, zNow }; }
    double outputImpedance() const noexcept { return zNow; }

private:
    void apply() noexcept { load.setParams (fs, params()); }
    Voicing5150 voicing;
    SpeakerLoad load;
    double fs = 48000.0, zNow = 4.0, zTarget = 4.0;
    int counter = 0;
};
} // namespace jb::dsp
