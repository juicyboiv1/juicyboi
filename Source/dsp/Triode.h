#pragma once

// Rev3: solved 12AX7 common-cathode gain stage with grid conduction and
// coupling-cap bias shift. docs/triode.md has the derivation, the component
// values per stage and the ngspice validation (tools/triode/*.cir).
//
// Circuit (one stage, everything referenced to ground):
//
//   vs --[Rs]--||Cc||-- n --[Rgs]-- grid      B+ --[Ra]-- plate (output)
//                       |                       cathode --[Rk || Ck]-- gnd
//                      [Rg]
//                       |
//                      gnd
//
//   Plate current:  Koren 12AX7 (mu 100, ex 1.4, kg1 1060, kp 600, kvb 300)
//   Grid current:   Cohen/Helie 12AX7 grid diode
//                   Ig = Gg * (ln(1 + e^(Cg*Vgk)) / Cg)^xi + Ig0
//
// vs is the Thevenin (open-circuit) voltage of whatever drives the stage, Rs
// its source impedance plus any series resistor before the leak node. The
// grid current flows back through Rgs into n and charges Cc: that is the
// bias shift (the grid's DC point walks negative under heavy drive and
// recovers with the Cc*(Rs+Rg) time constant -> blocking distortion).
// Cc = 0 means a DC-coupled source (vs is then an absolute voltage).
//
// Discretisation: trapezoidal companions for Cc and Ck (SPICE's default
// rule). Per sample, the stage reduces exactly to two unknowns
// u = Vgk, w = Vpk (the cathode voltage is eliminated in closed form) and a
// damped 2x2 Newton-Raphson solves them, predicted from the last sample.
// The stage works in volts; its output is the plate voltage MINUS the
// quiescent plate voltage (an AC deviation, inverted as on the real tube).

#include "Filters.h"

