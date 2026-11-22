#pragma once

#include "Filters.h"
#include "ChannelVoice.h"

#include <array>
#include <cmath>
#include <complex>

namespace jb::dsp
{
/** Power-amp / speaker-load interaction, low-frequency part (Rev2.3, Red v2).

    juicyboi has no power-amp model and a cab IR is normally captured with a
    low-impedance (solid-state) amp, so the bass bump a tube amp makes into a
    real 4x12 is missing: the amp's finite output impedance Zo and the
    cabinet's impedance peak at its in-box resonance form a divider
        H = [Zs / (Zs + Zo)] / [Re / (Re + Zo)]          (1 at DC and HF)
    with the speaker's low-frequency impedance (voice-coil inductance
    ignored; this is the LF part only)
        Zs(s) = Re + Res || L || C,  w0 = 1/sqrt(LC),  Q = Res sqrt(C/L)
              = Re + Res (s/(w0 Q)) / (s^2/w0^2 + s/(w0 Q) + 1).
    Substituting gives an exact second-order peaking section with the same w0:
        H(s) = (s^2/w0^2 + s/(w0 Qn) + 1) / (s^2/w0^2 + s/(w0 Qd) + 1),
        Qn = Q / (1 + Res/Re),   Qd = Q / (1 + Res/(Re + Zo)),
        peak |H(w0)| = (1 + Res/Re) / (1 + Res/(Re + Zo)).
    Bilinear transform prewarped at w0, double-precision coefficients and
    state (the corner sits ~600x below the oversampled rate). res = 0 is an
    exact bypass. Linear and time-invariant: it adds no harmonics or IMD.
    Values and sources: voicings::rectoSpeakerLoad(), docs/red-v2-bass.md. */
class SpeakerLoad
{
public:
    void prepare (double sampleRate, const SpeakerLoadParams& p) noexcept
    {
        enabled = p.res > 0.0;
        if (enabled)
        {
            const auto qn = p.q / (1.0 + p.res / p.re);
            const auto qd = p.q / (1.0 + p.res / (p.re + p.zout));
            const auto u  = 1.0 / std::tan (kPi * p.f0 / sampleRate); // s/w0 -> u (1-z^-1)/(1+z^-1)
            const auto a0 = u * u + u / qd + 1.0;
            b0 = (u * u + u / qn + 1.0) / a0;
            b1 = 2.0 * (1.0 - u * u) / a0;
            b2 = (u * u - u / qn + 1.0) / a0;
            a1 = 2.0 * (1.0 - u * u) / a0;
            a2 = (u * u - u / qd + 1.0) / a0;
        }
        reset();
    }

    void reset() noexcept { z1.fill (0.0); z2.fill (0.0); }

    /** Analytic |H(f)| of the configured network (for tests / docs). */
    static double magnitude (const SpeakerLoadParams& p, double f) noexcept
    {
        if (p.res <= 0.0) return 1.0;
        const auto x = f / p.f0;
        const std::complex<double> s (0.0, x), d = s * s + s / p.q + 1.0;
        const auto zs = p.re + p.res * (s / p.q) / d;
        return std::abs (zs / (zs + p.zout)) / (p.re / (p.re + p.zout));
    }

    inline float process (float xf, int ch) noexcept
    {
        if (! enabled) return xf;
        const auto x = (double) xf;
        auto& s1 = z1[(size_t) ch];
        auto& s2 = z2[(size_t) ch];
        const auto y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return (float) y;
    }

private:
    bool enabled = false;
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    std::array<double, kMaxChannels> z1 {}, z2 {};
};
} // namespace jb::dsp
