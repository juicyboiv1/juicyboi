// Rev3 dynamics / bias-shift probe and CPU meter (full processor, 48 kHz,
// Cab/EQ/Gate/Boost off, default tone knobs). Builds against Rev2.4 too
// (same parameter IDs), which is how the before/after numbers are produced.
// usage: juicyboi_dynamics [chord|levels|cpu|all] [csv-dir]
//   chord : 300 ms power chord (E2 B2 E3, 0.4 FS) over a quiet 0.02 FS 330 Hz
//           note that keeps sounding for 1 s. Reports the chord's attack
//           (5 ms-RMS peak re its level at 250-300 ms, time to settle within
//           1 dB) and the quiet note's recovery after the chord stops (level
//           in 5 ms windows re its steady level, time to come back within
//           1 dB / 3 dB): blocking + bias-shift recovery.
//   levels: 110 Hz sine from -42 to 0 dBFS in 6 dB steps: output level,
//           THD, H2, H3, even-odd balance -> the level-dependent harmonic
//           change (cleanup with guitar volume / picking strength).
//   cpu   : processBlock cost per 256-sample block (mono in, stereo out,
//           one amp active) on a guitar-like signal, mean of 20 s rendered.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Parameters.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
constexpr double sr = 48000.0, twoPi = 6.283185307179586;

void setParam (juce::AudioProcessor& p, const char* id, float plain)
{
    auto& proc = dynamic_cast<JuicyboiAudioProcessor&> (p);
    if (auto* param = proc.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (plain));
}

struct Cfg { const char* name; int model, channel, voice; float gain; };
const Cfg cfgs[] { { "RedV2", 0, 0, 1, 7.0f }, { "RedV2-g10", 0, 0, 1, 10.0f }, { "RedV1", 0, 0, 0, 7.0f },
                   { "Orange", 0, 1, 1, 7.0f }, { "Lead", 1, 1, 0, 7.0f }, { "Lead-g10", 1, 1, 0, 10.0f }, { "Crunch", 1, 0, 1, 7.0f } };

std::unique_ptr<juce::AudioProcessor> make (const Cfg& c, int bs)
{
    std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
    p->setPlayConfigDetails (1, 2, sr, bs);
    setParam (*p, jb::params::id::preampModel, (float) c.model);
    if (c.model == 1)
    {
        setParam (*p, jb::params::id::m5150Channel, (float) c.channel);
        setParam (*p, jb::params::id::m5150Crunch, (float) c.voice);
        setParam (*p, c.channel == 1 ? jb::params::id::m5150LeadPre : jb::params::id::m5150RhythmPre, c.gain);
    }
    else
    {
        setParam (*p, jb::params::id::channel, (float) c.channel);
        setParam (*p, jb::params::id::redVoice, (float) c.voice);
        setParam (*p, jb::params::id::gain, c.gain);
    }
    setParam (*p, jb::params::id::cabEnable, 0.0f);
    setParam (*p, jb::params::id::eqEnable, 0.0f);
    setParam (*p, jb::params::id::gateEnable, 0.0f);
    setParam (*p, jb::params::id::odEnable, 0.0f);
    p->prepareToPlay (sr, bs);
    return p;
}

std::vector<double> render (const Cfg& c, const std::function<double (long)>& in, long n)
{
    const int bs = 256;
    auto p = make (c, bs);
    std::vector<double> out;
    juce::AudioBuffer<float> buf (2, bs);
    juce::MidiBuffer midi;
    long t = 0;
    while ((long) out.size() < n)
    {
        buf.clear();
        for (int i = 0; i < bs; ++i, ++t) buf.setSample (0, i, (float) in (t));
        p->processBlock (buf, midi);
        for (int i = 0; i < bs; ++i) out.push_back (buf.getSample (0, i));
    }
    out.resize ((size_t) n);
    return out;
}

double db (double x) { return 20.0 * std::log10 (x + 1e-20); }

// amplitude of f over y[a, a+n)
double tone (const std::vector<double>& y, long a, long n, double f)
{
    double re = 0, im = 0; const double w = twoPi * f / sr;
    for (long i = 0; i < n; ++i) { re += y[(size_t) (a + i)] * std::cos (w * i); im -= y[(size_t) (a + i)] * std::sin (w * i); }
    return 2.0 * std::sqrt (re * re + im * im) / (double) n;
}

double rms (const std::vector<double>& y, long a, long n)
{
    double s = 0; for (long i = 0; i < n; ++i) s += y[(size_t) (a + i)] * y[(size_t) (a + i)];
    return std::sqrt (s / (double) n);
}

