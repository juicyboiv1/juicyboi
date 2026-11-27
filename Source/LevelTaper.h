#pragma once

#include <algorithm>
#include <cmath>

/** Amp-style 0–10 scales for the Input and Master knobs (since state version 2).

    Input Cal (input_trim): linear in dB, 4.8 dB per number. Shared by both
    amps as an interface-level calibration (not a per-amp faceplate knob).
        dB = -24 + 4.8 * x            0 = -24 dB, 5 = 0 dB (unity), 10 = +24 dB

    Master (master): smooth audio taper with a hard mute at 0.
        t  = x / 10
        dB = -60 + (9224 t - 2352 t^2 - 320 t^3) / 91      for x > 0
        gain = 0 (silence)                                  for x = 0
        The cubic is monotonic (slope ~10 dB/number at the bottom, ~4 at the top)
        and passes exactly through 6.5 = -6 dB (default), 7.5 = 0 dB (unity)
        and 10 = +12 dB.

    Cab Level (cab_level): identical to Input.
        dB = -24 + 4.8 * x            5 = 0 dB (unity, default)

    Boost Level (od_level): linear in dB, 3.6 dB per number.
        dB = -24 + 3.6 * x            0 = -24 dB, 20/3 (6.67) = 0 dB (unity, default),
                                      10 = +12 dB
        20/3 is not exact in float, so |dB| < 1e-4 snaps to exactly 0 dB (a tiny
        unity detent) — the default stays bit-identical to the old 0 dB.

    The old dB parameters (state versions 1 and 2) map onto these exactly, so legacy
    sessions are converted in setStateInformation. Pure functions, no JUCE. */
namespace jb::taper
{
// 1 = Input/Master/Boost Level/Cab Level stored in dB
// 2 = Input/Master are 0–10 knobs; Boost Level/Cab Level still dB
// 3 = all four are 0–10 knobs
// 4 = Rev2.4 per-amp settings (the 5150 set is copied from the legacy IDs on load).
//     input_trim is the shared input calibration; m5150_input_trim is unused.
inline constexpr int stateVersion = 4; // Rev2.4: per-amp settings (5150 set migrated from the legacy IDs)

inline constexpr float inputDefaultKnob  = 5.0f; // 0 dB  (old default)
inline constexpr float masterDefaultKnob = 6.5f; // -6 dB (old default)
inline constexpr float cabLevelDefaultKnob   = 5.0f;             // 0 dB (old default)
inline constexpr float boostLevelDefaultKnob = 20.0f / 3.0f;     // 0 dB (old default)

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

//== Cab Level (same law as Input) =============================================
inline double cabLevelKnobToDb (double x) noexcept { return inputKnobToDb (x); }
inline double cabLevelDbToKnob (double dB) noexcept { return inputDbToKnob (dB); }
inline float  cabLevelKnobToGain (float x) noexcept { return inputKnobToGain (x); }

//== Boost Level ================================================================
inline double boostLevelKnobToDb (double x) noexcept
{
    const auto dB = -24.0 + 3.6 * std::clamp (x, 0.0, 10.0);
    return std::abs (dB) < 1.0e-4 ? 0.0 : dB; // unity detent (20/3 is inexact in float)
}

inline double boostLevelDbToKnob (double dB) noexcept
{
    return std::clamp ((dB + 24.0) / 3.6, 0.0, 10.0);
}

inline float boostLevelKnobToGain (float x) noexcept
{
    return std::pow (10.0f, (float) boostLevelKnobToDb (x) * 0.05f);
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
