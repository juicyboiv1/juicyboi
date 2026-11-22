#pragma once

// Component-level op-amp diode clipper (Rev2.2).
//
// Circuit (non-inverting op-amp, everything referenced to Vr = AC ground):
//
//              +--- Rf = Rfixed + Rdrive(taper) ---+
//              +--- Cfb (0 = not fitted) ----------+
//              +--- Np diodes  (inn -> out) <------+   conduct when out > inn
//              +--- Nn diodes  (out -> inn) ------>+   conduct when out < inn
//              |                                    |
//   vin ->(+)--|>-----------------------------------+-- out
//         (-)--+
//              |
//             Rleg
//              |
//             Cleg
//              |
//             Vr
//
// Op-amp: single-pole model  dvo/dt = wt (v+ - v-) - (wt / A0) vo
// (GBW = wt / 2pi, DC gain A0; JRC/NJM4558 datasheet: 3 MHz, 100 dB typ.).
// No output rails / slew limit (see docs/diode-clippers.md). gbw = 0 selects an
// ideal op-amp (v- = v+).
//
// Unknowns per sample: v- (vn) and the voltage across the feedback network
// vd = vo - vn. KCL at v-:
//     vd / Rf + Cfb dvd/dt + Id(vd) = i_leg(vn)
// with the Shockley diode strings
//     Id(v) = Is (exp(v / (Np n Vt)) - 1) - Is (exp(-v / (Nn n Vt)) - 1).
// Rleg-Cleg and Cfb use trapezoidal companions (SPICE's default "trap"); the
// op-amp integrator uses BDF2 (2nd order and L-stable: with the diodes
// conducting its closed-loop pole sits far above Nyquist, where trapezoidal
// would ring at fs/2). The op-amp and leg equations are linear, so vn is
// eliminated in closed form and the remaining scalar implicit equation in vd
// is solved every sample with a bracketed Newton-Raphson iteration (strictly
// monotonic => unique root; the bracket guarantees convergence).

#include "Filters.h"

#include <array>
#include <cmath>
#include <algorithm>

namespace jb::dsp
{
struct ClipperComponents
{
    double rLeg      = 4.7e3;    // gain-leg resistor
    double cLeg      = 47e-9;    // gain-leg capacitor
    double rFixed    = 51e3;     // fixed feedback resistor
    double rDrivePot = 500e3;    // Drive pot (wired as rheostat)
    double cFeedback = 51e-12;   // feedback capacitor (0 = not fitted)
    int    diodesPos = 1;        // diodes in series conducting for out > in
    int    diodesNeg = 1;        // diodes in series conducting for out < in
    double is        = 2.52e-9;  // diode saturation current [A]
    double n         = 1.752;    // diode emission coefficient
    double vt        = 0.025851; // thermal voltage at 27 C (SPICE default TNOM)
    double gbw       = 3.0e6;    // op-amp gain-bandwidth [Hz] (0 = ideal op-amp)
    double a0        = 1.0e5;    // op-amp DC open-loop gain (100 dB)
};

class OpAmpDiodeClipper
{
public:
    /** Audio-taper (log) Drive law: 0..1 knob -> fraction of the pot in circuit. */
    static double driveTaper (double d01) noexcept
    {
        d01 = std::clamp (d01, 0.0, 1.0);
        return (std::pow (10.0, 2.0 * d01) - 1.0) / 99.0;
    }

    void setComponents (const ClipperComponents& c) noexcept { comp = c; recalc(); }
    const ClipperComponents& components() const noexcept { return comp; }

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        // ~20 ms one-pole glide for the Drive resistance (no zipper / level jumps)
        smoothCoeff = 1.0 - std::exp (-1.0 / (0.020 * fs));
        recalc();
        reset();
    }

    void reset() noexcept
    {
        jLeg.fill (0.0); jFb.fill (0.0); vPrev.fill (0.0); voPrev.fill (0.0); voPrev2.fill (0.0); idPrev.fill (0.0); gdPrev.fill (0.0);
    }

    void setDrive (double drive01) noexcept
    {
        targetRf = comp.rFixed + comp.rDrivePot * driveTaper (drive01);
    }

    void snap() noexcept { currentRf = targetRf; }