void chord (const Cfg& c, const char* dir)
{
    const long n = (long) (1.3 * sr), chordEnd = (long) (0.3 * sr);
    auto quiet = [] (long t) { return 0.02 * std::sin (twoPi * 330.0 * (double) t / sr); };
    auto in = [&] (long t)
    {
        double x = quiet (t);
        if (t < chordEnd)
        {
            const auto tt = (double) t / sr;
            const auto env = std::min (1.0, tt / 0.002);
            x += 0.4 / 3.0 * env * (std::sin (twoPi * 82.41 * tt) + std::sin (twoPi * 123.47 * tt) + std::sin (twoPi * 164.81 * tt));
        }
        return x;
    };
    const auto y = render (c, in, n);
    const auto yq = render (c, quiet, n); // the quiet note alone: its steady reference
    const long w = (long) (0.005 * sr);
    // attack
    const auto steady = rms (y, (long) (0.25 * sr), (long) (0.05 * sr));
    double peak = 0; long settle = 0;
    for (long a = 0; a + w <= chordEnd; a += w / 2) peak = std::max (peak, rms (y, a, w));
    for (long a = chordEnd - w; a > 0; a -= w / 2) if (std::abs (db (rms (y, a, w) / steady)) > 1.0) { settle = a + w; break; }
    // recovery of the quiet 330 Hz note after the chord (tone amplitude in 20 ms windows)
    const long tw = (long) (0.02 * sr);
    const auto ref = tone (yq, (long) (1.0 * sr), tw * 10, 330.0);
    double t1 = -1, t3 = -1, dip = 0;
    FILE* f = nullptr;
    if (dir) { char path[512]; std::snprintf (path, sizeof path, "%s/chord-%s.csv", dir, c.name); f = std::fopen (path, "w"); }
    for (long a = chordEnd; a + tw <= n; a += tw / 4)
    {
        const auto d = db (tone (y, a, tw, 330.0) / ref);
        if (f) std::fprintf (f, "%.4f,%.3f\n", (double) (a - chordEnd) / sr, d);
        dip = std::min (dip, d);
    }
    for (long a = n - tw; a >= chordEnd; a -= tw / 4)
    {
        const auto d = db (tone (y, a, tw, 330.0) / ref);
        if (t1 < 0 && std::abs (d) > 1.0) t1 = (double) (a + tw - chordEnd) / sr;
        if (t3 < 0 && std::abs (d) > 3.0) t3 = (double) (a + tw - chordEnd) / sr;
    }
    if (f) std::fclose (f);
    std::printf ("%-9s chord: attack peak %+5.2f dB re steady, settles in %5.1f ms | quiet note after chord: dip %+6.1f dB, back within 3 dB %6.1f ms, 1 dB %6.1f ms\n",
                 c.name, db (peak / steady), 1000.0 * (double) settle / sr, dip, 1000.0 * std::max (0.0, t3), 1000.0 * std::max (0.0, t1));
}

void levels (const Cfg& c)
{
    for (double l = -42.0; l <= 0.01; l += 6.0)
    {
        const auto a = std::pow (10.0, l / 20.0);
        const long n = (long) sr;
        const auto y = render (c, [a] (long t) { return a * std::sin (twoPi * 110.0 * (double) t / sr); }, n);
        const long st = (long) (0.5 * sr), len = (long) (sr * 50.0 / 110.0);
        double h[21] {};
        for (int k = 1; k <= 20; ++k) h[k] = tone (y, st, len, 110.0 * k);
        double s = 0, ev = 0, od = 0;
        for (int k = 2; k <= 20; ++k) { const auto p = h[k] * h[k]; s += p; (k % 2 ? od : ev) += p; }
        std::printf ("%-9s in %+5.0f dBFS: out %+6.1f dBFS  THD %6.1f dB  H2 %6.1f  H3 %6.1f  even-odd %+5.1f dB\n",
                     c.name, l, db (h[1] / std::sqrt (2.0)), 10 * std::log10 (s / (h[1] * h[1])), db (h[2] / h[1]), db (h[3] / h[1]), 10 * std::log10 ((ev + 1e-30) / (od + 1e-30)));
    }
}

void cpu (const Cfg& c)
{
    const int bs = 256;
    auto p = make (c, bs);
    juce::AudioBuffer<float> buf (2, bs);
    juce::MidiBuffer midi;
    const long blocks = (long) (20.0 * sr / bs);
    long t = 0;
    double total = 0;
    for (long b = 0; b < blocks; ++b)
    {
        buf.clear();
        for (int i = 0; i < bs; ++i, ++t)
        {
            const auto tt = (double) t / sr, ph = std::fmod (tt, 0.25); // 8th-note chugs at 120 bpm
            buf.setSample (0, i, (float) (0.3 * std::exp (-ph * 12.0) * (std::sin (twoPi * 82.41 * tt) + 0.6 * std::sin (twoPi * 123.47 * tt) + 0.4 * std::sin (twoPi * 164.8 * tt))));
        }
        const auto t0 = std::chrono::steady_clock::now();
        p->processBlock (buf, midi);
        total += std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    }
    const auto perBlock = total / (double) blocks;
    std::printf ("%-9s cpu: %7.1f us per 256-sample block = %5.2f %% of one core in real time (mono in)\n",
                 c.name, perBlock * 1e6, 100.0 * perBlock / (bs / sr));
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const std::string mode = argc > 1 ? argv[1] : "all";
    const char* dir = argc > 2 ? argv[2] : nullptr;
    for (const auto& c : cfgs)
    {
        if (mode == "chord" || mode == "all") chord (c, dir);
        if (mode == "levels" || mode == "all") levels (c);
        if (mode == "cpu" || mode == "all") cpu (c);
    }
    return 0;
}
