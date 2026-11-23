#pragma once

// Rev3: maps a Rev2.x behavioural stage (drive, bias, hardness) onto a solved
// TriodeStage so the cascade keeps Rev2.4's tuned gain staging (how hard each
// stage is driven, its small-signal gain) while the curve, asymmetry, grid
// conduction and bias shift now come from the circuit. docs/triode.md §3.
//
//   x (normalised) -> * inScale -> TriodeStage (volts) -> * outScale -> y
//
//   inScale : the input level at which the solved stage's incremental gain has
//             fallen to half (first side to clip, mid-band, quiescent bias)
//             lands where the behavioural curve's slope had fallen to half.
//   outScale: the small-signal gain equals the behavioural slope at rest.

#include "Triode.h"

#include <cmath>
#include <algorithm>

namespace jb::dsp::triode
{
/** Mid-band static transfer: coupling caps shorted, bypass caps holding the
    quiescent cathode voltage (bypassed stages) or Rk solved (unbypassed).
    Returns the plate deviation for a grid-network input vin (volts). */
inline double midbandTransfer (const StageParts& p, const OperatingPoint& op, double vin, const Tube& t = ax7()) noexcept
{
    const auto d = p.rg / (p.rs + p.rg);
    const auto rth = p.rs * p.rg / (p.rs + p.rg) + p.rgs;
    // the cap holds the quiescent grid-node voltage: add it to the AC drive
    const auto vth = vin * d + (op.vg + rth * op.ig);
    const bool fixedK = p.ck > 0.0;
    double u = op.vg - op.vk, w = op.vp - op.vk;
    for (int it = 0; it < 200; ++it)
    {
        double dIpu, dIpw, dIgu;
        const auto ip = plateCurrent (t, u, w, dIpu, dIpw);
        const auto ig = gridCurrent (t, u, dIgu);
        const auto k  = fixedK ? op.vk : p.rk * (ip + ig);
        const auto dku = fixedK ? 0.0 : p.rk * (dIpu + dIgu), dkw = fixedK ? 0.0 : p.rk * dIpw;
        const auto r1 = u + k - vth + rth * ig;
        const auto r2 = p.b - w - k - p.ra * ip;
        const auto j11 = 1.0 + dku + rth * dIgu, j12 = dkw;
        const auto j21 = -dku - p.ra * dIpu, j22 = -1.0 - dkw - p.ra * dIpw;
        const auto det = j11 * j22 - j12 * j21;
        auto du = std::clamp ((r1 * j22 - r2 * j12) / det, -1.0, 1.0);
        auto dw = std::clamp ((j11 * r2 - j21 * r1) / det, -50.0, 50.0);
        if (w - dw < 0.0) dw = w;
        u -= du; w -= dw;
        if (std::abs (du) < 1.0e-11 && std::abs (dw) < 1.0e-9) break;
    }
    double a, b, c;
    const auto ip = plateCurrent (t, u, w, a, b);
    const auto ig = gridCurrent (t, u, c);
    const auto k  = fixedK ? op.vk : p.rk * (ip + ig);
    return (w + k) - op.vp;
}

struct StageClip { double gain = 1.0, pos = 1.0, neg = 1.0; };

/** Small-signal mid-band gain (magnitude) and the input levels (volts, > 0)
    at which the incremental gain falls to half, for positive and negative input. */
inline StageClip clipPoints (const StageParts& p, const Tube& t = ax7()) noexcept
{
    const auto op = solveOperatingPoint (p, t);
    const auto h = 1.0e-4;
    StageClip c;
    c.gain = std::abs (midbandTransfer (p, op, h, t) - midbandTransfer (p, op, -h, t)) / (2.0 * h);
    auto find = [&] (double sign)
    {
        double prev = 0.0;
        for (double a = 1.0e-3; a < 2000.0; a *= 1.02)
        {
            const auto g = std::abs (midbandTransfer (p, op, sign * (a * 1.01), t) - midbandTransfer (p, op, sign * a, t)) / (0.01 * a);
            if (g < 0.5 * c.gain) return 0.5 * (prev + a);
            prev = a;
        }
        return 2000.0;
    };
    c.pos = find (1.0);
    c.neg = find (-1.0);
    return c;
}

/** Behavioural Rev2.x stage y = -(c(g x + b) - c(b)), c(u) = tanh(H u)/H
    (H = pos for u >= 0, neg below): slope at rest and the input deviations at
    which the slope has fallen to half of it (same definition as clipPoints). */
inline StageClip behaviouralClip (double g, double b, double pos, double neg) noexcept
{
    auto slope = [=] (double x)
    {
        const auto u = g * x + b;
        const auto h = u >= 0.0 ? pos : neg;
        const auto th = std::tanh (h * u);
        return g * (1.0 - th * th);
    };
    StageClip c;
    c.gain = slope (0.0);
    auto find = [&] (double sign)
    {
        for (double a = 1.0e-5; a < 100.0; a *= 1.005)
            if (slope (sign * a) < 0.5 * c.gain) return a;
        return 100.0;
    };
    c.pos = find (1.0);
    c.neg = find (-1.0);
    return c;
}

struct StageMap { double inScale = 1.0, outScale = 1.0; };

/** drive > 1 pushes the solved stage harder than the behavioural one did
    (same small-signal gain). */
inline StageMap mapStage (const StageParts& p, double g, double b, double pos, double neg, double drive = 1.0) noexcept
{
    const auto phys = clipPoints (p);
    const auto bh = behaviouralClip (g, b, pos, neg);
    StageMap m;
    m.inScale = drive * std::min (phys.pos, phys.neg) / std::min (bh.pos, bh.neg);
    m.outScale = bh.gain / (m.inScale * phys.gain);
    return m;
}
} // namespace jb::dsp::triode
