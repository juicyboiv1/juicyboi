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

    for (int odOn = 0; odOn < 2; ++odOn)
        for (int ch = 0; ch < 2; ++ch)
            for (float gain : { 0.0f, 6.0f, 10.0f })
            {
                setParam (*p, jb::params::id::odEnable, (float) odOn);
                setParam (*p, jb::params::id::channel, (float) ch);
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
                std::printf ("OD=%d ch=%s gain=%4.1f  rms=%6.1f dBFS  peak=%.3f  %s\n",
                             odOn, ch == 0 ? "Riff A" : "Riff B", gain, rmsDb, peak, ok ? "ok" : "FAIL");
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
    std::printf (failures == 0 ? "ALL OK\n" : "%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
