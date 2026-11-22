#pragma once

#include "Filters.h"
#include "SoftClip.h"

#include <cmath>
#include <algorithm>

namespace jb::dsp
{
/** Post-tone-stack recovery stage (Rev2.2).

    Both reference amps follow their passive TMB with more tube gain before
    the power section: the Dual Rectifier's FX-loop send/return recovery
    triodes and phase inverter, the 5150's effects-loop/phase-inverter
    triodes. Rev2.1 had no nonlinearity after the stack, so the stack's real
    insertion loss and mid scoop were only made up with clean gain, and the
    output carried ~2-3 dB less harmonic energy (re the fundamental) than
    Rev1's behavioural EQ at the same knobs.

    This is a behavioural stand-in for that recovery gain, not a solved tube
    stage: coupling HP -> asymmetric triode curve (shape::triode) driven by
    `drive`, normalised to UNITY small-signal gain so it adds compression and
    harmonics after the stack without changing the clean level. drive = 0 is
    an exact bypass. The drive is fixed per channel (it stands in for a typical
    Master/loop setting; the real stage would track the Master knob).
    Runs inside the 4x oversampled region. */
class RecoveryStage
{
public:
    struct Settings
    {
        float drive = 0.0f, bias = 0.10f, positive = 1.30f, negative = 1.00f;
        bool  mirror = false;   // flip the curve's asymmetry (sets which half flattens first)
        float outGain = 1.0f;   // fixed level trim after the stage
        double hpHz = 12.0;
    };

    void prepare (double sampleRate, const Settings& s) noexcept
    {
        cfg = s;
        hp.setup (OnePole::Type::highPass, s.hpHz, sampleRate);
        hp.reset();
        // small-signal slope of shape::triode at the bias point (positive branch for bias >= 0)
        const auto t = std::tanh ((double) s.bias * (double) (s.bias >= 0.0f ? s.positive : s.negative));
        invSlope = (float) (1.0 / (1.0 - t * t));
        drive = s.drive;
    }

    /** Per-sample drive (e.g. crossfaded between channels). */
    inline float process (float x, float k, int ch) noexcept
    {
        const auto u = hp.process (x, ch);
        if (k < 1.0e-3f)
            return u;
        // outGain belongs to the configured drive; fade it with k so a channel
        // crossfade towards a clean (k = 0) voicing ends at exactly unity.
        const auto g = cfg.drive > 0.0f ? 1.0f + (cfg.outGain - 1.0f) * std::min (k / cfg.drive, 1.0f) : 1.0f;
        const auto v = cfg.mirror ? -u : u;
        const auto y = shape::triode (v * k, cfg.bias, cfg.positive, cfg.negative) * (invSlope / k);
        return (cfg.mirror ? -y : y) * g;
    }

    inline float process (float x, int ch) noexcept { return process (x, drive, ch); }

    float getDrive() const noexcept { return drive; }

private:
    Settings cfg;
    OnePole hp;
    float invSlope = 1.0f, drive = 0.0f;
};
} // namespace jb::dsp
