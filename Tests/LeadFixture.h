#pragma once
// Deterministic renders shared by the 62a1239 capture and ResonanceTest.
// Fixture lead-62a1239.f32 was captured ONCE from unmodified commit 62a1239
// (before the FMV Bass-centre change and before lead_resonance existed).
// Do not regenerate it from feature code to bless a failure.
#include "PluginProcessor.h"
#include <vector>
#include <cmath>

namespace leadfixture
{
inline void setParam (JuicyboiAudioProcessor& p, const char* id, float v)
{
    if (auto* a = p.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v));
}

inline constexpr int numConfigs = 5, blocks = 32, blockSize = 256;

// 0: 5150 defaults                1: 5150 Presence 6, SD-1 on, Gain 8, Bass 7, EQ on
// 2: 5150 Presence 10, OD-808     3: Red v2, Bass 5, Mid 2, Treble 8, Presence 7, Gain 8
// 4: Orange, Bass 5, Mid 7, Treble 3, Presence 2, OD-808 on, EQ on
inline void configure (JuicyboiAudioProcessor& p, int config)
{
    switch (config)
    {
        case 0: setParam (p, "preamp_model", 1.0f); break;
        case 1: setParam (p, "preamp_model", 1.0f); setParam (p, "lead_presence", 6.0f);
                setParam (p, "od_enable", 1.0f); setParam (p, "od_type", 1.0f);
                setParam (p, "pre_gain", 8.0f); setParam (p, "pre_bass", 7.0f);
                setParam (p, "eq_enable", 1.0f); setParam (p, "eq_100", 4.0f); setParam (p, "eq_800", -3.0f); break;
        case 2: setParam (p, "preamp_model", 1.0f); setParam (p, "lead_presence", 10.0f);
                setParam (p, "od_enable", 1.0f); break;
        case 3: setParam (p, "pre_mid", 2.0f); setParam (p, "pre_treble", 8.0f);
                setParam (p, "pre_presence", 7.0f); setParam (p, "pre_gain", 8.0f); break;
        default: setParam (p, "channel", 1.0f); setParam (p, "pre_mid", 7.0f); setParam (p, "pre_treble", 3.0f);
                setParam (p, "pre_presence", 2.0f); setParam (p, "od_enable", 1.0f);
                setParam (p, "eq_enable", 1.0f); setParam (p, "eq_200", 5.0f); break;
    }
}

inline std::vector<float> render (int config)
{
    JuicyboiAudioProcessor p;
    configure (p, config);
    p.setPlayConfigDetails (1, 2, 48000.0, blockSize);
    p.prepareToPlay (48000.0, blockSize);
    juce::MidiBuffer midi;
    std::vector<float> out;
    for (int b = 0; b < blocks; ++b)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        for (int i = 0; i < blockSize; ++i)
        {
            const auto t = (double) (b * blockSize + i) / 48000.0;
            buffer.setSample (0, i, (float) (0.18 * std::sin (2.0 * jb::dsp::kPi * 82.4 * t)
                                                + 0.05 * std::sin (2.0 * jb::dsp::kPi * 41.2 * t)
                                                + 0.04 * std::sin (2.0 * jb::dsp::kPi * 733.0 * t)));
        }
        p.processBlock (buffer, midi);
        out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
    }
    return out;
}
} // namespace leadfixture
