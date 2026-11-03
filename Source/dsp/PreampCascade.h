#pragma once

#include "Filters.h"
#include "SoftClip.h"
#include "ChannelVoice.h"

namespace jb::dsp
{
/** Cascaded triode-style preamp: N x [coupling HP -> gain -> asymmetric
    soft saturation (inverting) -> plate/Miller LP], with a Gain pot
    attenuator between stages. Voiced by a ChannelVoicing.

    Intended to run at the oversampled rate. */
class PreampCascade
{
public:
    void prepare (double sampleRate, const ChannelVoicing& voicing);
    void reset();

    /** gain01 in 0..1 (the "Gain" knob). Smoothed internally per sample. */
    void setGain (float gain01) noexcept;

    inline float process (float x, int ch) noexcept
    {
        potGain += potSmoothCoef * (potTarget - potGain);

        auto s = inputTight.process (x, ch);

        for (int i = 0; i < v.numStages; ++i)
        {
            const auto& st = v.stages[(size_t) i];
            s = stageHP[(size_t) i].process (s, ch);
            // Common-cathode stages invert polarity; alternating polarity
            // changes how each stage's asymmetry stacks with the next.
            s = -shape::triode (s * st.gain, st.bias, st.posHardness, st.negHardness);
            s = stageLP[(size_t) i].process (s, ch);

            if (i == v.gainPotAfterStage)
                s *= potGain;
        }

        s = postLP.process (s, ch);
        return dcBlock.process (s * v.makeup, ch);
    }

private:
    ChannelVoicing v;
    double fs = 44100.0;

    OnePole inputTight, postLP, dcBlock;
    std::array<OnePole, ChannelVoicing::maxStages> stageHP, stageLP;

    float potTarget = 0.5f, potGain = 0.5f, potSmoothCoef = 0.001f;
};
} // namespace jb::dsp