    double feedbackResistance() const noexcept { return currentRf; }

    /** Call once per sample frame (shared by all channels). */
    inline void beginFrame() noexcept
    {
        currentRf += smoothCoeff * (targetRf - currentRf);
        gRf = 1.0 / currentRf;
    }

    /** vin = op-amp (+) input voltage; returns op-amp output voltage. */
    inline double process (double vin, int ch) noexcept
    {
        const auto c = (size_t) ch;

        // Gain leg (Rleg + Cleg companion) seen from v-:  i_leg = gEq vn - jEq
        const auto jEq = jLeg[c] * gRleg / (gRleg + gCleg);

        // Op-amp (BE):  vo = P - Ks vn,  vo = vn + vd  =>  vn = (P - vd) * invOneKs
        // (ideal op-amp: vn = vin)
        double vn, vd;
        if (idealOpAmp)
        {
            vn = vin;
            vd = solve (gRf + gCfb, gEq * vn - jEq + jFb[c], c);
        }
        else
        {
            // BDF2 history: (4 vo[n-1] - vo[n-2]) / 3
            const auto P = opS * ((4.0 * voPrev[c] - voPrev2[c]) * (1.0 / 3.0) + opK * vin);
            const auto gl = gEq * invOneKs;
            vd = solve (gRf + gCfb + gl, gl * P - jEq + jFb[c], c);
            vn = (P - vd) * invOneKs;
        }
        const auto vo = vn + vd;

        // state updates
        const auto vCleg = (vn * gRleg + jLeg[c]) / (gRleg + gCleg);
        jLeg[c]   = flush (2.0 * gCleg * vCleg - jLeg[c]);
        jFb[c]    = flush (2.0 * gCfb * vd - jFb[c]);
        vPrev[c]  = vd;
        voPrev2[c] = voPrev[c];
        voPrev[c]  = flush (vo);
        return vo;
    }

    int lastIterations() const noexcept { return iterations; }

private:
    static inline double flush (double v) noexcept { return std::abs (v) < 1.0e-30 ? 0.0 : v; }

    void recalc() noexcept
    {
        const auto T = 1.0 / fs;
        gRleg = 1.0 / comp.rLeg;
        gCleg = 2.0 * comp.cLeg / T;
        gCfb  = 2.0 * comp.cFeedback / T;
        gEq   = gRleg * gCleg / (gRleg + gCleg);
        idealOpAmp = ! (comp.gbw > 0.0);
        if (! idealOpAmp)
        {
            const auto wt = 2.0 * kPi * comp.gbw;
            // BDF2: vo = (4 vo1 - vo2)/3 + (2/3) T (wt (v+ - v-) - (wt/A0) vo)
            opS = 1.0 / (1.0 + (2.0 / 3.0) * (wt / comp.a0) * T);
            opK = (2.0 / 3.0) * wt * T;
            const auto ks = opS * opK;
            invOneKs = 1.0 / (1.0 + ks);
        }
        gRf = 1.0 / currentRf;
        invVp = 1.0 / ((double) comp.diodesPos * comp.n * comp.vt);
        invVn = 1.0 / ((double) comp.diodesNeg * comp.n * comp.vt);
        ratio = comp.diodesPos == comp.diodesNeg ? Ratio::same
              : comp.diodesPos == 2 * comp.diodesNeg ? Ratio::double_ : Ratio::other;
        targetRf = std::clamp (targetRf, comp.rFixed, comp.rFixed + comp.rDrivePot);
        currentRf = std::clamp (currentRf, comp.rFixed, comp.rFixed + comp.rDrivePot);
    }

    static inline double safeExp (double x) noexcept { return std::exp (std::clamp (x, -300.0, 300.0)); }

