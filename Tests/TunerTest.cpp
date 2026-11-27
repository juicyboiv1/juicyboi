// Rev3.2 tuner pedal: pitch accuracy (+/-1 cent, sine / band-limited sawtooth /
// decaying pluck, 27.5 Hz .. 2 kHz at 44.1/48/96 kHz), display smoothing and
// reference A, TU-3 mute (click-free, silent), off = bit-identical.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "dsp/Tuner.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
int failures = 0;
void check (const char* what, bool ok)
{
    std::printf ("[%s] tuner: %s\n", ok ? "ok" : "FAIL", what);
    if (! ok) ++failures;
}
constexpr double pi = 3.14159265358979323846;

/** type 0 sine, 1 band-limited sawtooth, 2 decaying "pluck" (per-harmonic decay, random-ish phases) */
std::vector<float> tone (int type, double f, double fs, double seconds, double amp = 0.3, unsigned seed = 1)
{
    const int n = (int) (seconds * fs);
    std::vector<float> x ((size_t) n);
    juce::Random rng ((juce::int64) seed);
    std::vector<double> ph (200);
    for (auto& p : ph) p = rng.nextDouble() * 2.0 * pi;
    for (int i = 0; i < n; ++i)
    {
        const auto t = i / fs, w = 2.0 * pi * f * t;
        double v = 0.0;
        if (type == 0) v = std::sin (w);
        else if (type == 1) { for (int h = 1; h * f < 0.45 * fs && h < 200; ++h) v += std::sin (h * w) / h; }
        else { for (int h = 1; h * f < 0.45 * fs && h < 60; ++h) v += 1.3 * std::sin (h * w + ph[(size_t) h]) / h * std::exp (-t * (1.5 + 0.4 * h)); }
        x[(size_t) i] = (float) (amp * v);
    }
    return x;
}

void setParam (JuicyboiAudioProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}
} // namespace

