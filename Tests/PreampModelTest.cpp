// Rectifier regression fixture rect-rev2.3.f32: captured ONCE from the Rev2.3 build
// (Red v2 power-amp / 4x12 load bump; same render as the original 348fc68 capture).
// rect-rev2.2-clippers.f32 (Rev2.2) must still null for Red v1 and Orange: Rev2.3
// only touches Red v2. rect-rev2-tonestack.f32 (Rev2.1) is kept as the level
// reference printed below; the pre-tone-stack legacy-348fc68.f32 is kept for
// ToneStackTest. Do not regenerate a fixture from feature code to bless a failure.
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
    juce::File file (juce::String (JUICYBOI_TEST_DIR) + "/rect-rev2.3.f32");
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
    std::printf ("Rectifier Rev2.3 fixture null: %zu samples, max error %.9g %s\n",
                 result.size(), (double) error, finite && error < 2.0e-6f ? "ok" : "FAIL");

    const char* names[] = { "Red v1", "Red v2", "Orange" };
    const char* boosts[] = { "Boost off", "OD-808", "SD-1" };
    const size_t per = 32 * 256;

    // Rev2.3 changes Red v2 only: Red v1 and Orange still null against the Rev2.2
    // fixture; Red v2 gains low end (more level on this 82.4 Hz-heavy signal).
    bool othersOk = true, bassOk = true;
    {
        juce::File prev (juce::String (JUICYBOI_TEST_DIR) + "/rect-rev2.2-clippers.f32");
        auto ref = prev.createInputStream();
        othersOk = ref != nullptr && ref->getTotalLength() == (juce::int64) (result.size() * sizeof (float));
        for (size_t c = 0; othersOk && c < 9; ++c)
        {
            float err = 0.0f; double a = 0, b = 0;
            for (size_t i = 0; i < per; ++i)
            {
                const auto r = ref->readFloat(), x = result[c * per + i];
                err = std::max (err, std::abs (x - r));
                if (i >= per / 2) { a += (double) x * x; b += (double) r * r; }
            }
            const auto d = 10.0 * std::log10 (a / b);
            std::printf ("Rev2.3 vs Rev2.2: %-6s %-9s max error %.9g, level %+.2f dB\n", names[c / 3], boosts[c % 3], (double) err, d);
            if (c / 3 == 1) bassOk = bassOk && d > 0.3 && d < 3.0;
            else othersOk = othersOk && err < 2.0e-6f;
        }
    }
    std::printf ("Red v1 + Orange null vs the Rev2.2 fixture (unchanged): %s\n", othersOk ? "ok" : "FAIL");
    std::printf ("Red v2 louder than Rev2.2 by 0.3..3 dB on the low-E test signal (bass): %s\n", bassOk ? "ok" : "FAIL");

    // Level vs the Rev2.1 render (rect-rev2-tonestack.f32, kept as reference).
    // Red v2 allows 2.5 dB: Rev2.3 adds up to +3 dB at 80-110 Hz on purpose.
    bool levelOk = true;
    {
        juce::File old (juce::String (JUICYBOI_TEST_DIR) + "/rect-rev2-tonestack.f32");
        auto ref = old.createInputStream();
        for (size_t c = 0; ref && c < 9; ++c)
        {
            double a = 0, b = 0;
            for (size_t i = 0; i < per; ++i)
            {
                const auto r = (double) ref->readFloat();
                if (i >= per / 2) { a += (double) result[c * per + i] * result[c * per + i]; b += r * r; }
            }
            const auto d = 10.0 * std::log10 (a / b);
            std::printf ("Rev2.3 vs Rev2.1 level: %-6s %-9s %+.2f dB\n", names[c / 3], boosts[c % 3], d);
            levelOk = levelOk && std::abs (d) < (c / 3 == 1 ? 2.5 : 1.5);
        }
    }
    std::printf ("Rev2.3 level within 1.5 dB of Rev2.1 (Red v2: 2.5 dB) for every Rectifier voice/boost: %s\n", levelOk ? "ok" : "FAIL");
    return finite && error < 2.0e-6f && othersOk && bassOk && levelOk ? 0 : 1;
}