    /** Solve f(v) = g v + Id(v) - b = 0 (strictly increasing in v) for channel c. */
    inline double solve (double g, double b, size_t c) noexcept
    {
        const auto is = comp.is;
        // Root lies between 0 and b/g (Id(v) has the sign of v).
        const auto vLin = b / g;
        double lo = std::min (0.0, vLin), hi = std::max (0.0, vLin);
        bool tightened = false;

        // Predictor: one Newton step from last sample's root using last
        // sample's diode current/conductance (no exp needed).
        auto v = vPrev[c] + (b - (g * vPrev[c] + idPrev[c])) / (g + gdPrev[c]);
        v = std::clamp (v, lo, hi);
        if (v * invVp > 25.0 || -v * invVn > 25.0)   // > ~0.2 A in the diodes: implausible start
        {
            tightened = true;
            if (b >= 0.0) hi = std::min (hi, std::log1p (b / is) / invVp);
            else          lo = std::max (lo, -std::log1p (-b / is) / invVn);
            v = std::clamp (v, lo, hi);
        }

        double id = 0.0, gd = 0.0;
        int it = 0;
        for (; it < maxIterations; ++it)
        {
            // one exp per iteration: exp(-v/(Nn n Vt)) = ep^(-Np/Nn)
            const auto ep = safeExp (v * invVp);
            const auto en = ratio == Ratio::same ? 1.0 / ep
                          : ratio == Ratio::double_ ? 1.0 / (ep * ep)
                          : safeExp (-v * invVn);
            id = is * (ep - en);
            gd = is * (ep * invVp + en * invVn);
            const auto f = g * v + id - b;

            if (f > 0.0) hi = v; else lo = v;

            auto next = v - f / (g + gd);
            // Deep in conduction with the diode current overshooting, Newton only
            // retreats one N n Vt per step; invert the diode law instead (exact
            // when the diode term dominates).
            const auto rest = b - g * v;   // current the diodes should carry
            if (v > 0.0 && id > 4.0 * rest && rest > 0.0)
                next = std::log1p (rest / is) / invVp;
            else if (v < 0.0 && id < 4.0 * rest && rest < 0.0)
                next = -std::log1p (-rest / is) / invVn;
            // SPICE-style junction limiting: a step that pushes deeper into
            // conduction is compressed logarithmically (Newton otherwise crawls
            // back down the exponential one n*Vt per iteration).
            if (next > v && next > 0.0)
            {
                const auto from = std::max (v, 0.0), vt1 = 1.0 / invVp;
                if (next - from > 2.0 * vt1) next = from + vt1 * std::log1p ((next - from) / vt1);
            }
            else if (next < v && next < 0.0)
            {
                const auto from = std::min (v, 0.0), vt1 = 1.0 / invVn;
                if (from - next > 2.0 * vt1) next = from - vt1 * std::log1p ((from - next) / vt1);
            }
            const bool wild = next * invVp > 25.0 || -next * invVn > 25.0;  // > ~0.2 A
            if (! tightened && (wild || ! (next >= lo && next <= hi)))
            {
                // diode-only bound: |Id(v)| <= |b|  =>  |v| <= N n Vt ln(1 + |b|/Is)
                tightened = true;
                if (b >= 0.0) hi = std::min (hi, std::log1p (b / is) / invVp);
                else          lo = std::max (lo, -std::log1p (-b / is) / invVn);
            }
            if (! (next >= lo && next <= hi)) // outside bracket (or NaN) -> bisect
                next = 0.5 * (lo + hi);

            const auto step = std::abs (next - v);
            v = next;
            if (step < 1.0e-8 || (hi - lo) < 1.0e-12)   // 10 nV: far below float output resolution
                break;
        }
        iterations = it + 1;
        // id/gd belong to the last evaluated point; close enough for the next predictor
        idPrev[c] = id; gdPrev[c] = gd;
        return v;
    }

    ClipperComponents comp;
    double fs = 192000.0;
    double smoothCoeff = 1.0;
    double targetRf = 51e3, currentRf = 51e3, gRf = 1.0 / 51e3;
    double gRleg = 1.0, gCleg = 0.0, gCfb = 0.0, gEq = 0.0, invVp = 1.0, invVn = 1.0;
    double opS = 1.0, opK = 0.0, invOneKs = 1.0;
    bool idealOpAmp = true;
    enum class Ratio { same, double_, other } ratio = Ratio::same;
    std::array<double, kMaxChannels> jLeg {}, jFb {}, vPrev {}, voPrev {}, voPrev2 {}, idPrev {}, gdPrev {};
    int iterations = 0;
    static constexpr int maxIterations = 50;
};
} // namespace jb::dsp
