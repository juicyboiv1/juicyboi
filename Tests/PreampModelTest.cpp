// Rev3.1: rect-rev3.1.f32 was captured ONCE from the Rev3.1 build (component-level
// Boost tone/level) and every configuration must null against it. Boost-off
// configurations must ALSO null against rect-rev3.f32 (the amp path is unchanged),
// and boosted ones stay within 1 dB of Rev3.
// Rectifier regression fixture rect-rev3.f32: captured ONCE from the Rev3 build
// (solved triode stages; same render as the original 348fc68 capture). Rev3
// deliberately changes the audio, so the Rev2.x fixtures are level references:
// rect-rev2.3.f32 is what Rev2.3/Rev2.4 rendered (the Rectifier was
// bit-identical in Rev2.4) and every voice/boost must stay within 1.5 dB of it.
// rect-rev2.2-clippers.f32 / rect-rev2-tonestack.f32 / legacy-348fc68.f32 are
// kept for history and ToneStackTest. Do not regenerate a fixture from feature
// code to bless a failure.
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
    juce::File file (juce::String (JUICYBOI_TEST_DIR) + "/rect-rev3.f32");
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
    const char* names[] = { "Red v1", "Red v2", "Orange" };
    const char* boosts[] = { "Boost off", "OD-808", "SD-1" };
    const size_t per = 32 * 256;
    juce::File file31 (juce::String (JUICYBOI_TEST_DIR) + "/rect-rev3.1.f32");
    if (std::getenv ("JB_CAPTURE_REV31") != nullptr && ! file31.existsAsFile())
    {
        juce::FileOutputStream os (file31);
        for (auto v : result) os.writeFloat (v);
    }
    auto in = file31.createInputStream();
    auto in3 = file.createInputStream();
    float error = 0.0f, errorOff = 0.0f;
    bool finite = in != nullptr && in3 != nullptr && in->getTotalLength() == (juce::int64) (result.size() * sizeof (float))
                  && in3->getTotalLength() == in->getTotalLength();
    bool rev3LevelOk = finite;
    for (size_t c = 0; finite && c < 9; ++c)
    {
        double a = 0, b = 0;
        for (size_t i = 0; i < per; ++i)
        {
            const auto y = result[c * per + i];
            const auto r31 = in->readFloat(), r3 = in3->readFloat();
            finite = finite && std::isfinite (y) && std::isfinite (r31);
            error = std::max (error, std::abs (y - r31));
            if (c % 3 == 0) errorOff = std::max (errorOff, std::abs (y - r3));
            if (i >= per / 2) { a += (double) y * y; b += (double) r3 * r3; }
        }
        if (c % 3 != 0)
        {
            const auto d = 10.0 * std::log10 (a / b);
            std::printf ("Rev3.1 vs Rev3 level: %-6s %-9s %+.2f dB\n", names[c / 3], boosts[c % 3], d);
            rev3LevelOk = rev3LevelOk && std::abs (d) < 1.0;
        }
    }
    std::printf ("Rectifier Rev3.1 fixture null: %zu samples, max error %.9g %s\n",
                 result.size(), (double) error, finite && error < 2.0e-6f ? "ok" : "FAIL");
    std::printf ("Rectifier Boost-off vs Rev3 fixture (bit-identical): max error %.9g %s\n",
                 (double) errorOff, finite && ! (errorOff > 0.0f) ? "ok" : "FAIL");
    std::printf ("Rectifier boosted configs within 1 dB of Rev3: %s\n", rev3LevelOk ? "ok" : "FAIL");

    // Level vs Rev2.4 (rect-rev2.3.f32: the Rectifier was bit-identical in Rev2.4)
    bool levelOk = true;
    {
        juce::File old (juce::String (JUICYBOI_TEST_DIR) + "/rect-rev2.3.f32");
        auto ref = old.createInputStream();
        levelOk = ref != nullptr;
        for (size_t c = 0; ref && c < 9; ++c)
        {
            double a = 0, b = 0;
            for (size_t i = 0; i < per; ++i)
            {
                const auto r = (double) ref->readFloat();
                if (i >= per / 2) { a += (double) result[c * per + i] * result[c * per + i]; b += r * r; }
            }
            const auto d = 10.0 * std::log10 (a / b);
            std::printf ("Rev3 vs Rev2.4 level: %-6s %-9s %+.2f dB\n", names[c / 3], boosts[c % 3], d);
            levelOk = levelOk && std::abs (d) < 1.5;
        }
    }
    std::printf ("Rev3 level within 1.5 dB of Rev2.4 for every Rectifier voice/boost: %s\n", levelOk ? "ok" : "FAIL");
    return finite && error < 2.0e-6f && ! (errorOff > 0.0f) && rev3LevelOk && levelOk ? 0 : 1;
}
