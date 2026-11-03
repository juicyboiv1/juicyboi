#pragma once

// Small, allocation-free filter primitives used by the juicyboi DSP stages.
// Per-channel state is held inline (kMaxChannels) so stages can be called
// sample-by-sample from the audio thread without any heap activity.

#include <array>
#include <cmath>
#include <algorithm>

namespace jb::dsp
{
inline constexpr int kMaxChannels = 2;
inline constexpr double kPi = 3.14159265358979323846;

/** Tolerance-based change check (avoids -Wfloat-equal on cached params). */
template <typename T>
inline bool differs (T a, T b) noexcept { return std::abs (a - b) > (T) 1.0e-6; }

/** Clamp a cutoff so the bilinear transform stays well-behaved. */
inline double safeCutoff (double hz, double sampleRate) noexcept
{
    return std::clamp (hz, 5.0, 0.45 * sampleRate);
}

//==============================================================================
/** One-pole (6 dB/oct) filter. Good stand-in for single RC sections such as
    coupling caps (HP) or Miller / bypass caps (LP). Matched via bilinear
    transform with prewarping. */
class OnePole
{
public:
    enum class Type { lowPass, highPass };

    void setup (Type t, double cutoffHz, double sampleRate) noexcept
    {
        type = t;
        const auto fc = safeCutoff (cutoffHz, sampleRate);
        const auto k  = std::tan (kPi * fc / sampleRate);
        // y = b0*x + b1*x[n-1] - a1*y[n-1]
        if (type == Type::lowPass)
        {
            b0 = (float) (k / (1.0 + k));
            b1 = b0;
        }
        else
        {
            b0 = (float) (1.0 / (1.0 + k));
            b1 = -b0;
        }
        a1 = (float) ((k - 1.0) / (k + 1.0));
    }

    void reset() noexcept { x1.fill (0.0f); y1.fill (0.0f); }

    inline float process (float x, int ch) noexcept
    {
        const auto y = b0 * x + b1 * x1[(size_t) ch] - a1 * y1[(size_t) ch];
        x1[(size_t) ch] = x;
        y1[(size_t) ch] = y;
        return y;
    }

private:
    Type type = Type::lowPass;
    float b0 = 1.0f, b1 = 0.0f, a1 = 0.0f;
    std::array<float, kMaxChannels> x1 {}, y1 {};
};

//==============================================================================
/** RBJ-cookbook biquad, transposed direct form II. */
struct BiquadCoeffs
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;

    static BiquadCoeffs normalise (double b0, double b1, double b2,
                                   double a0, double a1, double a2) noexcept
    {
        return { (float) (b0 / a0), (float) (b1 / a0), (float) (b2 / a0),
                 (float) (a1 / a0), (float) (a2 / a0) };
    }

    static BiquadCoeffs lowPass (double fs, double f, double q) noexcept
    {
        const auto w = 2.0 * kPi * safeCutoff (f, fs) / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + al, -2 * cw, 1 - al);
    }

    static BiquadCoeffs highPass (double fs, double f, double q) noexcept
    {
        const auto w = 2.0 * kPi * safeCutoff (f, fs) / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al);
    }

    static BiquadCoeffs peak (double fs, double f, double q, double gainDb) noexcept
    {
        const auto A = std::pow (10.0, gainDb / 40.0);
        const auto w = 2.0 * kPi * safeCutoff (f, fs) / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise (1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A);
    }

    static BiquadCoeffs lowShelf (double fs, double f, double q, double gainDb) noexcept
    {
        const auto A = std::pow (10.0, gainDb / 40.0);
        const auto w = 2.0 * kPi * safeCutoff (f, fs) / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        const auto sA = 2.0 * std::sqrt (A) * al;
        return normalise (A * ((A + 1) - (A - 1) * cw + sA),
                          2 * A * ((A - 1) - (A + 1) * cw),
                          A * ((A + 1) - (A - 1) * cw - sA),
                          (A + 1) + (A - 1) * cw + sA,
                          -2 * ((A - 1) + (A + 1) * cw),
                          (A + 1) + (A - 1) * cw - sA);
    }

    static BiquadCoeffs highShelf (double fs, double f, double q, double gainDb) noexcept
    {
        const auto A = std::pow (10.0, gainDb / 40.0);
        const auto w = 2.0 * kPi * safeCutoff (f, fs) / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
        const auto sA = 2.0 * std::sqrt (A) * al;
        return normalise (A * ((A + 1) + (A - 1) * cw + sA),
                          -2 * A * ((A - 1) + (A + 1) * cw),
                          A * ((A + 1) + (A - 1) * cw - sA),
                          (A + 1) - (A - 1) * cw + sA,
                          2 * ((A - 1) - (A + 1) * cw),
                          (A + 1) - (A - 1) * cw - sA);
    }
};

class Biquad
{
public:
    void setCoeffs (const BiquadCoeffs& newCoeffs) noexcept { c = newCoeffs; }
    void reset() noexcept { z1.fill (0.0f); z2.fill (0.0f); }

    inline float process (float x, int ch) noexcept
    {
        auto& s1 = z1[(size_t) ch];
        auto& s2 = z2[(size_t) ch];
        const auto y = c.b0 * x + s1;
        s1 = c.b1 * x - c.a1 * y + s2;
        s2 = c.b2 * x - c.a2 * y;
        return y;
    }

private:
    BiquadCoeffs c;
    std::array<float, kMaxChannels> z1 {}, z2 {};
};
} // namespace jb::dsp
