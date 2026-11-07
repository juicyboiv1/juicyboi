#pragma once

#include "Filters.h"
#include "SoftClip.h"

namespace jb::dsp
{
/** Schematic-informed Lead voice, NOT a component/tube solver.
    Stage order: V1A, V1B, V2A (39k cold cathode), V2B, V5B, V5A.
    Filter frequencies and normalized gains below are behavioral tuning, not
    unloaded 1/(2*pi*RC) claims. Loaded networks and supply operating points
    need a circuit solve for accurate physical values. Keep legacy four-stage
    ChannelVoicing/PreampCascade entirely independent. Runs at 4x sample rate. */
class Lead5150
{
public:
    void setGain (float value) noexcept
    {
        target = 0.012f * std::pow (1.0f / 0.012f, std::clamp (value, 0.0f, 1.0f));
    }

    void prepare (double fs) noexcept
    {
        smoothing = (float) (1.0 - std::exp (-1.0 / (0.020 * fs)));
        pot = target;
        for (size_t i = 0; i < stages.size(); ++i)
        {
            hp[i].setup (OnePole::Type::highPass, stages[i].hp, fs);
            lp[i].setup (OnePole::Type::lowPass, stages[i].lp, fs);
            hp[i].reset(); lp[i].reset();
        }
        dc.setup (OnePole::Type::highPass, 12.0, fs); dc.reset();
        air.setup (OnePole::Type::lowPass, 8200.0, fs); air.reset();
    }

    // Advance once per sample FRAME, not per channel: stereo has identical gain.
    void beginFrame() noexcept { pot += smoothing * (target - pot); }

    float process (float x, int ch) noexcept
    {
        for (size_t i = 0; i < stages.size(); ++i)
        {
            const auto& s = stages[i];
            x = hp[i].process (x, ch);
            x = -shape::triode (x * s.drive, s.bias, s.positive, s.negative);
            x = lp[i].process (x, ch) * s.attenuation;
            if (i == 0) x *= pot; // Lead pre-gain after initial stage
        }
        // Six inversions: same nominal polarity as the four-stage Rectifier.
        return dc.process (air.process (x, ch) * 0.72f, ch);
    }

private:
    struct Stage { double hp, lp; float drive, bias, positive, negative, attenuation; };
    inline static constexpr std::array<Stage, 6> stages {{
        { 42.0, 12000.0, 3.8f, 0.04f, 1.10f, 0.92f, 0.72f }, // V1A
        { 85.0, 10500.0, 7.0f, 0.05f, 1.15f, 0.90f, 0.48f }, // V1B
        { 55.0,  8500.0, 4.2f, 0.38f, 3.20f, 0.72f, 0.68f }, // V2A cold asymmetry
        { 110.0, 7200.0, 7.0f, 0.07f, 1.20f, 0.90f, 0.42f }, // V2B
        { 65.0,  9500.0, 4.4f, 0.03f, 1.10f, 0.95f, 0.60f }, // V5B
        { 38.0,  8000.0, 3.0f, 0.02f, 1.05f, 0.95f, 1.00f }  // V5A
    }};
    std::array<OnePole, 6> hp, lp;
    OnePole dc, air;
    float target = 0.14f, pot = 0.14f, smoothing = 0.001f;
};

/** Model-specific plate-fed TMB approximation. The fixed broad mid contour and
    three controls are behavioral biquads, NOT an exact interacting passive stack.
    No PI, power amp, NFB presence/resonance, loop or recovery-stage simulation. */
class Lead5150Tone
{
public:
    void setKnobs (float bass, float mid, float treble) noexcept
    {
        targets = { std::clamp (bass, 0.0f, 10.0f), std::clamp (mid, 0.0f, 10.0f),
                    std::clamp (treble, 0.0f, 10.0f) };
    }
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        smooth = (float) (1.0 - std::exp (-1.0 / (0.025 * fs)));
        knobs = targets; last = { -100.0f, -100.0f, -100.0f }; counter = 0;
        contour.setCoeffs (BiquadCoeffs::peak (fs, 620.0, 0.65, -3.0));
        contour.reset(); bassF.reset(); midF.reset(); trebleF.reset(); update();
    }
    void beginFrame() noexcept
    {
        for (size_t i = 0; i < knobs.size(); ++i) knobs[i] += smooth * (targets[i] - knobs[i]);
        if (++counter == 16) { counter = 0; update(); }
    }
    float process (float x, int ch) noexcept
    {
        return trebleF.process (midF.process (bassF.process (contour.process (x, ch), ch), ch), ch);
    }
private:
    void update() noexcept
    {
        if (std::abs (knobs[0] - last[0]) + std::abs (knobs[1] - last[1])
            + std::abs (knobs[2] - last[2]) < 0.0001f) return;
        last = knobs;
        bassF.setCoeffs (BiquadCoeffs::lowShelf (fs, 130.0, 0.707, (knobs[0] - 5.0) * 1.8));
        midF.setCoeffs (BiquadCoeffs::peak (fs, 760.0, 0.72, (knobs[1] - 5.0) * 2.0));
        trebleF.setCoeffs (BiquadCoeffs::highShelf (fs, 2300.0, 0.707, (knobs[2] - 5.0) * 1.8));
    }
    double fs = 48000.0;
    float smooth = 0.001f;
    int counter = 0;
    std::array<float, 3> targets { 5.0f, 4.0f, 5.5f }, knobs {}, last {};
    Biquad contour, bassF, midF, trebleF;
};
} // namespace jb::dsp
