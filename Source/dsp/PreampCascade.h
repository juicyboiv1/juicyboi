#pragma once

#include "Filters.h"
#include "SoftClip.h"
#include "ChannelVoice.h"
#include "Triode.h"
#include "TriodeCalibration.h"

namespace jb::dsp
{
/** Cascaded triode preamp: N x [coupling HP -> solved 12AX7 stage (Rev3:
    grid conduction, coupling-cap bias shift, inverting) -> plate/Miller LP],
    with a Gain pot attenuator between stages. Voiced by a ChannelVoicing.

    Intended to run at the oversampled rate. */
class PreampCascade
{
public:
    void prepare (double sampleRate, const ChannelVoicing& voicing);
    void reset();

    /** Diagnostics: the solved stage i (bias shift, iterations). */
    const TriodeStage& stage (int i) const noexcept { return tube[(size_t) i]; }

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
            // Rev3: the solved stage (volts inside; TriodeCalibration maps levels)
            if (v.behavioural) // Rev2.4 reference curve (tests / tools only)
                s = -shape::triode (s * st.gain, st.bias, st.posHardness, st.negHardness);
            else
                s = (float) (tube[(size_t) i].process (shelfEq[(size_t) i].process ((double) s, ch) * map[(size_t) i].inScale, ch) * map[(size_t) i].outScale);
            s = stageLP[(size_t) i].process (s, ch);

            if (i == v.gainPotAfterStage)
                s *= potGain;
        }

        s = postLP.process (s, ch);
        return dcBlock.process (s * (v.behavioural ? v.makeup : v.makeup * v.triodeMakeup), ch);
    }

private:
    ChannelVoicing v;
    double fs = 44100.0;

    OnePole inputTight, postLP, dcBlock;
    std::array<TriodeStage, ChannelVoicing::maxStages> tube;
    std::array<triode::StageMap, ChannelVoicing::maxStages> map {};
    std::array<ShelfEq, ChannelVoicing::maxStages> shelfEq {};
    std::array<OnePole, ChannelVoicing::maxStages> stageHP, stageLP;

    float potTarget = 0.5f, potGain = 0.5f, potSmoothCoef = 0.001f;
};
} // namespace jb::dsp
