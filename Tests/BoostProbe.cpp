// Rev3.1 OD-808 compression / clarity probe.
// usage: juicyboi_boostprobe
//  A) pedal alone (192 kHz, Tone 5, Level 0 dB): output level change for a
//     20 dB input change (330 Hz sine 0.05 -> 0.5 V; smaller = more compressed)
//     and two-tone IMD (E2 + B2, 0.15 V each; off-grid products re f1+f2).
//  B) the same pedal feeding the amp (Red v2 / 5150 Lead, default knobs, cab/EQ/
//     gate off): the pedal is rendered at 192 kHz, decimated to 48 kHz and fed to
//     the processor with its own Boost off. The "plugin OD-808" row uses the
//     processor's own Boost, as a check of that offline path.
//  Variants: the stock TS-808 values and NON-STOCK mods (measured only; none is
//  in the plugin): 2+1 asymmetric 1N914, 2+2 1N914, red-LED pair, bass mod C3 0.1 uF.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Parameters.h"
#include "dsp/Od808Stage.h"
#include <cmath>
#include <complex>
#include <cstdio>
#include <functional>
#include <vector>

extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
constexpr double sr = 48000.0, fsHi = 192000.0, twoPi = 6.283185307179586;
using Fn = std::function<double (double)>; // input voltage at time t [s]

void setParam (juce::AudioProcessor& p, const char* id, float plain)
{
    auto& proc = dynamic_cast<JuicyboiAudioProcessor&> (p);
    if (auto* param = proc.apvts.getParameter (id)) param->setValueNotifyingHost (param->convertTo0to1 (plain));
}

struct Variant { const char* name; jb::dsp::ClipperComponents c; };

std::vector<Variant> variants()
{
    auto s = jb::dsp::Od808Stage::clipperComponents();
    std::vector<Variant> v { { "stock TS-808 (1N914 pair)", s } };
    auto a = s; a.diodesPos = 2; a.diodesNeg = 1;           v.push_back ({ "mod: 2+1 asymmetric 1N914", a });
    auto b = s; b.diodesPos = 2; b.diodesNeg = 2;           v.push_back ({ "mod: 2+2 1N914", b });
    auto l = s; l.is = 7.0e-18; l.n = 2.0;                  v.push_back ({ "mod: red LED pair", l });
    auto m = s; m.cLeg = 0.1e-6;                            v.push_back ({ "mod: bass C3 0.1 uF", m });
    return v;
}

// whole OD-808 stage with a given clipper, at 192 kHz
std::vector<double> pedal (const jb::dsp::ClipperComponents& cc, float drive, const Fn& in, long nHi)
{
    jb::dsp::OnePole hp; hp.setup (jb::dsp::OnePole::Type::highPass, 1.0 / (twoPi * 10e3 * 1e-6), fsHi);
    jb::dsp::OpAmpDiodeClipper clip; clip.setComponents (cc); clip.prepare (fsHi); clip.setDrive (drive); clip.snap(); clip.beginFrame();
    jb::dsp::PedalPostStage post;
    post.prepare (fsHi, jb::dsp::Od808Stage::toneComponents(), jb::dsp::Od808Stage::outputComponents(), jb::dsp::Od808Stage::makeup);
    post.setTargets (0.5f, 1.0f); post.snap();
    std::vector<double> y ((size_t) nHi);
    for (long i = 0; i < nHi; ++i)
    {
        clip.beginFrame(); post.beginFrame();
        const auto c = clip.process ((double) hp.process ((float) in ((double) i / fsHi), 0), 0);
        y[(size_t) i] = (double) post.process ((float) c, 0);
    }
    return y;
}

std::vector<double> decimate4 (const std::vector<double>& x)
{
    constexpr int taps = 127; std::vector<double> h (taps); double sum = 0;
    for (int k = 0; k < taps; ++k)
    {
        const double m = k - (taps - 1) / 2.0, fc = 20500.0 / fsHi;
        const double sinc = m == 0.0 ? 2 * fc : std::sin (twoPi * fc * m) / (jb::dsp::kPi * m);
        h[(size_t) k] = sinc * (0.42 - 0.5 * std::cos (twoPi * k / (taps - 1)) + 0.08 * std::cos (2 * twoPi * k / (taps - 1)));
        sum += h[(size_t) k];
    }
    std::vector<double> y (x.size() / 4);
    for (size_t j = 0; j < y.size(); ++j)
    {
        double a = 0; const long c = (long) (4 * j);
        for (int k = 0; k < taps; ++k) { const long i = c - k + (taps - 1) / 2; if (i >= 0 && i < (long) x.size()) a += h[(size_t) k] * x[(size_t) i]; }
        y[j] = a / sum;
    }
    return y;
}

struct Amp { const char* name; int model; };

std::vector<double> amp (const Amp& a, const std::vector<double>& in, bool pluginOd = false, float drive = 0.2f)
{
    std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
    const int bs = 512;
    p->setPlayConfigDetails (1, 2, sr, bs);
    setParam (*p, jb::params::id::preampModel, (float) a.model);
    if (a.model == 1) setParam (*p, jb::params::id::m5150Channel, 1.0f);
    else setParam (*p, jb::params::id::redVoice, 1.0f);
    for (auto* id : { "cab_enable", "eq_enable", "gate_enable", "m5150_cab_enable", "m5150_eq_enable", "m5150_gate_enable" }) setParam (*p, id, 0.0f);
    const char* odE = a.model == 1 ? "m5150_od_enable" : "od_enable";
    const char* odD = a.model == 1 ? "m5150_od_drive" : "od_drive";
    setParam (*p, odE, pluginOd ? 1.0f : 0.0f);
    setParam (*p, odD, 10.0f * drive);
    p->prepareToPlay (sr, bs);
    std::vector<double> out; juce::AudioBuffer<float> buf (2, bs); juce::MidiBuffer midi; size_t t = 0;
    while (out.size() < in.size())
    {
        buf.clear();
        for (int i = 0; i < bs; ++i, ++t) buf.setSample (0, i, t < in.size() ? (float) in[t] : 0.0f);
        p->processBlock (buf, midi);
        for (int i = 0; i < bs; ++i) out.push_back (buf.getSample (0, i));
    }
    out.resize (in.size());
    return out;
}