#include <array>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace jb::dsp
{
namespace triode
{
/** 12AX7 model constants (Koren plate law, Cohen/Helie grid law). */
struct Tube
{
    double mu = 100.0, ex = 1.4, kg1 = 1060.0, kp = 600.0, kvb = 300.0;
    double gg = 6.062e-4, xi = 1.354, cg = 13.9, ig0 = 3.917e-8;
};

inline const Tube& ax7() noexcept { static const Tube t; return t; }

/** Fast exp / log for the per-sample solver (relative error < 2e-9 over the
    ranges the stage uses; std:: versions dominate the cost otherwise). */
inline double fastExp (double x) noexcept
{
    x = std::clamp (x, -700.0, 700.0);
    const auto t = x * 1.4426950408889634;           // log2(e)
    const auto n = (t + 6755399441055744.0) - 6755399441055744.0; // round to nearest (no SSE4.1 floor call)
    const auto f = (t - n) * 0.6931471805599453;      // |f| <= ln2/2
    // e^f, degree-7 Taylor (|f| <= 0.347: relative error < 2e-9)
    const auto f2 = f * f;
    auto p = (1.0 + f) + f2 * ((0.5 + f * (1.0 / 6.0)) + f2 * ((1.0 / 24.0 + f * (1.0 / 120.0))
               + f2 * (1.0 / 720.0 + f * (1.0 / 5040.0))));
    std::int64_t bits;
    std::memcpy (&bits, &p, sizeof bits);
    bits += (std::int64_t) n << 52;
    std::memcpy (&p, &bits, sizeof p);
    return p;
}

inline double fastLog (double x) noexcept
{
    std::int64_t bits;
    std::memcpy (&bits, &x, sizeof bits);
    auto e = (int) ((bits >> 52) & 0x7ff) - 1023;
    bits = (bits & 0x000fffffffffffffLL) | 0x3ff0000000000000LL;
    double m;
    std::memcpy (&m, &bits, sizeof m);               // m in [1, 2)
    if (m > 1.4142135623730951) { m *= 0.5; ++e; }
    const auto s = (m - 1.0) / (m + 1.0), s2 = s * s; // |s| <= 0.172
    const auto s4 = s2 * s2; // |s| <= 0.172: error < 3e-10
    const auto l = 2.0 * s * ((1.0 + s2 * (1.0 / 3.0)) + s4 * ((1.0 / 5.0 + s2 * (1.0 / 7.0)) + s4 * (1.0 / 9.0)));
    return l + (double) e * 0.6931471805599453;
}

/** ln(1 + e^x) without overflow, and its derivative (logistic). */
inline double softplus (double x) noexcept
{
    if (x > 30.0) return x;
    if (x < -30.0) return std::exp (x);
    return std::log1p (std::exp (x));
}

/** Plate current (A) and its partial derivatives, Koren:
    E1 = Vpk/kp * ln(1 + exp(kp*(1/mu + Vgk/sqrt(kvb + Vpk^2))))
    Ip = 2 * E1^ex / kg1  (E1 > 0), else 0. */
inline double plateCurrent (const Tube& t, double vgk, double vpk, double& dIdVgk, double& dIdVpk) noexcept
{
    dIdVgk = dIdVpk = 0.0;
    if (vpk <= 0.0) return 0.0;
    const auto s = std::sqrt (t.kvb + vpk * vpk);
    const auto z = t.kp * (1.0 / t.mu + vgk / s);
    double L, sig;
    if (z > 30.0)       { L = z; sig = 1.0; }
    else if (z < -30.0) { const auto e = fastExp (z); L = e; sig = e; }
    else                { const auto e = fastExp (z); L = fastLog (1.0 + e); sig = e / (1.0 + e); }
    const auto e1 = vpk / t.kp * L;
    if (e1 <= 1.0e-12) return 0.0;
    const auto pw = fastExp ((t.ex - 1.0) * fastLog (e1)); // e1^(ex-1)
    const auto ip = 2.0 * pw * e1 / t.kg1;
    const auto dIde1 = 2.0 * t.ex * pw / t.kg1;
    const auto de1du = vpk * sig / s;
    const auto de1dw = L / t.kp - sig * vgk * vpk * vpk / (s * s * s);
    dIdVgk = dIde1 * de1du;
    dIdVpk = dIde1 * de1dw;
    return ip;
}

/** Grid current (A) into the grid (out of the cathode) and dIg/dVgk. */
inline double gridCurrent (const Tube& t, double vgk, double& dIdVgk) noexcept
{
    const auto z = t.cg * vgk;
    double L, sig;
    if (z > 30.0)       { L = z; sig = 1.0; }
    else if (z < -12.0) { dIdVgk = 0.0; return t.ig0; } // < 2e-12 A above Ig0
    else                { const auto e = fastExp (z); L = fastLog (1.0 + e); sig = e / (1.0 + e); }
    const auto base = L / t.cg;
    const auto pw = fastExp ((t.xi - 1.0) * fastLog (base)); // base^(xi-1)
    dIdVgk = t.gg * t.xi * pw * sig;
    return t.gg * pw * base + t.ig0;
}

/** Component values of one stage (ohms, farads, volts). */
struct StageParts
{
    double b   = 390.0;    // supply node feeding Ra
    double ra  = 100.0e3;  // plate load
    double rk  = 1.8e3;    // cathode resistor
    double ck  = 1.0e-6;   // cathode bypass (0 = unbypassed)
    double cc  = 22.0e-9;  // coupling cap (0 = DC-coupled source)
    double rs  = 100.0e3;  // source impedance + series R before the leak node
    double rg  = 1.0e6;    // grid leak
    double rgs = 1.0e3;    // grid stopper (leak node -> grid)
};

/** DC operating point (volts / amps). */
struct OperatingPoint { double vg = 0, vk = 0, vp = 0, ip = 0, ig = 0; };

/** Quiescent point of a stage with vs = 0 (caps open). */
inline OperatingPoint solveOperatingPoint (const StageParts& p, const Tube& t = ax7()) noexcept
{
    // grid network Thevenin at DC: caps open
    const auto rthDc = (p.cc > 0.0 ? p.rg : (p.rs * p.rg) / (p.rs + p.rg)) + p.rgs;
    double u = -1.5, w = p.b * 0.6;
    for (int it = 0; it < 200; ++it)
    {
        double dIpu, dIpw, dIgu;
        const auto ip = plateCurrent (t, u, w, dIpu, dIpw);
        const auto ig = gridCurrent (t, u, dIgu);
        const auto k = (ip + ig) * p.rk;
        const auto dku = (dIpu + dIgu) * p.rk, dkw = dIpw * p.rk;
        // grid: vg = -rth*ig ; vg = u + k
        const auto r1 = u + k + rthDc * ig;
        const auto r2 = p.b - w - k - p.ra * ip;
        const auto j11 = 1.0 + dku + rthDc * dIgu, j12 = dkw;
        const auto j21 = -dku - p.ra * dIpu, j22 = -1.0 - dkw - p.ra * dIpw;
        const auto det = j11 * j22 - j12 * j21;
        auto du = (r1 * j22 - r2 * j12) / det;
        auto dw = (j11 * r2 - j21 * r1) / det;
        du = std::clamp (du, -0.5, 0.5); dw = std::clamp (dw, -50.0, 50.0);
        u -= du; w -= dw;
        if (w < 0.0) w = 0.0;
        if (std::abs (du) < 1.0e-12 && std::abs (dw) < 1.0e-9) break;
    }
    double a, b, c;
    OperatingPoint op;
    op.ip = plateCurrent (t, u, w, a, b);
    op.ig = gridCurrent (t, u, c);
    op.vk = (op.ip + op.ig) * p.rk;
    op.vg = u + op.vk;
    op.vp = w + op.vk;
    return op;
}
} // namespace triode

/** One solved triode stage, per-channel state. Runs at the oversampled rate. */
class TriodeStage
{
public:
    void prepare (double sampleRate, const triode::StageParts& parts, const triode::Tube& tube = triode::ax7()) noexcept
    {
        fs = sampleRate; p = parts; t = tube;
        const auto h = 1.0 / fs;
        rc  = p.cc > 0.0 ? h / (2.0 * p.cc) : 0.0;
        gck = p.ck > 0.0 ? 2.0 * p.ck / h : 0.0;
        gkt = 1.0 / p.rk + gck;
        gb  = 1.0 / (p.rs + rc);
        gn  = gb + 1.0 / p.rg;
        rth = 1.0 / gn + p.rgs;
        op = triode::solveOperatingPoint (p, t);
        reset();
    }

    /** Back to the quiescent point (caps charged to their DC voltages). */
    void reset() noexcept
    {
        for (auto& s : st)
        {
            s.u = s.u1 = op.vg - op.vk; s.w = s.w1 = op.vp - op.vk; s.k = op.vk; s.ick = 0.0;
            // DC: grid-leak node n = vg + rgs*ig, cap holds vs - n with vs = 0
            const auto n = op.vg + p.rgs * op.ig;
            s.vc = p.cc > 0.0 ? -n : 0.0;
            s.ic = 0.0;
        }
        lastIt = 0;
    }

    /** vs: source voltage (deviation for AC-coupled stages). Returns the plate
        voltage deviation from the quiescent point. */
    inline double process (double vs, int ch) noexcept
    {
        auto& s = st[(size_t) ch];
        const auto ib  = p.cc > 0.0 ? gb * (s.vc + rc * s.ic) : 0.0;
        const auto vth = (gb * vs - ib) / gn;
        const auto jck = gck * s.k + s.ick;

        // predictor: linear extrapolation of the last two solutions
        auto u = 2.0 * s.u - s.u1, w = 2.0 * s.w - s.w1;
        if (w < 0.5 * s.w) w = 0.5 * s.w;
        double ip = 0, ig = 0, k = s.k;
        int it = 0;
        for (; it < 50; ++it)
        {
            double dIpu, dIpw, dIgu;
            ip = triode::plateCurrent (t, u, w, dIpu, dIpw);
            ig = triode::gridCurrent (t, u, dIgu);
            k = (ip + ig + jck) / gkt;
            const auto dku = (dIpu + dIgu) / gkt, dkw = dIpw / gkt;
            const auto r1 = u + k - vth + rth * ig;
            const auto r2 = p.b - w - k - p.ra * ip;
            const auto j11 = 1.0 + dku + rth * dIgu, j12 = dkw;
            const auto j21 = -dku - p.ra * dIpu, j22 = -1.0 - dkw - p.ra * dIpw;
            const auto det = j11 * j22 - j12 * j21;
            auto du = (r1 * j22 - r2 * j12) / det;
            auto dw = (j11 * r2 - j21 * r1) / det;
            // step limiting: only where the grid conducts (exponential/power-law
            // region) is a long step risky; from deep cut-off, jump straight to
            // the conduction knee instead of crawling there 2 V at a time
            {
                const auto un = u - du;
                if (un > u + 2.0) du = u - std::max (u + 2.0, std::min (un, 0.5));
                else if (un < u - 2.0 && u > 0.5) du = u - std::max (un, std::min (u - 2.0, 0.5));
            }
            dw = std::clamp (dw, -100.0, 100.0);
            if (w - dw < 0.0) dw = 0.5 * w;           // approach w = 0 by halving (plate law is degenerate there)
            u -= du; w -= dw;
            if (std::abs (du) < 1.0e-6 && std::abs (dw) < 1.0e-4)
            {
                // converged: carry the currents to the final point linearly
                // (the step is tiny; saves a full model evaluation)
                ip -= dIpu * du + dIpw * dw;
                ig -= dIgu * du;
                k = (ip + ig + jck) / gkt;
                ++it;
                break;
            }
        }
        lastIt = it;
        // update the companions
        const auto n = vth - (ig / gn);           // leak node: n = (gb vs - ib - ig)/gn
        if (p.cc > 0.0)
        {
            const auto i = gb * (vs - n) - ib;     // branch current through Cc
            s.vc += rc * (i + s.ic);
            s.ic = i;
        }
        s.ick = gck * k - jck;
        s.k = k; s.u1 = s.u; s.w1 = s.w; s.u = u; s.w = w;
        return (w + k) - op.vp;
    }

    inline float process (float vs, int ch) noexcept { return (float) process ((double) vs, ch); }

    const triode::OperatingPoint& operatingPoint() const noexcept { return op; }
    const triode::StageParts& parts() const noexcept { return p; }
    int lastIterations() const noexcept { return lastIt; }
    /** Current grid-to-cathode voltage (the bias, incl. bias shift). */
    double gridBias (int ch) const noexcept { return st[(size_t) ch].u; }
    /** Coupling-cap voltage deviation from quiescent (bias shift), volts. */
    double couplingShift (int ch) const noexcept
    {
        const auto n = op.vg + p.rgs * op.ig;
        return p.cc > 0.0 ? st[(size_t) ch].vc + n : 0.0;
    }
    /** Small-signal mid-band voltage gain (numerical, plate deviation / vs). */
    double smallSignalGain() const noexcept
    {
        TriodeStage probe = *this;
        probe.reset();
        // midband: drive a 1 mV step after the cap transient is irrelevant
        // (rc ~ 0 at the oversampled rate and Rg >> Rs for most stages);
        // use a symmetric +-1 mV evaluation of the static solve.
        const auto up = probe.process (1.0e-3, 0);
        probe.reset();
        const auto dn = probe.process (-1.0e-3, 0);
        return (up - dn) / 2.0e-3;
    }

    /** Small-signal cathode-bypass shelf of this stage, rad/s: the stage gain
        rises from the degenerated value below wz = 1/(Rk Ck) to the bypassed
        value above wp = wz (1 + (mu+1) Rk / (Ra + rp)). wz = wp = 0 when the
        cathode is unbypassed (no shelf: the gain is degenerated everywhere). */
    struct Shelf { double wz = 0, wp = 0; };
    /** Fraction (in dB) of the cathode-bypass bass loss that ShelfEq gives back.
        0 = the bare circuit (Rev3 WIP: 40 Hz 7-10 dB under Rev2.4), 1 = fully
        cancelled (40 Hz 2-5 dB over Rev2.4 once the stages saturate). 0.5 puts
        the driven 40/80/100 Hz response within ~1 dB of Rev2.4 on Red v2 and the
        5150 (juicyboi_bassprobe, docs/triode.md "Low end"). */
    static constexpr double kShelfCompensation = 0.5;
    Shelf cathodeShelf() const noexcept
    {
        if (p.ck <= 0.0 || p.rk <= 0.0) return {};
        double gm, gp;
        triode::plateCurrent (t, op.vg - op.vk, op.vp - op.vk, gm, gp);
        const auto rp = 1.0 / gp, mu = gm * rp;
        const auto wz = 1.0 / (p.rk * p.ck);
        return { wz, wz * std::pow (1.0 + (mu + 1.0) * p.rk / (p.ra + rp), kShelfCompensation) };
    }

private:
    struct State { double u = 0, w = 0, u1 = 0, w1 = 0, k = 0, ick = 0, vc = 0, ic = 0; };
    triode::StageParts p;
    triode::Tube t;
    triode::OperatingPoint op;
    double fs = 192000.0, rc = 0, gck = 0, gkt = 1, gb = 1, gn = 1, rth = 1;
    std::array<State, kMaxChannels> st {};
    int lastIt = 0;
};

/** Rev3 voicing EQ: first-order low shelf (s + wp) / (s + wz), unity at high
    frequencies, wp/wz at DC. Placed before a solved stage it cancels that
    stage's small-signal cathode-bypass shelf, so the voiced coupling high-
    passes (Rev2.x) keep owning the cascade's low end while the stage keeps its
    nonlinear cathode dynamics. docs/triode.md, "Low end". */
class ShelfEq
{
public:
    void setup (double wz, double wp, double fs) noexcept
    {
        on = wz > 0.0 && wp > wz;
        if (! on) return;
        // bilinear transform with the zero/pole prewarped
        const auto k = 2.0 * fs;
        const auto pz = 2.0 * fs * std::tan (wz / k), pp = 2.0 * fs * std::tan (wp / k);
        const auto a0 = k + pz;
        b0 = (k + pp) / a0; b1 = (pp - k) / a0; a1 = (pz - k) / a0;
        reset();
    }
    void reset() noexcept { for (auto& z : x1) z = 0.0; for (auto& z : y1) z = 0.0; }
    inline double process (double x, int ch) noexcept
    {
        if (! on) return x;
        const auto c = (size_t) ch;
        const auto y = b0 * x + b1 * x1[c] - a1 * y1[c];
        x1[c] = x; y1[c] = y;
        return y;
    }
private:
    bool on = false;
    double b0 = 1, b1 = 0, a1 = 0;
    std::array<double, kMaxChannels> x1 {}, y1 {};
};
} // namespace jb::dsp
