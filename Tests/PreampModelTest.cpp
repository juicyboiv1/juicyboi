// Regression fixture captured with the unmodified 348fc68 processor, Release.
#include "PluginProcessor.h"
#include <cstdio>
#include <vector>
#include <cmath>

static void parameter (JuicyboiAudioProcessor& p, const char* id, float v)
{
    auto* a = p.apvts.getParameter (id);
    a->setValueNotifyingHost (a->convertTo0to1 (v));
}

int legacyRegression()
{
    juce::File file (juce::String (JUICYBOI_TEST_DIR) + "/legacy-348fc68.f32");
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
    std::printf ("Rectifier 348fc68 null: %zu samples, max error %.9g %s\n",
                 result.size(), (double) error, finite && error < 2.0e-6f ? "ok" : "FAIL");
    return finite && error < 2.0e-6f ? 0 : 1;
}
