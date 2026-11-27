// Rev2.3 Red v2 low end: power-amp / 4x12 load interaction (SpeakerLoad.h).
//  1) analytic network values at the documented frequencies
//  2) digital filter == analytic |H| at the oversampled rates (sine, steady state)
//  3) res = 0 is an exact bypass; only Red v2 enables it
//  4) whole plugin: the bump follows the Red v1 -> v2 crossfade without a click.
// Red v1 / Orange / 5150 staying bit-identical to Rev2.2 is checked by the
// fixture nulls in PreampModelTest and ResonanceTest.
#include "PluginProcessor.h"
#include "dsp/SpeakerLoad.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace jb::dsp;

namespace
{
int failures = 0;
void check (const char* what, bool ok)
{
    std::printf ("[%s] %s\n", ok ? "ok" : "FAIL", what);
    if (! ok) ++failures;
}

double sineGainDb (const SpeakerLoadParams& p, double fs, double hz)
{
    SpeakerLoad s; s.prepare (fs, p);
    const int settle = (int) (fs * 0.4), meas = (int) (fs * 0.5);
    double in = 0, out = 0;
    for (int i = 0; i < settle + meas; ++i)
    {
        const auto x = std::sin (2.0 * kPi * hz * i / fs);
        const auto y = (double) s.process ((float) x, 0);
        if (i >= settle) { in += x * x; out += y * y; }
    }
    return 10.0 * std::log10 (out / in);
}
} // namespace

int speakerLoadValidation()
{
    failures = 0;
    std::printf ("== Rev2.3 Red v2 power-amp / 4x12 load ==\n");
    const auto p = voicings::rectoSpeakerLoad();
    auto db = [&] (double f) { return 20.0 * std::log10 (SpeakerLoad::magnitude (p, f)); };

    // 1) analytic network
    std::printf ("4x12 load bump (analytic): 40:%+.2f 60:%+.2f 80:%+.2f 100:%+.2f 106:%+.2f 150:%+.2f 200:%+.2f 1k:%+.3f dB\n",
                 db (40), db (60), db (80), db (100), db (106), db (150), db (200), db (1000));
    const auto peakAnalytic = 20.0 * std::log10 ((1.0 + p.res / p.re) / (1.0 + p.res / (p.re + p.zout)));
    check ("peak at f0 equals (1+Res/Re)/(1+Res/(Re+Zo)) = +3.0 dB", std::abs (db (p.f0) - peakAnalytic) < 1e-9 && std::abs (peakAnalytic - 3.0) < 0.05);
    check ("bump: +2.4..+2.8 dB at 80 Hz, +2.8..+3.1 dB at 100 Hz", db (80) > 2.4 && db (80) < 2.8 && db (100) > 2.8 && db (100) < 3.1);
    check ("tight: 40 Hz < +1 dB, 30 Hz < +0.6 dB (no sub-bass flub)", db (40) < 1.0 && db (30) < 0.6);
    check ("mids/highs untouched: |1 kHz| < 0.1 dB, |5 kHz| < 0.01 dB, DC = 0 dB",
           std::abs (db (1000)) < 0.1 && std::abs (db (5000)) < 0.01 && std::abs (db (0.01)) < 1e-6);

    // 2) digital == analytic (bilinear prewarped at f0)
    {
        double worst = 0;
        for (double fs : { 176400.0, 192000.0, 384000.0 })
            for (double f : { 40.0, 80.0, 106.0, 200.0, 1000.0 })
                worst = std::max (worst, std::abs (sineGainDb (p, fs, f) - db (f)));
        std::printf ("digital vs analytic |H|: worst %.4f dB (4x of 44.1/48/96 kHz)\n", worst);
        check ("digital filter matches the analytic network within 0.02 dB", worst < 0.02);
    }

    // 3) bypass is exact; only Red v2 sets it
    {
        SpeakerLoad s; s.prepare (192000.0, SpeakerLoadParams {});
        bool exact = true;
        for (int i = 0; i < 1000; ++i) { const auto x = (float) std::sin (0.01 * i) * 0.7f; exact = exact && s.process (x, 0) == x; }
        check ("res = 0 passes samples through bit-exactly", exact);
        check ("only Red v2 enables the load (Red v1 / Orange res = 0)",
               voicings::redV2().speakerLoad.res > 0.0 && voicings::redV1().speakerLoad.res == 0.0
               && voicings::orange().speakerLoad.res == 0.0);
    }

    // 4) whole plugin: Red v1 -> v2 switch with a 100 Hz tone stays click-free
    {
        JuicyboiAudioProcessor proc;
        auto set = [&] (const char* id, float v) { auto* a = proc.apvts.getParameter (id); a->setValueNotifyingHost (a->convertTo0to1 (v)); };
        set ("red_voice", 0.0f); set ("cab_enable", 0.0f); set ("gate_enable", 0.0f);
        proc.setPlayConfigDetails (1, 2, 48000.0, 256);
        proc.prepareToPlay (48000.0, 256);
        juce::MidiBuffer midi;
        std::vector<float> y;
        for (int b = 0; b < 160; ++b)
        {
            if (b == 80) set ("red_voice", 1.0f);
            juce::AudioBuffer<float> buf (2, 256); buf.clear();
            for (int i = 0; i < 256; ++i) buf.setSample (0, i, (float) (0.15 * std::sin (2.0 * kPi * 100.0 * (b * 256 + i) / 48000.0)));
            proc.processBlock (buf, midi);
            y.insert (y.end(), buf.getReadPointer (0), buf.getReadPointer (0) + 256);
        }
        double fade = 0, v1 = 0, v2 = 0; bool finite = true;
        for (size_t i = 2; i < y.size(); ++i)
        {
            finite = finite && std::isfinite (y[i]);
            const auto d2 = std::abs ((double) y[i] - 2.0 * y[i - 1] + y[i - 2]);
            const auto blk = i / 256;
            if (blk >= 40 && blk < 80) v1 = std::max (v1, d2);
            else if (blk >= 80 && blk < 110) fade = std::max (fade, d2);
            else if (blk >= 120) v2 = std::max (v2, d2);
        }
        std::printf ("Red v1 -> v2 switch at 100 Hz: 2nd difference during fade %.4f (steady v1 %.4f, v2 %.4f)\n", fade, v1, v2);
        check ("Red v1 -> v2 switch (load bump fades in) is finite and click-free", finite && fade < 1.25 * std::max (v1, v2) + 1e-4);
    }
    return failures;
}
