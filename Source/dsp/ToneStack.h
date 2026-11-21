#pragma once

#include "Filters.h"
#include "ChannelVoice.h"
#include <complex>

namespace jb::dsp
{
//==============================================================================
/** Passive Treble/Bass/Mid tone-stack network, solved from component values.

    Topology (classic passive TMB, ideal voltage source in, unloaded output):

        in ──Ct──┬─────────────── Treble pot top
                 │  Treble pot (Rt): (1-t)·Rt top→wiper(out), t·Rt wiper→bottom
        in ──Rs──X                 bottom ─ Bass rheostat (l·Rb) ─ Mid pot top
                 ├──Cb── Treble pot bottom
                 └──Cm── Mid wiper            Mid pot (Rm): (1-m)·Rm top→wiper,
                                                             m·Rm wiper→ground

    Closed form 3rd-order H(s) = (b1 s + b2 s² + b3 s³) / (1 + a1 s + a2 s² + a3 s³)
    (the standard Yeh/Smith TMB form). It is cross-checked against an independent
    nodal solve in Tests/ToneStackTest.cpp and tools/tonestack/. All three knobs
    interact because every coefficient mixes t, m and l, and the network keeps its
    real insertion loss; a fixed per-component-set makeup gain sits after it.
    Component values come from the sourced schematics (ChannelVoice.h redTmb /
    orangeTmb, Lead5150.h leadTmb; docs/tonestack.md).

    Discretised with the bilinear transform prewarped at 3 kHz (exact there;
    ~0.55 dB worst-case error to 8 kHz at 44.1 kHz, ~0.1 dB at 96 kHz). Knob values,
    component values and makeup are smoothed (~20 ms); the state-space model is
    re-solved every 8 frames while anything moves and ramped linearly per sample
    in between, so knob turns and Red/Orange switches never zipper or click. */
namespace tmb
{
struct Analog  { double b[4] {}, a[4] {}; };  // s-domain, a[0] = 1, b[0] = 0
struct Digital { double b[4] {}, a[4] {}; };  // z^-1 domain, a[0] = 1

/** Audio (log) taper: 15 % resistance at mid-rotation. p in 0..1 -> 0..1. */
inline double audioTaper (double p) noexcept
{
    constexpr double base = (0.85 / 0.15) * (0.85 / 0.15); // (1-m)^2/m^2, m = 0.15
    p = std::clamp (p, 0.0, 1.0);
    return (std::pow (base, p) - 1.0) / (base - 1.0);
}

/** Residual pot end/wiper resistance as a fraction of the track (0.1 %: 1 kOhm on
    a 1M pot). Physically real, and it keeps the network strictly 3rd order: with
    an ideal 0-ohm Bass end and Mid fully up the cubic terms vanish and the
    bilinear map would leave a marginal pole/zero pair at Nyquist. */
inline constexpr double potEnd = 0.001;
inline double potClamp (double p) noexcept { return std::clamp (p, potEnd, 1.0 - potEnd); }

/** Knob (0..10) -> pot fractions. Bass audio taper, Treble and Mid linear
    (as printed on the 5150 sheet: LOW 1M A, HIGH 250K B, MID 50K B). */
inline double bassPosition   (double knob) noexcept { return potClamp (audioTaper (knob / 10.0)); }
inline double treblePosition (double knob) noexcept { return potClamp (knob / 10.0); }
inline double midPosition    (double knob) noexcept { return potClamp (knob / 10.0); }

/** Analog coefficients for pot fractions t (treble), m (mid), l (bass). */
inline Analog analog (const TmbComponents& c, double t, double m, double l) noexcept
{
    const auto R1 = c.rt, R2 = c.rb, R3 = c.rm, R4 = c.rs, C1 = c.ct, C2 = c.cb, C3 = c.cm;
    const auto m2 = m * m;
    const auto C123 = C1 * C2 * C3;
    Analog h;
    h.b[1] = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
    h.b[2] = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4)
           - m2 * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
           + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
           + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
           + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
           + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
    h.b[3] = l * m * C123 * (R1 * R2 * R3 + R2 * R3 * R4)
           - m2 * C123 * (R1 * R3 * R3 + R3 * R3 * R4)
           + m * C123 * (R1 * R3 * R3 + R3 * R3 * R4)
           + t * C123 * R1 * R3 * R4 - t * m * C123 * R1 * R3 * R4
           + t * l * C123 * R1 * R2 * R4;
    h.a[0] = 1.0;
    h.a[1] = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
    h.a[2] = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
           + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
           - m2 * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
           + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
           + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4
              + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
    h.a[3] = l * m * C123 * (R1 * R2 * R3 + R2 * R3 * R4)
           - m2 * C123 * (R1 * R3 * R3 + R3 * R3 * R4)
           + m * C123 * (R3 * R3 * R4 + R1 * R3 * R3 - R1 * R3 * R4)
           + l * C123 * R1 * R2 * R4 + C123 * R1 * R3 * R4;
    return h;
}

/** Analog response at f Hz, evaluated directly at s = j*2*pi*f (no makeup). */
inline std::complex<double> response (const Analog& h, double hz) noexcept
{
    const std::complex<double> s (0.0, 2.0 * kPi * hz);
    const auto num = s * (h.b[1] + s * (h.b[2] + s * h.b[3]));
    const auto den = 1.0 + s * (h.a[1] + s * (h.a[2] + s * h.a[3]));
    return num / den;
}

inline constexpr double prewarpHz = 3000.0;

/** Bilinear transform with prewarping: s = K (1 - z^-1) / (1 + z^-1). */
inline Digital bilinear (const Analog& h, double fs, double warpHz = prewarpHz) noexcept
{
    const auto fw = std::min (warpHz, 0.25 * fs);
    const auto K = 2.0 * kPi * fw / std::tan (kPi * fw / fs);
    const auto K2 = K * K, K3 = K2 * K;
    const double B1 = h.b[1] * K, B2 = h.b[2] * K2, B3 = h.b[3] * K3;
    const double A0 = h.a[0], A1 = h.a[1] * K, A2 = h.a[2] * K2, A3 = h.a[3] * K3;
    // sum_k P_k (1 - z^-1)^k (1 + z^-1)^(3-k), expanded
    Digital d;
    const double nb[4] = { B1 + B2 + B3, B1 - B2 - 3.0 * B3, -B1 - B2 + 3.0 * B3, -B1 + B2 - B3 };
    const double na[4] = { A0 + A1 + A2 + A3, 3.0 * A0 + A1 - A2 - 3.0 * A3,
                           3.0 * A0 - A1 - A2 + 3.0 * A3, A0 - A1 + A2 - A3 };
    for (int i = 0; i < 4; ++i) { d.b[i] = nb[i] / na[0]; d.a[i] = na[i] / na[0]; }
    return d;
}

inline std::complex<double> response (const Digital& d, double hz, double fs) noexcept
{
    const auto z1 = std::polar (1.0, -2.0 * kPi * hz / fs);
    const auto num = d.b[0] + z1 * (d.b[1] + z1 * (d.b[2] + z1 * d.b[3]));
    const auto den = d.a[0] + z1 * (d.a[1] + z1 * (d.a[2] + z1 * d.a[3]));
    return num / den;
}
} // namespace tmb

