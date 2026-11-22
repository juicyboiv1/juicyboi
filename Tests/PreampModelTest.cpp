// Rectifier regression fixture rect-rev2.2-clippers.f32: captured ONCE from the Rev2.2
// build (solved diode clippers + post-TMB recovery; same render as the original
// 348fc68 capture). rect-rev2-tonestack.f32 (Rev2.1) is kept as the level reference
// printed below; the pre-tone-stack legacy-348fc68.f32 is kept for ToneStackTest.
#include "PluginProcessor.h"
#include <cstdio>
#include <vector>
#include <cmath>
#include <cstdlib>

static void parameter (JuicyboiAudioProcessor& p, const char* id, float v)
{
    auto* a = p.apvts.getParameter (id);
    a->setValueNotifyingHost (a->convertTo0to1 (v));
}

int legacyRegression()
{
    juce::File file (juce::String (JUICYBOI_TEST_DIR) + "/rect-rev2.2-clippers.f32");
    std::vector<float> result;
    for (int voice = 0; voice < 3; ++voice)
        for (int boost = 0; boost < 3; ++boost)
        {
            JuicyboiAudioProcessor p;
            parameter (p, "channel", voice == 2 ? 1.0f : 0.0f);
            parameter (p, "red_voice", voice == 1 ? 1.0f : 0.0f);
            parameter (p, "od_enable", boost == 0 ? 0.0f : 1.0f);
            parameter (p, "od_type", boost == 2 ? 1.0f : 0.0f);
            p.setPlayConfigDetails (1, 2, 48000.0, 256);
            p.prepareToPlay (48000.0, 256);
            juce::MidiBuffer midi;
            for (int b = 0; b < 32; ++b)
            {
                juce::AudioBuffer<float> buffer (2, 256);
                buffer.clear();
                for (int i = 0; i < 256; ++i)
                {
                    const auto t = (double) (b * 256 + i) / 48000.0;
                    buffer.setSample (0, i, (float) (0.18 * std::sin (2.0 * jb::dsp::kPi * 82.4 * t)
                                                        + 0.04 * std::sin (2.0 * jb::dsp::kPi * 733.0 * t)));
                }
                p.processBlock (buffer, midi);
                result.insert (result.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 256);
            }
        }
    auto in = file.createInputStream();
    if (! in || in->getTotalLength() != (juce::int64) (result.size() * sizeof (float))) return 1;
    float error = 0.0f; bool finite = true;
    for (auto f : result)
    {
        const auto reference = in->readFloat();
        finite = finite && std::isfinite (f) && std::isfinite (reference);
        error = std::max (error, std::abs (f - reference));
    }
    std::printf ("Rectifier Rev2.2 fixture null: %zu samples, max error %.9g %s\n",
                 result.size(), (double) error, finite && error < 2.0e-6f ? "ok" : "FAIL");

    // Level vs the Rev2.1 render (rect-rev2-tonestack.f32, kept as reference)
    bool levelOk = true;
    {
        juce::File old (juce::String (JUICYBOI_TEST_DIR) + "/rect-rev2-tonestack.f32");
        auto ref = old.createInputStream();
        const char* names[] = { "Red v1", "Red v2", "Orange" };
        const char* boosts[] = { "Boost off", "OD-808", "SD-1" };
        const size_t per = 32 * 256;
        for (size_t c = 0; ref && c < 9; ++c)
        {
            double a = 0, b = 0;
            for (size_t i = 0; i < per; ++i)
            {
                const auto r = (double) ref->readFloat();
                if (i >= per / 2) { a += (double) result[c * per + i] * result[c * per + i]; b += r * r; }
            }
            const auto d = 10.0 * std::log10 (a / b);
            std::printf ("Rev2.2 vs Rev2.1 level: %-6s %-9s %+.2f dB\n", names[c / 3], boosts[c % 3], d);
            levelOk = levelOk && std::abs (d) < 1.5;
        }
    }
    std::printf ("Rev2.2 level within 1.5 dB of Rev2.1 for every Rectifier voice/boost: %s\n", levelOk ? "ok" : "FAIL");
    return finite && error < 2.0e-6f && levelOk ? 0 : 1;
}
