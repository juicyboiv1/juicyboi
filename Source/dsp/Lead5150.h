#pragma once

#include "Filters.h"
#include "SoftClip.h"
#include "ToneStack.h"

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

/** Lead passive TMB, read off the Peavey EVH 5150 schematic, sheet 3 (Tone
    Stack/PI), https://www.prowessamplifiers.com/schematics/post/peavey_evh5150.pdf:
      VR5 HIGH 250K (B = linear), C14 470pF || C60 100pF (= 570 pF treble cap),
      R28 47K (slope), C18 .022uF (bass), C21 .022uF (mid), VR3 LOW 1M
      (A = audio, wired as rheostat), VR4 MID 50K (B = linear).
    Not modelled: the R65 47K branch from the Low/Mid junction, the R24 33K /
    C59-R94 loading at the V5A drive node and the load on the treble wiper
    (ideal source, unloaded output). makeupDb only restores the Rev1 Lead level
    at default knobs. */
inline TmbComponents leadTmb()
{
    return { 250e3, 1e6, 50e3, 47e3, 570e-12, 22e-9, 22e-9, 2.80 };
}

/** Model-specific passive TMB after the Lead cascade (solved network, see
    ToneStack.h). Ideal source/unloaded output: the plate's source impedance and
    the following stage's load are not modelled. No PI, power amp, NFB
    presence/resonance (those are LeadPresence / LeadResonance). */
class Lead5150Tone
{
public:
    Lead5150Tone() { stack.setComponents (leadTmb()); }

    void setKnobs (float bass, float mid, float treble) noexcept
    {
        stack.setKnobs (std::clamp (bass, 0.0f, 10.0f), std::clamp (mid, 0.0f, 10.0f),
                        std::clamp (treble, 0.0f, 10.0f));
    }
    void prepare (double sampleRate) noexcept { stack.prepare (sampleRate); }
    void snap() noexcept { stack.snap(); }
    void beginFrame() noexcept { stack.beginFrame(); }
    float process (float x, int ch) noexcept { return stack.process (x, ch); }

    const PassiveToneStack& passive() const noexcept { return stack; }

private:
    PassiveToneStack stack;
};
} // namespace jb::dsp
