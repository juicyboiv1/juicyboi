#include "PluginProcessor.h"
#include <complex>
#include <cstdio>
#include <cstring>

namespace
{
using Complex = std::complex<double>;
// Independent nodal reference: solve loaded feedback divider directly, rather
// than reusing production pole/zero or TPT coefficient code. Values/assumptions
// are the declared nominal reduction, NOT amplifier measurements.
Complex reference (double knob, double hz)
{
    if (hz < 1e-12) return { 1.0, 0.0 };
    const Complex s (0.0, 2.0 * jb::dsp::kPi * hz);
    auto loop = [s] (double k)
    {
        const double rp = 10000.0 * (std::pow (10.0, 2.0 * (1.0 - k / 10.0)) - 1.0) / 99.0;
        const auto z = 1.0 / (1.0 / 4700.0 + 1.0 / (rp + 1.0 / (s * 0.133e-6)));
        const auto beta = z / (39000.0 + z);
        return 20.0 / (1.0 + 20.0 * beta);
    };
    return loop (knob) / loop (0.0);
}
void set (JuicyboiAudioProcessor& p, const char* id, float v)
{
    auto* param = p.apvts.getParameter (id); param->setValueNotifyingHost (param->convertTo0to1 (v));
}
void configure (JuicyboiAudioProcessor& p, float model, float presence, bool eq)
{
    set (p, "preamp_model", model); set (p, "lead_presence", presence);
    set (p, "od_enable", 1.0f); set (p, "od_type", 1.0f); set (p, "pre_gain", 10.0f);
    set (p, "eq_enable", eq ? 1.0f : 0.0f); set (p, "eq_800", 6.0f); set (p, "eq_6k4", -4.0f);
    p.setPlayConfigDetails (1, 2, 48000.0, 64); p.prepareToPlay (48000.0, 64);
}
}