//==============================================================================
/** Real-time passive TMB (one component set, smoothed knobs and components).

    Realised as a nodal (DK-style) state-space model of the actual network, not
    as a direct-form transfer function: each capacitor is a trapezoidal companion
    (conductance Gc = C*K, K = the prewarped bilinear constant) and the three
    states are the capacitors' history sources. Steady-state response is exactly
    tmb::bilinear(tmb::analog(...)); but because the states are physical capacitor
    quantities, moving a pot changes only the resistances around charged caps, as
    in the real circuit. A direct-form filter would instead re-interpret its
    abstract states under new coefficients, which clicks on fast knob moves. */
class PassiveToneStack
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Target component set (+ makeup). Snaps on the first call after prepare. */
    void setComponents (const TmbComponents& c) noexcept;
    /** Target knobs 0..10 (Bass, Mid, Treble). Snap on the first call after prepare. */
    void setKnobs (float bass, float mid, float treble) noexcept;

    /** Advance smoothing once per sample FRAME (shared by all channels). */
    inline void beginFrame() noexcept
    {
        if (ramp < updateInterval)
        {
            ++ramp;
            applyState (lerp (ssFrom, ssTo, (double) ramp / (double) updateInterval));
        }
        if (++counter < updateInterval) return;
        counter = 0;
        if (moving) step (updateInterval);
        flushTinyState();
    }

    inline float process (float xIn, int ch) noexcept
    {
        auto& J = hist[(size_t) ch];
        const auto u = (double) xIn;
        const auto y = ss.yu * u + ss.yj[0] * J[0] + ss.yj[1] * J[1] + ss.yj[2] * J[2];
        double vc[3];
        for (size_t k = 0; k < 3; ++k)
            vc[k] = ss.vu[k] * u + ss.vj[k][0] * J[0] + ss.vj[k][1] * J[1] + ss.vj[k][2] * J[2];
        for (size_t k = 0; k < 3; ++k)
            J[k] = 2.0 * ss.gc[k] * vc[k] - J[k];
        return (float) (y * gain);
    }

    /** Jump straight to the targets (session load / prepare): no fade-in. */
    void snap() noexcept { cur = tgt; moving = false; counter = 0; ramp = updateInterval; applyState (solve()); }

    /** Transfer function of the current settings (bilinear of the analog H; tests/plots). */
    tmb::Digital transferFunction() const noexcept;
    double makeupGain() const noexcept { return gain; }
    bool isSmoothing() const noexcept { return moving; }

    static constexpr int updateInterval = 8;
    static constexpr double smoothingSeconds = 0.020;

