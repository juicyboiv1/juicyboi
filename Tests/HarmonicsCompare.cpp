// Harmonic-spectrum / THD probe for A/B'ing preamp revisions at matched knobs.
// usage: juicyboi_harmonics [spectrum-dir|-] [input-scale]
// Renders a 110 Hz sine (two input levels) through the full processor with
// Cab/EQ/Gate/Boost off and default tone knobs, for Rectifier Red v1/v2,
// Orange and 5150 Lead at Gain 3/5/7/10, and prints level, THD and the low
// harmonics. The same file builds against any revision (parameter IDs are
// stable since state v3), which is how Rev1 vs Rev2 numbers are produced.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Parameters.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
void setParam (juce::AudioProcessor& p, const char* id, float plain)
{
    auto& proc = dynamic_cast<JuicyboiAudioProcessor&> (p);
    if (auto* param = proc.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (plain));
}

struct Result { double levelDb, thdPct, thdDb; double h[41]; int dense; double hfDb; };

Result measure (int model, int channel, int redVoice, float gain, double amp, const char* dumpPath)
{
    std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
    const double sr = 48000.0; const int bs = 512;
    p->setPlayConfigDetails (1, 2, sr, bs);
    setParam (*p, jb::params::id::preampModel, (float) model);
    setParam (*p, jb::params::id::channel, (float) channel);
    setParam (*p, jb::params::id::redVoice, (float) redVoice);
    setParam (*p, jb::params::id::gain, gain);
    setParam (*p, jb::params::id::cabEnable, 0.0f);
    setParam (*p, jb::params::id::eqEnable, 0.0f);
    setParam (*p, jb::params::id::gateEnable, 0.0f);
    setParam (*p, jb::params::id::odEnable, 0.0f);
    p->prepareToPlay (sr, bs);

    const double f0 = 110.0;
    const int settle = (int) sr, n = 48000; // 1 s settle, 1 s analysis (110 whole cycles)
    std::vector<float> out;
    juce::AudioBuffer<float> buf (2, bs);
    juce::MidiBuffer midi;
    long t = 0;
    while ((int) out.size() < settle + n)
    {
        buf.clear();
        for (int i = 0; i < bs; ++i, ++t)
            buf.setSample (0, i, (float) (amp * std::sin (2.0 * juce::MathConstants<double>::pi * f0 * (double) t / sr)));
        p->processBlock (buf, midi);
        for (int i = 0; i < bs; ++i) out.push_back (buf.getSample (0, i));
    }
    std::vector<double> y (out.begin() + settle, out.begin() + settle + n);

    // Exact-bin DFT (110 cycles in the window): harmonic k sits on bin 110k
    Result r {};
    double ms = 0; for (auto v : y) ms += v * v;
    r.levelDb = 10.0 * std::log10 (ms / n + 1e-30);
    for (int k = 1; k <= 40; ++k)
    {
        double re = 0, im = 0;
        const double w = 2.0 * juce::MathConstants<double>::pi * f0 * k / sr;
        for (int i = 0; i < n; ++i) { re += y[(size_t) i] * std::cos (w * i); im -= y[(size_t) i] * std::sin (w * i); }
        r.h[k] = 2.0 * std::sqrt (re * re + im * im) / n;
    }
    double hs = 0, hf = 0;
    for (int k = 2; k <= 40; ++k)
    {
        hs += r.h[k] * r.h[k];
        if (k * f0 >= 2000.0) hf += r.h[k] * r.h[k];
        if (r.h[k] > r.h[1] * 0.01) ++r.dense; // within 40 dB of H1
    }
    r.thdPct = 100.0 * std::sqrt (hs) / r.h[1];
    r.thdDb = 20.0 * std::log10 (std::sqrt (hs) / r.h[1]);
    r.hfDb = 10.0 * std::log10 (hf / (r.h[1] * r.h[1]) + 1e-30); // harmonics >= 2 kHz re H1
    if (dumpPath != nullptr)
        if (auto* f = std::fopen (dumpPath, "w"))
        {
            for (int k = 1; k <= 40; ++k) std::fprintf (f, "%d %.9e\n", k, r.h[k]);
            std::fclose (f);
        }
    return r;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    struct Cfg { const char* name; int model, channel, redVoice; };
    const Cfg cfgs[] = { { "RedV2", 0, 0, 1 }, { "RedV1", 0, 0, 0 }, { "Orange", 0, 1, 0 }, { "Lead", 1, 0, 1 } };
    const float gains[] = { 3.0f, 5.0f, 7.0f, 10.0f };
    const double amps[] = { 0.05, 0.25 };
    const double scale = argc > 2 ? std::atof (argv[2]) : 1.0; // probe: input scale (pre-gain experiments)
    std::printf ("cfg     gain  in   level_dBFS  THD%%    THDdB  H2    H3    H4    H5    H7    H9   >=2kHz  n(-40dBc)\n");
    const char* only = argc > 3 ? argv[3] : nullptr; // optional: one config name
    for (const auto& c : cfgs)
        for (auto g : gains)
            for (auto a : amps)
            {
                if (only != nullptr && std::strcmp (only, c.name) != 0) continue;
                char path[512]; const char* dump = nullptr;
                if (argc > 1 && argv[1][0] != '-') { std::snprintf (path, sizeof path, "%s/%s-g%g-a%g.txt", argv[1], c.name, (double) g, a); dump = path; }
                const auto r = measure (c.model, c.channel, c.redVoice, g, a * scale, dump);
                auto dbc = [&] (int k) { return 20.0 * std::log10 (r.h[k] / r.h[1] + 1e-12); };
                std::printf ("%-7s %4.0f %5.2f %8.2f %8.1f %7.2f %5.1f %5.1f %5.1f %5.1f %5.1f %5.1f %7.1f %4d\n",
                             c.name, (double) g, a, r.levelDb, r.thdPct, r.thdDb, dbc (2), dbc (3), dbc (4), dbc (5), dbc (7), dbc (9), r.hfDb, r.dense);
            }
    return 0;
}