double rmsDb (const std::vector<double>& y, size_t a)
{
    double s = 0; for (size_t i = a; i < y.size(); ++i) s += y[i] * y[i];
    return 10.0 * std::log10 (s / (double) (y.size() - a) + 1e-30);
}

double imdDb (const std::vector<double>& y, double fs, size_t a) // off-grid products re f1 + f2 (g = 41.25 Hz)
{
    const double g = 41.25; double ref = 0, im = 0;
    auto pw = [&] (double f) { double re = 0, imv = 0; const double w = twoPi * f / fs; const size_t n = y.size() - a;
                               for (size_t i = 0; i < n; ++i) { re += y[a + i] * std::cos (w * (double) i); imv -= y[a + i] * std::sin (w * (double) i); }
                               return (re * re + imv * imv) / ((double) n * (double) n); };
    ref = pw (2 * g) + pw (3 * g);
    for (int k = 1; k * g < 6000.0; ++k) if (k % 2 != 0 && k % 3 != 0) im += pw (k * g);
    return 10.0 * std::log10 (im / ref);
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const Fn sine05 = [] (double t) { return 0.05 * std::sin (twoPi * 330.0 * t); };
    const Fn sine5 = [] (double t) { return 0.5 * std::sin (twoPi * 330.0 * t); };
    const Fn two = [] (double t) { return 0.15 * std::sin (twoPi * 82.5 * t) + 0.15 * std::sin (twoPi * 123.75 * t); };
    const Fn lo05 = [] (double t) { return 0.05 * std::sin (twoPi * 110.0 * t); };
    const Fn lo5 = [] (double t) { return 0.5 * std::sin (twoPi * 110.0 * t); };
    const long nHi = (long) (0.8 * fsHi);  // 0.8 s; 32 cycles of 41.25 Hz in the last 0.7757 s
    const size_t aHi = (size_t) (nHi - (long) std::lround (32.0 / 41.25 * fsHi));
    std::printf ("A) pedal alone, Tone 5, Level 0 dB\n%-28s %6s  %22s  %18s\n", "variant", "Drive", "dOut for +20 dB in", "2-tone IMD [dB]");
    for (const auto& v : variants())
        for (float d : { 0.2f, 0.5f })
        {
            const auto lo = rmsDb (pedal (v.c, d, sine05, nHi), (size_t) (0.3 * fsHi));
            const auto hi = rmsDb (pedal (v.c, d, sine5, nHi), (size_t) (0.3 * fsHi));
            const auto im = imdDb (pedal (v.c, d, two, nHi), fsHi, aHi);
            std::printf ("%-28s %6.0f  %8.1f dB (out %+.1f dBV) %12.1f\n", v.name, 10 * d, hi - lo, hi, im);
        }

    const Amp amps[] = { { "Red v2", 0 }, { "5150 Lead", 1 } };
    const long n = (long) (0.8 * sr);
    const size_t a = (size_t) (n - (long) std::lround (32.0 / 41.25 * sr));
    auto sampled = [&] (const Fn& f) { std::vector<double> x ((size_t) n); for (long i = 0; i < n; ++i) x[(size_t) i] = f ((double) i / sr); return x; };
    std::printf ("\nB) into the amp (default knobs), 110 Hz sine 0.05 -> 0.5 V and 2-tone E2+B2 0.15 V each\n");
    std::printf ("%-10s %-30s %6s %20s %16s\n", "amp", "front end", "Drive", "dOut for +20 dB in", "2-tone IMD [dB]");
    for (const auto& am : amps)
    {
        {
            const auto lo = rmsDb (amp (am, sampled (lo05)), (size_t) (0.3 * sr)), hi = rmsDb (amp (am, sampled (lo5)), (size_t) (0.3 * sr));
            std::printf ("%-10s %-30s %6s %17.1f dB %16.1f\n", am.name, "no boost", "-", hi - lo, imdDb (amp (am, sampled (two)), sr, a));
        }
        for (float d : { 0.2f })
        {
            const auto lo = rmsDb (amp (am, sampled (lo05), true, d), (size_t) (0.3 * sr)), hi = rmsDb (amp (am, sampled (lo5), true, d), (size_t) (0.3 * sr));
            std::printf ("%-10s %-30s %6.0f %17.1f dB %16.1f\n", am.name, "plugin OD-808 (check)", 10 * d, hi - lo, imdDb (amp (am, sampled (two), true, d), sr, a));
        }
        for (const auto& v : variants())
            for (float d : { 0.0f, 0.2f, 0.5f })
            {
                auto run = [&] (const Fn& f) { return amp (am, decimate4 (pedal (v.c, d, f, n * 4))); };
                const auto lo = rmsDb (run (lo05), (size_t) (0.3 * sr)), hi = rmsDb (run (lo5), (size_t) (0.3 * sr));
                std::printf ("%-10s %-30s %6.0f %17.1f dB %16.1f\n", am.name, v.name, 10 * d, hi - lo, imdDb (run (two), sr, a));
            }
    }
    return 0;
}