private:
    struct StateSpace;
    void step (int frames) noexcept;
    StateSpace solve() const noexcept;           // nodal solve at the current settings
    void applyState (const StateSpace& n) noexcept;
    void flushTinyState() noexcept
    {
        for (auto& ch : hist)
            for (auto& v : ch)
                if (std::abs (v) < 1.0e-30) v = 0.0; // no denormals on decaying tails
    }
    TmbComponents currentComponents() const noexcept;

    struct StateSpace
    {
        double yu = 0.0, yj[3] {};        // output = yu*u + yj.J
        double vu[3] {}, vj[3][3] {};     // capacitor voltages (Ct, Cb, Cm)
        double gc[3] {};                  // companion conductances
        double makeup = 1.0;
    };
    static StateSpace lerp (const StateSpace& a, const StateSpace& b, double f) noexcept
    {
        StateSpace r;
        auto mix = [f] (double x, double y) { return x + f * (y - x); };
        r.yu = mix (a.yu, b.yu); r.makeup = mix (a.makeup, b.makeup);
        for (int i = 0; i < 3; ++i)
        {
            r.yj[i] = mix (a.yj[i], b.yj[i]); r.vu[i] = mix (a.vu[i], b.vu[i]); r.gc[i] = mix (a.gc[i], b.gc[i]);
            for (int j = 0; j < 3; ++j) r.vj[i][j] = mix (a.vj[i][j], b.vj[i][j]);
        }
        return r;
    }

    double fs = 48000.0, alphaPerSample = 0.001, K = 96000.0;
    // current / target: treble, mid, bass knobs (0..10), rt, rb, rm, rs, ct, cb, cm, makeup gain
    static constexpr int numState = 11;
    std::array<double, numState> cur {}, tgt {};
    bool haveComponents = false, haveKnobs = false, moving = false;
    int counter = 0;
    StateSpace ss, ssFrom, ssTo;   // state-space ramps linearly (per sample) between solves
    int ramp = updateInterval;
    double gain = 1.0;
    std::array<std::array<double, 3>, kMaxChannels> hist {};
};

//==============================================================================
/** Rectifier-path tone section: passive TMB (per-channel component set) followed
    by the preamp Presence shelf (NFB-shelf idea; presence is not part of the
    passive TMB). Knobs 0..10. */
class ToneStack
{
public:
    void prepare (double sampleRate);
    void reset();

    void setVoicing (const ChannelVoicing& v);
    void setKnobs (float bass, float mid, float treble, float presence);

    inline void beginFrame() noexcept { tmbStack.beginFrame(); }
    void snap() noexcept { tmbStack.snap(); }

    inline float process (float x, int ch) noexcept
    {
        return presenceF.process (tmbStack.process (x, ch), ch);
    }

    const PassiveToneStack& passive() const noexcept { return tmbStack; }

private:
    void updatePresence();

    double fs = 44100.0;
    double presenceHz = 2400.0;
    float kPresence = 5.0f;
    PassiveToneStack tmbStack;
    Biquad presenceF;
};
} // namespace jb::dsp
