#pragma once

#include <algorithm>
#include <cmath>

/** Amp-style 0–10 scales for the Input and Master knobs (since state version 2).

    Input  (input_trim): linear in dB, 4.8 dB per number.
        dB = -24 + 4.8 * x            0 = -24 dB, 5 = 0 dB (unity), 10 = +24 dB

    Master (master): smooth audio taper with a hard mute at 0.
        t  = x / 10
        dB = -60 + (9224 t - 2352 t^2 - 320 t^3) / 91      for x > 0
        gain = 0 (silence)                                  for x = 0
        The cubic is monotonic (slope ~10 dB/number at the bottom, ~4 at the top)
        and passes exactly through 6.5 = -6 dB (default), 7.5 = 0 dB (unity)
        and 10 = +12 dB.

    The old dB parameters (state version 1) map onto these exactly, so legacy
    sessions are converted in setStateInformation. Pure functions, no JUCE. */
namespace jb::taper
{
inline constexpr int stateVersion = 2; // 1 = Input/Master stored in dB

inline constexpr float inputDefaultKnob  = 5.0f; // 0 dB  (old default)
inline constexpr float masterDefaultKnob = 6.5f; // -6 dB (old default)

//== Input ======================================================================
inline double inputKnobToDb (double x) noexcept
{
    return -24.0 + 4.8 * std::clamp (x, 0.0, 10.0);
}

inline double inputDbToKnob (double dB) noexcept
{
    return std::clamp ((dB + 24.0) / 4.8, 0.0, 10.0);
}

inline float inputKnobToGain (float x) noexcept
{
    return std::pow (10.0f, (float) inputKnobToDb (x) * 0.05f);
}

//== Master =====================================================================
inline constexpr double masterMinDb = -60.0;
inline constexpr double masterMaxDb = 12.0;

/** dB for knob x in (0, 10]. x <= 0 returns -60 here, but is muted by masterKnobToGain. */
inline double masterKnobToDb (double x) noexcept
{
    const auto t = std::clamp (x, 0.0, 10.0) / 10.0;
    return masterMinDb + (9224.0 * t - 2352.0 * t * t - 320.0 * t * t * t) / 91.0;
}

/** Linear gain: exactly 0 at knob 0 (mute), else 10^(dB/20). */
inline float masterKnobToGain (float x) noexcept
{
    if (! (x > 0.0f))
        return 0.0f;
    return std::pow (10.0f, (float) masterKnobToDb (x) * 0.05f);
}

/** Inverse of masterKnobToDb. dB <= -60 → 0 (old -60 dB was already silent). */
inline double masterDbToKnob (double dB) noexcept
{
    if (! (dB > masterMinDb)) return 0.0;
    if (dB >= masterMaxDb)    return 10.0;

    double lo = 0.0, hi = 10.0; // monotonic → bisection to double precision
    for (int i = 0; i < 64; ++i)
    {
        const auto mid = 0.5 * (lo + hi);
        (masterKnobToDb (mid) < dB ? lo : hi) = mid;
    }
    return 0.5 * (lo + hi);
}

/** Gain the pre-v2 processor applied for a stored dB value (Decibels::decibelsToGain(dB, -60)). */
inline float legacyMasterGain (float dB) noexcept
{
    return dB > -60.0f ? std::pow (10.0f, dB * 0.05f) : 0.0f;
}

inline float legacyInputGain (float dB) noexcept
{
    return dB > -100.0f ? std::pow (10.0f, dB * 0.05f) : 0.0f;
}
} // namespace jb::taper
