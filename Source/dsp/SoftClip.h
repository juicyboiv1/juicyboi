#pragma once

// Static nonlinearities ("waveshapers") used by the circuit-principle stages.
// These are memoryless approximations; dynamic effects (bias shift, blocking,
// supply sag) are TODOs for later phases.

#include <cmath>

namespace jb::dsp::shape
{
/** Plain tanh soft clip. */
inline float tanhClip (float x) noexcept { return std::tanh (x); }

/** Smooth symmetric "diode pair" limiter.
    y = x / (1 + |x|^n)^(1/n)
    n controls the knee: ~2 is a soft silicon-pair feel, higher n approaches
    a hard clip. Output is bounded to (-1, 1). Input should be pre-scaled by
    1/Vf (diode forward voltage) by the caller. */
inline float diodePair (float x, float knee = 2.5f) noexcept
{
    const auto ax = std::abs (x);
    return x / std::pow (1.0f + std::pow (ax, knee), 1.0f / knee);
}

/** Asymmetric triode-like transfer curve.
    - bias moves the operating point (asymmetry -> even harmonics)
    - posHardness > negHardness models the positive half (grid conduction)
      flattening sooner than the negative half (cutoff) — a common
      qualitative description of a common-cathode 12AX7-style stage.
    The static DC offset of the bias point is removed so silence maps to 0.
    Stage polarity inversion is handled by the caller. */
inline float triode (float x, float bias, float posHardness, float negHardness) noexcept
{
    auto curve = [=] (float u) noexcept
    {
        return u >= 0.0f ? std::tanh (u * posHardness) / posHardness
                         : std::tanh (u * negHardness) / negHardness;
    };

    return curve (x + bias) - curve (bias);
}
/** Asymmetric "3-diode" soft clip (1 diode one polarity, 2 series the other).
    posScale / negScale set the relative clip thresholds (e.g. 1.0 / 2.0).
    Input should be pre-scaled by 1/Vf by the caller. Output roughly (-negScale, +posScale). */
inline float diodeAsym3 (float x, float posScale = 1.0f, float negScale = 2.0f,
                         float knee = 2.6f) noexcept
{
    const auto s = x >= 0.0f ? posScale : negScale;
    const auto u = x / s;
    const auto au = std::abs (u);
    return s * u / std::pow (1.0f + std::pow (au, knee), 1.0f / knee);
}

} // namespace jb::dsp::shape