int presenceValidation()
{
    int failures = 0;
    auto check = [&] (const char* label, bool ok)
    { std::printf ("presence %-54s %s\n", label, ok ? "ok" : "FAIL"); if (! ok) ++failures; };
    double worstComplex = 0.0, worstUnwarpedDb = 0.0; bool curvesFinite = true;
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (float knob : { 0.0f, 5.0f, 10.0f })
            for (double hz : { 20.0, 100.0, 300.0, 1000.0, 3000.0, 10000.0 })
            {
                jb::dsp::LeadPresence filter; filter.setPosition (knob); filter.prepare (fs);
                double real = 0.0, imag = 0.0; int count = 0;
                const int n = (int) fs;
                for (int i = 0; i < n; ++i)
                {
                    const auto phase = 2.0 * jb::dsp::kPi * hz * (double) i / fs;
                    const auto x = (float) (0.01 * std::sin (phase)); filter.beginFrame();
                    const auto y = filter.process (x, 0);
                    if (i >= n / 2) { real += y * std::sin (phase); imag += y * std::cos (phase); ++count; }
                }
                const Complex measured (2.0 * real / (0.01 * count), 2.0 * imag / (0.01 * count));
                const auto warped = fs / jb::dsp::kPi * std::tan (jb::dsp::kPi * hz / fs);
                const auto expected = reference (knob, warped);
                curvesFinite = curvesFinite && std::isfinite (measured.real()) && std::isfinite (measured.imag());
                worstComplex = std::max (worstComplex, std::abs (measured - expected));
                const auto db = 20.0 * std::log10 (std::abs (measured));
                worstUnwarpedDb = std::max (worstUnwarpedDb, std::abs (db - 20.0 * std::log10 (std::abs (reference (knob, hz)))));
                if (std::abs (fs - 48000.0) < 0.1 && (std::abs (hz - 100.0) < 0.1 || std::abs (hz - 1000.0) < 0.1 || std::abs (hz - 10000.0) < 0.1))
                    std::printf ("Presence @48k p=%2.0f f=%5.0f: measured=%7.3f dB, nodal BLT=%7.3f dB\n",
                                 (double) knob, hz, db, 20.0 * std::log10 (std::abs (expected)));
            }
    std::printf ("Presence worst complex error %.9g; unwarped analog magnitude discrepancy %.6f dB\n", worstComplex, worstUnwarpedDb);
    check ("54 measured magnitude/phase points vs loaded nodal model", curvesFinite && worstComplex < 1e-5 && worstUnwarpedDb < 0.1);
    check ("no DC gain, sensible HF range and mid taper", std::abs (reference (10.0, 0.0) - Complex (1.0)) < 1e-12
           && std::abs (reference (10.0, 10000.0)) > std::abs (reference (5.0, 10000.0))
           && std::abs (reference (10.0, 10000.0)) < 3.0);

    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        jb::dsp::LeadPresence filter; filter.prepare (fs);
        bool exact = true, finite = true, stereo = true; float maxStep = 0.0f, peak = 0.0f, previous = 0.0f;
        for (int i = 0; i < (int) fs; ++i)
        {
            const float x = (float) (0.01 * std::sin (2.0 * jb::dsp::kPi * 1000.0 * i / fs));
            filter.beginFrame(); const auto y = filter.process (x, 0); filter.process (x, 1);
            exact = exact && std::memcmp (&x, &y, sizeof (float)) == 0;
        }
        check ("position zero is bit-identical reference (each rate)", exact);
        for (int i = 0; i < (int) (2.0 * fs); ++i)
        {
            if (i % 113 == 0) filter.setPosition ((float) ((i / 113) % 11));
            filter.beginFrame();
            const float x = (float) (0.01 * std::sin (2.0 * jb::dsp::kPi * 1000.0 * i / fs));
            const auto y = filter.process (x, 0), right = filter.process (x, 1);
            finite = finite && std::isfinite (y); stereo = stereo && std::abs (y - right) < 1e-7f;
            peak = std::max (peak, std::abs (y)); maxStep = std::max (maxStep, std::abs (y - previous)); previous = y;
        }
        check ("rapid sweeps stay finite/bounded/stereo/smooth", finite && stereo && peak < 0.05f && maxStep < 0.02f);
        filter.setPosition (10.0f);
        float tail = 0.0f;
        for (int i = 0; i < (int) fs; ++i)
        {
            filter.beginFrame(); const auto y = filter.process (0.0f, 0); filter.process (0.0f, 1);
            if (i > (int) fs / 2) tail = std::max (tail, std::abs (y));
        }
        check ("silence tail decays without residual DC", tail < 1e-8f);
        filter.prepare (fs); bool quiet = true;
        for (int i = 0; i < 1000; ++i) { filter.beginFrame(); quiet = quiet && std::abs (filter.process (0.0f, 0)) < 1e-12f; }
        check ("fresh silence stays zero at maximum Presence", quiet);
        for (int i = 0; i < (int) fs; ++i) { filter.beginFrame(); filter.process (0.1f, 0); }
        bool dc = true;
        for (int i = 0; i < (int) fs; ++i)
        {
            if (i % 503 == 0) filter.setPosition ((float) ((i / 503) % 11));
            filter.beginFrame(); dc = dc && std::abs (filter.process (0.1f, 0) - 0.1f) < 1e-6f;
        }
        check ("constant DC unaffected during control motion", dc);
    }

    // Pre-Presence feature state: missing new parameter must select exact
    // reference=0 even if restoring over a currently bright Lead processor.
    {
        JuicyboiAudioProcessor p; set (p, "preamp_model", 1.0f); set (p, "lead_presence", 9.0f);
        set (p, "pre_presence", 3.0f);
        auto state = p.apvts.copyState(); state.removeChild (state.getChildWithProperty ("id", "lead_presence"), nullptr);
        juce::MemoryBlock binary; juce::AudioProcessor::copyXmlToBinary (*state.createXml(), binary);
        p.setStateInformation (binary.getData(), (int) binary.getSize());
        check ("200471f state migrates to reference, retains model/Rect P", p.apvts.getRawParameterValue ("lead_presence")->load() < 0.01f
               && p.apvts.getRawParameterValue ("preamp_model")->load() > 0.5f
               && std::abs (p.apvts.getRawParameterValue ("pre_presence")->load() - 3.0f) < 0.01f);
        set (p, "lead_presence", 7.3f); p.getStateInformation (binary);
        JuicyboiAudioProcessor restored; restored.setStateInformation (binary.getData(), (int) binary.getSize());
        check ("new Presence value recalls independently", std::abs (restored.apvts.getRawParameterValue ("lead_presence")->load() - 7.3f) < 0.02f
               && std::abs (restored.apvts.getRawParameterValue ("pre_presence")->load() - 3.0f) < 0.01f);
    }

    // Correct model contribution after independent EQ histories. Include EQ
    // enable transitions, model reversals mid-fade and maximum Lead Presence.
    for (bool eq : { false, true })
    {
        JuicyboiAudioProcessor mixed, rect, lead, rectZero;
        configure (mixed, 0.0f, 10.0f, eq); configure (rect, 0.0f, 10.0f, eq);
        configure (lead, 1.0f, 10.0f, eq); configure (rectZero, 0.0f, 0.0f, eq);
        juce::SmoothedValue<float> blend; blend.reset (48000.0, 0.030); blend.setCurrentAndTargetValue (0.0f);
        juce::MidiBuffer midi; float error = 0.0f, legacyError = 0.0f, max = 0.0f; bool finite = true;
        for (int b = 0; b < 220; ++b)
        {
            if (b == 40 || b == 53 || b == 100 || b == 170)
            {
                const auto value = b == 40 || b == 100 ? 1.0f : 0.0f;
                set (mixed, "preamp_model", value); blend.setTargetValue (value);
            }
            if (b == 70 || b == 140)
                for (auto* p : { &mixed, &rect, &lead, &rectZero }) set (*p, "eq_enable", b == 70 ? 1.0f : 0.0f);
            juce::AudioBuffer<float> x (2, 64); x.clear();
            for (int i = 0; i < 64; ++i) x.setSample (0, i, (float) (0.18 * std::sin (2.0 * jb::dsp::kPi * 313.0 * (b * 64 + i) / 48000.0)));
            auto a = x, c = x, d = x;
            mixed.processBlock (x, midi); rect.processBlock (a, midi); lead.processBlock (c, midi); rectZero.processBlock (d, midi);
            for (int i = 0; i < 64; ++i)
            {
                const auto amount = blend.getNextValue();
                const auto expected = a.getSample (0, i) + amount * (c.getSample (0, i) - a.getSample (0, i));
                finite = finite && std::isfinite (x.getSample (0, i)) && std::isfinite (expected);
                error = std::max (error, std::abs (expected - x.getSample (0, i)));
                legacyError = std::max (legacyError, std::abs (a.getSample (0, i) - d.getSample (0, i)));
                max = std::max (max, std::abs (x.getSample (0, i)));
            }
        }
        std::printf ("Presence switch EQ=%d error %.9g, Rect leakage %.9g peak %.4f\n", eq, (double) error, (double) legacyError, (double) max);
        check ("EQ/model transitions blend correct Presence endpoints", finite && std::isfinite (max) && max < 2.0f && error < 3e-6f);
        check ("Lead Presence has zero effect on Rectifier (EQ on/off)", legacyError < 1e-9f);
    }
    return failures;
}
