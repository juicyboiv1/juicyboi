#pragma once
// Rev3.3 bypass fixture. rev3.2.2-bypass.f32 was captured ONCE from the v3.2.2
// build (commit 297dbd4) with exactly these renders; the scenarios only touch
// parameters that exist in v3.2.2, so with the PITCH pedal off (its default)
// every later build must reproduce them bit-for-bit (PitchShiftTest.cpp).
// Kept header-only so the identical code can be compiled against old trees.
#include "PluginProcessor.h"
#include <cmath>
#include <cstdint>
#include <vector>

namespace jbtest
{
inline constexpr double bypassSr = 48000.0;
inline constexpr int bypassBlock = 256;
inline constexpr int bypassSamples = 24000; // 0.5 s per scenario, channel 0
inline constexpr int bypassConfigs = 4;

/** Guitar-like input: plucked E2/B2/E3 power chord with decaying partials, a
    re-pluck at 0.25 s, plus low-level LCG noise. Deterministic. */
inline std::vector<float> bypassInput()
{
    std::vector<float> x ((size_t) bypassSamples);
    std::uint32_t lcg = 12345u;
    const double f0[] = { 82.41, 123.47, 164.81 };
    for (int i = 0; i < bypassSamples; ++i)
    {
        const double t = i / bypassSr;
        const double tp = t < 0.25 ? t : t - 0.25;
        double s = 0.0;
        for (double f : f0)
            for (int h = 1; h <= 6; ++h)
                s += std::sin (2.0 * 3.14159265358979323846 * f * h * t) * std::exp (-tp * (3.0 + h)) / (h * 1.5);
        lcg = lcg * 1664525u + 1013904223u;
        const double noise = ((double) (lcg >> 8) / 16777216.0 - 0.5) * 2.0e-3;
        x[(size_t) i] = (float) (0.12 * s * std::min (1.0, tp * 400.0) + noise);
    }
    return x;
}

inline void bypassSet (JuicyboiAudioProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

/** Scenario c (0..bypassConfigs-1): v3.2.2 parameters only. */
inline void bypassConfigure (JuicyboiAudioProcessor& p, int c)
{
    switch (c)
    {
        case 0: // Rectifier Red v2, cab on (no IR) + gate
            bypassSet (p, "cab_enable", 1.0f);
            bypassSet (p, "gate_enable", 1.0f);
            break;
        case 1: // Orange + SD-1 + GEQ-7 + cab filters
            bypassSet (p, "channel", 1.0f);
            bypassSet (p, "od_enable", 1.0f);
            bypassSet (p, "od_type", 1.0f);
            bypassSet (p, "eq_enable", 1.0f);
            bypassSet (p, "eq_400", -6.0f);
            bypassSet (p, "eq_1k6", 4.0f);
            bypassSet (p, "cab_enable", 1.0f);
            bypassSet (p, "cab_hpf", 80.0f);
            bypassSet (p, "cab_lpf", 6000.0f);
            break;
        case 2: // 5150 Lead + OD-808 + gate + cab HPF
            bypassSet (p, "preamp_model", 1.0f);
            bypassSet (p, "m5150_od_enable", 1.0f);
            bypassSet (p, "m5150_gate_enable", 1.0f);
            bypassSet (p, "m5150_cab_enable", 1.0f);
            bypassSet (p, "m5150_cab_hpf", 100.0f);
            break;
        default: // 5150 Rhythm Crunch, Preamp Out
            bypassSet (p, "preamp_model", 1.0f);
            bypassSet (p, "m5150_channel", 0.0f);
            bypassSet (p, "m5150_crunch", 1.0f);
            bypassSet (p, "preamp_out", 1.0f);
            break;
    }
}

/** Renders scenario c through a fresh processor; returns channel 0. `extra`
    may set more parameters (e.g. PITCH settings with the pedal off). */
template <typename Extra>
inline std::vector<float> bypassRender (int c, Extra&& extra)
{
    JuicyboiAudioProcessor p;
    p.stopTunerThread();
    p.setPlayConfigDetails (1, 2, bypassSr, bypassBlock);
    bypassConfigure (p, c);
    extra (p);
    p.prepareToPlay (bypassSr, bypassBlock);
    const auto x = bypassInput();
    std::vector<float> y ((size_t) bypassSamples);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, bypassBlock);
    for (int s = 0; s < bypassSamples; s += bypassBlock)
    {
        const int n = std::min (bypassBlock, bypassSamples - s);
        buf.setSize (2, n, false, false, true);
        buf.clear();
        for (int i = 0; i < n; ++i) buf.setSample (0, i, x[(size_t) (s + i)]);
        p.processBlock (buf, midi);
        for (int i = 0; i < n; ++i) y[(size_t) (s + i)] = buf.getSample (0, i);
    }
    p.releaseResources();
    return y;
}

inline std::vector<float> bypassRender (int c) { return bypassRender (c, [] (JuicyboiAudioProcessor&) {}); }
} // namespace jbtest