int tunerValidation()
{
    failures = 0;
    using jb::dsp::Tuner;

    // 1) raw detection accuracy
    {
        const double notes[] { 27.5, 30.868, 41.203, 55.0, 73.416, 82.407, 110.0, 146.83, 196.0, 246.94,
                               329.63, 440.0, 659.26, 987.77, 1318.51, 1760.0, 1975.53 };
        const char* typeNames[] { "sine", "sawtooth", "pluck" };
        double worstAll = 0.0;
        int misses = 0;
        for (double fs : { 44100.0, 48000.0, 96000.0 })
            for (int type = 0; type < 3; ++type)
            {
                double worst = 0.0;
                for (auto f0 : notes)
                    for (double det : { -23.0, 0.0, 7.0, 41.0 })   // cents off the note
                    {
                        const auto f = f0 * std::pow (2.0, det / 1200.0);
                        Tuner t; t.prepare (fs, 512); t.setEnabled (true);
                        const auto x = tone (type, f, fs, 0.3);
                        t.push (x.data(), (int) x.size());
                        t.analyse();
                        const auto raw = t.getDisplay().rawHz;
                        if (raw <= 0.0) { ++misses; continue; }
                        worst = std::max (worst, std::abs (1200.0 * std::log2 (raw / f)));
                    }
                std::printf ("tuner: %5.1f kHz %-8s 27.5 Hz..1.98 kHz: worst %.3f cents\n", fs / 1000.0, typeNames[type], worst);
                worstAll = std::max (worstAll, worst);
            }
        check ("every test tone detected", misses == 0);
        check ("pitch within +/-1 cent (sine, sawtooth, pluck; E1..B6 and drop/low-B)", worstAll < 1.0);
    }

    // 2) display: note name, smoothing / jitter, reference A
    {
        Tuner t; t.prepare (48000.0, 512); t.setEnabled (true);
        // low E a few cents flat, with a little noise
        const auto f = 82.407 * std::pow (2.0, -4.0 / 1200.0);
        auto x = tone (2, f, 48000.0, 2.0, 0.3, 7);
        juce::Random rng (3);
        for (auto& v : x) v += 0.003f * (rng.nextFloat() - 0.5f);
        std::vector<double> shown;
        for (size_t i = 0; i < x.size(); i += 1200)   // 25 ms hops
        {
            t.push (x.data() + i, (int) std::min<size_t> (1200, x.size() - i));
            t.analyse (0.025);
            const auto d = t.getDisplay();
            if (d.hasPitch && i > 48000 / 2) shown.push_back (d.cents);
        }
        const auto d = t.getDisplay();
        double mean = 0.0, var = 0.0;
        for (auto c : shown) mean += c;
        mean /= std::max<size_t> (1, shown.size());
        for (auto c : shown) var += (c - mean) * (c - mean);
        const auto sd = std::sqrt (var / std::max<size_t> (1, shown.size()));
        std::printf ("tuner: E2 -4 c pluck: shows %s%d %+.2f c (mean %+.2f, jitter sd %.3f c over %zu frames)\n",
                     Tuner::noteName (d.midiNote), Tuner::octave (d.midiNote), d.cents, mean, sd, shown.size());
        check ("decaying E2 shows E2, about -4 cents, steady (sd < 0.3 c)",
               d.hasPitch && d.midiNote == 40 && std::abs (mean + 4.0) < 1.0 && sd < 0.3);

        Tuner r; r.prepare (48000.0, 512); r.setEnabled (true); r.setReference (442.0);
        const auto a = tone (1, 440.0, 48000.0, 0.5);
        for (int k = 0; k < 6; ++k) { r.push (a.data() + k * 2400, 2400); r.analyse(); }
        const auto rd = r.getDisplay();
        const auto expect = 1200.0 * std::log2 (440.0 / 442.0);
        std::printf ("tuner: A=442 reference, 440 Hz tone -> %s %+.2f c (expect %+.2f)\n", Tuner::noteName (rd.midiNote), rd.cents, expect);
        check ("reference A=442 shifts the reading", rd.midiNote == 69 && std::abs (rd.cents - expect) < 0.5);

        // silence -> display clears after the hold
        std::vector<float> z (48000, 0.0f);
        for (int k = 0; k < 20; ++k) { r.push (z.data() + k * 2400, 2400); r.analyse (0.05); }
        check ("silence clears the display after the hold", ! r.getDisplay().hasPitch);
        check ("note names", juce::String (Tuner::noteName (40)) == "E" && juce::String (Tuner::noteName (61)) == "C#"
                             && Tuner::octave (40) == 2);
    }

    // 3) plugin: TU-3 mute (silent, click-free), off = bit-identical, detection via the processor
    {
        const double sr = 48000.0;
        const int bs = 256;
        JuicyboiAudioProcessor a, b;
        a.stopTunerThread(); b.stopTunerThread();
        for (auto* p : { &a, &b })
        {
            p->setPlayConfigDetails (1, 2, sr, bs);
            p->prepareToPlay (sr, bs);
        }
        check ("tuner defaults: off, A = 440 Hz",
               a.apvts.getRawParameterValue ("tuner_on")->load() < 0.5f && a.apvts.getRawParameterValue ("tuner_ref")->load() == 440.0f);

        const auto guitar = tone (2, 110.0, sr, 3.0, 0.25, 11);
        juce::MidiBuffer midi;
        double prevGain = -1.0, maxJump = 0.0, mutedPeak = 0.0, unmutedPeakA = 0.0;
        bool identicalAfter = true, identicalBefore = true;
        const int blocks = (int) guitar.size() / bs;
        for (int k = 0; k < blocks; ++k)
        {
            if (k == 100) setParam (b, "tuner_on", 1.0f);
            if (k == 300) setParam (b, "tuner_on", 0.0f);
            juce::AudioBuffer<float> ba (2, bs), bb (2, bs);
            for (int ch = 0; ch < 2; ++ch) { ba.clear (ch, 0, bs); bb.clear (ch, 0, bs); }
            ba.copyFrom (0, 0, guitar.data() + k * bs, bs);
            bb.copyFrom (0, 0, guitar.data() + k * bs, bs);
            a.processBlock (ba, midi);
            b.processBlock (bb, midi);
            if (k == 250) b.getTuner().analyse();
            for (int i = 0; i < bs; ++i)
            {
                const auto ya = ba.getSample (0, i), yb = bb.getSample (0, i);
                if (k < 100 && ya != yb) identicalBefore = false;
                if (k >= 305 && (ya != yb || ba.getSample (1, i) != bb.getSample (1, i))) identicalAfter = false;
                if (k >= 110 && k < 300) mutedPeak = std::max (mutedPeak, (double) std::abs (yb));
                if (k >= 95 && k < 315)
                {
                    if (std::abs (ya) > 1e-3)
                    {
                        const double g = yb / ya;
                        if (prevGain >= 0.0) maxJump = std::max (maxJump, std::abs (g - prevGain));
                        prevGain = g;
                    }
                    unmutedPeakA = std::max (unmutedPeakA, (double) std::abs (ya));
                }
            }
            if (k == 250)
            {
                const auto d = b.getTuner().getDisplay();
                std::printf ("tuner: plugin path, A2 pluck -> %s%d %+.2f c\n", Tuner::noteName (d.midiNote), Tuner::octave (d.midiNote), d.cents);
                check ("plugin feeds the tuner the input (A2 detected within 1 cent)", d.hasPitch && d.midiNote == 45 && std::abs (d.cents) < 1.0);
            }
        }
        std::printf ("tuner: muted peak %.3g (unmuted %.3f); fade gain step per sample <= %.4f\n", mutedPeak, unmutedPeakA, maxJump);
        check ("tuner off: bit-identical to a processor that never used it", identicalBefore);
        check ("tuner on: output fully muted (exact zeros)", mutedPeak == 0.0);
        check ("mute fade is click-free (gain step <= 1 %% per sample)", maxJump <= 0.01);
        check ("tuner off again: bit-identical once the 10 ms fade is done", identicalAfter);
    }
    return failures;
}
