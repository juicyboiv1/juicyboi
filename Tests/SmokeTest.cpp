// Headless smoke test: instantiates the processor, runs a guitar-ish test
// signal through every channel/boost combination and checks output sanity,
// reported latency, and bypass alignment.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <cstdio>

extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static void setParam (juce::AudioProcessor& p, const char* id, float plain)
{
    auto& proc = dynamic_cast<JuicyboiAudioProcessor&> (p);
    auto* param = proc.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (plain));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());

    const double sr = 48000.0;
    const int bs = 256;
    p->setPlayConfigDetails (1, 2, sr, bs);
    p->prepareToPlay (sr, bs);
    std::printf ("latency: %d samples\n", p->getLatencySamples());

    int failures = 0;
    juce::MidiBuffer midi;

    {
        auto& jp0 = dynamic_cast<JuicyboiAudioProcessor&> (*p);
        const bool rvDef = jp0.apvts.getRawParameterValue (jb::params::id::redVoice)->load() > 0.5f;
        std::printf ("red_voice default v2  %s\n", rvDef ? "ok" : "FAIL");
        if (! rvDef) ++failures;
    }

    // ch: 0 = Red v1, 1 = Red v2, 2 = Orange
    static const char* chNames[] = { "Red v1", "Red v2", "Orange" };
    for (int odOn = 0; odOn < 2; ++odOn)
        for (int ch = 0; ch < 3; ++ch)
            for (float gain : { 0.0f, 6.0f, 10.0f })
            {
                setParam (*p, jb::params::id::odEnable, (float) odOn);
                setParam (*p, jb::params::id::channel, ch == 2 ? 1.0f : 0.0f);
                setParam (*p, jb::params::id::redVoice, ch == 1 ? 1.0f : 0.0f);
                setParam (*p, jb::params::id::gain, gain);
                setParam (*p, jb::params::id::odDrive, 5.0f);

                double sumSq = 0.0; float peak = 0.0f; bool finite = true; long n = 0;
                double phase = 0.0;
                for (int b = 0; b < 200; ++b) // ~1 s
                {
                    juce::AudioBuffer<float> buf (2, bs);
                    for (int i = 0; i < bs; ++i)
                    {
                        // decaying low-E-ish tone + harmonic, ~-12 dBFS peak
                        const auto s = 0.25 * std::sin (phase) + 0.08 * std::sin (3.0 * phase);
                        phase += 2.0 * juce::MathConstants<double>::pi * 82.4 / sr;
                        buf.setSample (0, i, (float) s);
                        buf.setSample (1, i, 0.0f);
                    }
                    p->processBlock (buf, midi);
                    if (b < 20) continue; // skip settle
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < bs; ++i)
                        {
                            const auto v = buf.getSample (c, i);
                            if (! std::isfinite (v)) finite = false;
                            peak = std::max (peak, std::abs (v));
                            sumSq += (double) v * v; ++n;
                        }
                }
                const auto rmsDb = 10.0 * std::log10 (sumSq / (double) n + 1e-20);
                const bool ok = finite && peak < 4.0f && rmsDb > -60.0;
                if (! ok) ++failures;
                std::printf ("OD=%d ch=%-6s gain=%4.1f  rms=%6.1f dBFS  peak=%.3f  %s\n",
                             odOn, chNames[ch], gain, rmsDb, peak, ok ? "ok" : "FAIL");
            }

    // Gate: off by default; when on, a -86 dBFS hiss key must be muted and a
    // played note must pass. Measures output RMS for a given input generator.
    {
        auto& jp = dynamic_cast<JuicyboiAudioProcessor&> (*p);
        const bool defOff = jp.apvts.getRawParameterValue (jb::params::id::gateEnable)->load() < 0.5f;
        std::printf ("gate default off  %s\n", defOff ? "ok" : "FAIL");
        if (! defOff) ++failures;

        setParam (*p, jb::params::id::odEnable, 1.0f);
        setParam (*p, jb::params::id::channel, 1.0f);
        setParam (*p, jb::params::id::gain, 10.0f);

        juce::Random rng (1234);
        auto runRms = [&] (bool tone, int blocks)
        {
            double sumSq = 0.0; long n = 0; double ph = 0.0; bool fin = true;
            for (int b = 0; b < blocks; ++b)
            {
                juce::AudioBuffer<float> gb (2, bs);
                for (int i = 0; i < bs; ++i)
                {
                    auto s = 5.0e-5 * (rng.nextFloat() * 2.0 - 1.0); // ~-86 dBFS hiss
                    if (tone) { s += 0.2 * std::sin (ph); ph += 2.0 * juce::MathConstants<double>::pi * 82.4 / sr; }
                    gb.setSample (0, i, (float) s);
                    gb.setSample (1, i, 0.0f);
                }
                p->processBlock (gb, midi);
                if (b < blocks / 2) continue;
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < bs; ++i)
                    {
                        const auto v = gb.getSample (c, i);
                        if (! std::isfinite (v)) fin = false;
                        sumSq += (double) v * v; ++n;
                    }
            }
            const auto db = 10.0 * std::log10 (sumSq / (double) n + 1e-20);
            return fin ? db : 999.0;
        };

        const auto hissOff = runRms (false, 100);
        setParam (*p, jb::params::id::gateEnable, 1.0f);
        const auto hissOn  = runRms (false, 100);
        const auto noteOn  = runRms (true, 100);
        const auto openAmt = jp.getGateOpenAmount();
        const auto tail    = runRms (false, 100); // note stops -> gate must close again
        const auto closedAmt = jp.getGateOpenAmount();

        const bool gOk = hissOn < -100.0 && hissOff > hissOn + 30.0 && noteOn > -40.0 && noteOn < 20.0
                         && openAmt > 0.99f && tail < -100.0 && closedAmt < 0.01f;
        std::printf ("gate: hiss off=%6.1f  hiss on=%6.1f  note on=%6.1f  tail=%6.1f dBFS  open=%.2f closed=%.2f  %s\n",
                     hissOff, hissOn, noteOn, tail, openAmt, closedAmt, gOk ? "ok" : "FAIL");
        if (! gOk) ++failures;

        setParam (*p, jb::params::id::gateEnable, 0.0f);
        runRms (false, 10);
        const bool bpGate = ! jp.isGateActive() && jp.getGateOpenAmount() > 0.999f;
        std::printf ("gate off -> true bypass  %s\n", bpGate ? "ok" : "FAIL");
        if (! bpGate) ++failures;
    }

    // Graphic EQ: standalone DSP checks (band gain, shelf, level, true bypass)
    {
        auto sineGainDb = [] (jb::dsp::GraphicEq& eq, double hz, double fs)
        {
            double inSq = 0.0, outSq = 0.0, ph = 0.0;
            const int n = 256;
            for (int b = 0; b < 400; ++b)
            {
                float buf[n];
                for (int i = 0; i < n; ++i) { buf[i] = (float) (0.1 * std::sin (ph)); ph += 2.0 * juce::MathConstants<double>::pi * hz / fs; }
                float in[n]; std::copy (buf, buf + n, in);
                float* ch[1] { buf };
                eq.process (ch, 1, n);
                if (b < 200) continue;
                for (int i = 0; i < n; ++i) { inSq += (double) in[i] * in[i]; outSq += (double) buf[i] * buf[i]; }
            }
            return 10.0 * std::log10 (outSq / inSq);
        };

        const double fs = 48000.0;
        std::array<float, 7> flat {};
        jb::dsp::GraphicEq eq;
        const bool defOff = dynamic_cast<JuicyboiAudioProcessor&> (*p).apvts.getRawParameterValue (jb::params::id::eqEnable)->load() < 0.5f;

        auto test = [&] (const char* what, std::array<float, 7> g, float lvl, double hz, double expect, double tol)
        {
            eq.setParameters (true, g, lvl);
            eq.prepare (fs, 256);
            const auto got = sineGainDb (eq, hz, fs);
            const bool ok = std::abs (got - expect) < tol;
            std::printf ("eq %-26s %7.2f dB (expect %6.1f)  %s\n", what, got, expect, ok ? "ok" : "FAIL");
            if (! ok) ++failures;
        };
        test ("flat @ 1 kHz", flat, 0.0f, 1000.0, 0.0, 0.05);
        test ("800 +15 @ 800 Hz",  { 0, 0, 0, 15, 0, 0, 0 }, 0.0f, 800.0, 15.0, 0.5);
        test ("100 -15 @ 100 Hz",  { -15, 0, 0, 0, 0, 0, 0 }, 0.0f, 100.0, -15.0, 0.6);
        test ("6.4k shelf +15 @ 14 kHz", { 0, 0, 0, 0, 0, 0, 15 }, 0.0f, 14000.0, 15.0, 1.0);
        test ("800 +15 @ 6.4 kHz (isol.)", { 0, 0, 0, 15, 0, 0, 0 }, 0.0f, 6400.0, 0.0, 1.0);
        test ("level +6 @ 1 kHz", flat, 6.0f, 1000.0, 6.0, 0.05);

        // Off -> fades, then true bypass (bit-identical passthrough)
        eq.setParameters (true, { 15, -15, 15, -15, 15, -15, 15 }, 10.0f);
        eq.prepare (fs, 256);
        eq.setParameters (false, { 15, -15, 15, -15, 15, -15, 15 }, 10.0f);
        float tmp[512] {}; float* tch[1] { tmp };
        eq.process (tch, 1, 512); // 10 ms fade at 48k = 480 samples
        float buf[256], ref[256];
        for (int i = 0; i < 256; ++i) buf[i] = ref[i] = (float) std::sin (i * 0.1);
        float* bch[1] { buf };
        eq.process (bch, 1, 256);
        bool identical = ! eq.isActive();
        for (int i = 0; i < 256; ++i) identical = identical && buf[i] == ref[i];
        std::printf ("eq default off  %s\neq off -> true bypass  %s\n", defOff ? "ok" : "FAIL", identical ? "ok" : "FAIL");
        if (! defOff) ++failures;
        if (! identical) ++failures;

        // In-chain: EQ on, all bands/level maxed, Red v2 gain 10 + boost -> finite
        setParam (*p, jb::params::id::eqEnable, 1.0f);
        for (auto* id : jb::params::id::eqBands) setParam (*p, id, 15.0f);
        setParam (*p, jb::params::id::eqLevel, 15.0f);
        bool fin = true; double ph = 0.0; float pk = 0.0f;
        for (int b = 0; b < 100; ++b)
        {
            juce::AudioBuffer<float> eb (2, bs);
            for (int i = 0; i < bs; ++i) { eb.setSample (0, i, (float) (0.25 * std::sin (ph))); eb.setSample (1, i, 0.0f); ph += 2.0 * juce::MathConstants<double>::pi * 82.4 / sr; }
            p->processBlock (eb, midi);
            for (int i = 0; i < bs; ++i) { const auto v = eb.getSample (0, i); fin = fin && std::isfinite (v); pk = std::max (pk, std::abs (v)); }
        }
        const bool chainOk = fin && dynamic_cast<JuicyboiAudioProcessor&> (*p).isEqActive();
        std::printf ("eq in-chain maxed: finite, peak=%.2f  %s\n", pk, chainOk ? "ok" : "FAIL");
        if (! chainOk) ++failures;
        for (auto* id : jb::params::id::eqBands) setParam (*p, id, 0.0f);
        setParam (*p, jb::params::id::eqLevel, 0.0f);
        setParam (*p, jb::params::id::eqEnable, 0.0f);
    }

    // Bypass alignment: impulse should come out exactly `latency` samples later
    setParam (*p, jb::params::id::bypass, 1.0f);
    p->prepareToPlay (sr, bs); // snaps smoothers to bypassed state
    const int lat = p->getLatencySamples();
    juce::AudioBuffer<float> buf (2, bs);
    buf.clear();
    buf.setSample (0, 10, 1.0f);
    p->processBlock (buf, midi);
    int found = -1;
    for (int i = 0; i < bs; ++i)
        if (std::abs (buf.getSample (0, i) - 1.0f) < 1e-4f) found = i;
    const bool bpOk = found == 10 + lat && std::abs (buf.getSample (1, 10 + lat) - 1.0f) < 1e-4f;
    std::printf ("bypass impulse at %d (expected %d, both channels)  %s\n", found, 10 + lat, bpOk ? "ok" : "FAIL");
    if (! bpOk) ++failures;

    p->releaseResources();

    // Standalone input path: the JUCE Standalone player opens the processor
    // with however many interface inputs are ticked (Scarlett 4i4: up to 4).
    // A mono guitar on ANY one of them must reach the preamp and come out on
    // both L and R. Plugin builds must still reject non-matching multi-in.
    {
        using ACS = juce::AudioChannelSet;
        const bool pluginRejects = ! p->checkBusesLayoutSupported ({ { ACS::canonicalChannelSet (4) }, { ACS::stereo() } });
        std::printf ("plugin wrapper rejects 4-in/2-out  %s\n", pluginRejects ? "ok" : "FAIL");
        if (! pluginRejects) ++failures;

        juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
        std::unique_ptr<juce::AudioProcessor> sa (createPluginFilter());
        juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);

        for (int numIns : { 1, 2, 4 })
        {
            const bool layoutOk = sa->checkBusesLayoutSupported ({ { ACS::canonicalChannelSet (numIns) }, { ACS::stereo() } });
            sa->setPlayConfigDetails (numIns, 2, sr, bs);
            sa->prepareToPlay (sr, bs);

            for (int guitarCh = 0; guitarCh < numIns; ++guitarCh)
            {
                double l = 0.0, r = 0.0; double ph = 0.0; long n = 0; bool fin = true;
                for (int b = 0; b < 100; ++b)
                {
                    juce::AudioBuffer<float> sb (juce::jmax (numIns, 2), bs);
                    sb.clear();
                    for (int i = 0; i < bs; ++i)
                    {
                        sb.setSample (guitarCh, i, (float) (0.25 * std::sin (ph)));
                        ph += 2.0 * juce::MathConstants<double>::pi * 82.4 / sr;
                    }
                    sa->processBlock (sb, midi);
                    if (b < 20) continue;
                    for (int i = 0; i < bs; ++i)
                    {
                        const auto vl = sb.getSample (0, i), vr = sb.getSample (1, i);
                        if (! std::isfinite (vl) || ! std::isfinite (vr)) fin = false;
                        l += (double) vl * vl; r += (double) vr * vr; ++n;
                    }
                }
                const auto lDb = 10.0 * std::log10 (l / (double) n + 1e-20);
                const auto rDb = 10.0 * std::log10 (r / (double) n + 1e-20);
                const bool ok = layoutOk && fin && lDb > -60.0 && std::abs (lDb - rDb) < 0.01;
                std::printf ("standalone %d-in, guitar on input %d: L=%6.1f R=%6.1f dBFS  %s\n",
                             numIns, guitarCh + 1, lDb, rDb, ok ? "ok" : "FAIL");
                if (! ok) ++failures;
            }
            sa->releaseResources();
        }
    }

    std::printf (failures == 0 ? "ALL OK\n" : "%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
