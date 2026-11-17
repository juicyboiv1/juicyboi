#include "PluginProcessor.h"
#include "LeadFixture.h"
#include <complex>
#include <cstdio>
#include <cstring>

namespace
{
using Complex = std::complex<double>;
// Independent nodal reference: solve the loaded feedback divider directly
// (VR8||C12 in series with R56, presence branch at the PI-tail node) rather
// than reusing production polynomial/TPT code. Declared nominal reduction,
// NOT amplifier measurements. Returns T(Rp,Rr)/T(10k,0).
Complex loop (double rp, double rr, Complex s)
{
    const auto rf = 39000.0 + (rr > 0.0 ? 1.0 / (1.0 / rr + s * 0.0068e-6) : Complex (0.0));
    const auto z = 1.0 / (1.0 / 4700.0 + 1.0 / (rp + 1.0 / (s * 0.133e-6)));
    const auto beta = z / (rf + z);
    return 20.0 / (1.0 + 20.0 * beta);
}
double vr8 (double knob) { return 1.0e6 * (std::pow (10.0, 2.0 * knob / 10.0) - 1.0) / 99.0; }
double vr9 (double knob) { return 10000.0 * (std::pow (10.0, 2.0 * (1.0 - knob / 10.0)) - 1.0) / 99.0; }
Complex reference (double presenceKnob, double resonanceKnob, double hz)
{
    const Complex s (0.0, 2.0 * jb::dsp::kPi * std::max (hz, 1.0e-9));
    return loop (vr9 (presenceKnob), vr8 (resonanceKnob), s) / loop (10000.0, 0.0, s);
}
Complex referenceResonanceOnly (double presenceKnob, double resonanceKnob, double hz)
{
    const Complex s (0.0, 2.0 * jb::dsp::kPi * std::max (hz, 1.0e-9));
    return loop (vr9 (presenceKnob), vr8 (resonanceKnob), s) / loop (vr9 (presenceKnob), 0.0, s);
}
void set (JuicyboiAudioProcessor& p, const char* id, float v)
{
    auto* param = p.apvts.getParameter (id); param->setValueNotifyingHost (param->convertTo0to1 (v));
}
float get (JuicyboiAudioProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
void configure (JuicyboiAudioProcessor& p, float model, float resonance, bool eq)
{
    set (p, "preamp_model", model); set (p, "lead_resonance", resonance); set (p, "lead_presence", 4.0f);
    set (p, "od_enable", 1.0f); set (p, "od_type", 0.0f); set (p, "pre_gain", 9.0f);
    set (p, "eq_enable", eq ? 1.0f : 0.0f); set (p, "eq_100", 5.0f); set (p, "eq_3k2", -4.0f);
    p.setPlayConfigDetails (1, 2, 48000.0, 64); p.prepareToPlay (48000.0, 64);
}
// Measure the production Presence -> Resonance chain at a settled setting.
Complex measure (double fs, float presence, float resonance, double hz)
{
    jb::dsp::LeadPresence pres; pres.setPosition (presence); pres.prepare (fs);
    jb::dsp::LeadResonance res; res.setPosition (resonance); res.prepare (fs, pres.resistance());
    double real = 0.0, imag = 0.0; int count = 0;
    const int n = (int) (2.0 * fs);
    for (int i = 0; i < n; ++i)
    {
        const auto phase = 2.0 * jb::dsp::kPi * hz * (double) i / fs;
        const auto x = (float) (0.01 * std::sin (phase));
        pres.beginFrame(); res.beginFrame (pres.resistance());
        const auto y = res.process (pres.process (x, 0), 0);
        if (i >= n / 2) { real += y * std::sin (phase); imag += y * std::cos (phase); ++count; }
    }
    return { 2.0 * real / (0.01 * count), 2.0 * imag / (0.01 * count) };
}
}

int resonanceValidation()
{
    int failures = 0;
    auto check = [&] (const char* label, bool ok)
    { std::printf ("resonance %-52s %s\n", label, ok ? "ok" : "FAIL"); if (! ok) ++failures; };

    // 1) 54 points (3 rates x 3 knobs x 6 freqs) at Presence 0, plus 36
    //    Presence-interaction points (Presence 5/10 x Resonance 5/10 x 3 rates x 3 freqs).
    double worstComplex = 0.0, worstUnwarpedDb = 0.0; bool finite = true;
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        for (float knob : { 0.0f, 5.0f, 10.0f })
            for (double hz : { 20.0, 100.0, 300.0, 1000.0, 3000.0, 10000.0 })
            {
                const auto m = measure (fs, 0.0f, knob, hz);
                const auto warped = fs / jb::dsp::kPi * std::tan (jb::dsp::kPi * hz / fs);
                const auto e = reference (0.0, knob, warped);
                finite = finite && std::isfinite (m.real()) && std::isfinite (m.imag());
                worstComplex = std::max (worstComplex, std::abs (m - e));
                worstUnwarpedDb = std::max (worstUnwarpedDb, std::abs (20.0 * std::log10 (std::abs (m))
                                                                       - 20.0 * std::log10 (std::abs (reference (0.0, knob, hz)))));
                if (std::abs (fs - 48000.0) < 0.1 && (hz < 150.0 || std::abs (hz - 1000.0) < 0.1))
                    std::printf ("Resonance @48k r=%2.0f f=%5.0f: measured=%7.3f dB, nodal BLT=%7.3f dB\n",
                                 (double) knob, hz, 20.0 * std::log10 (std::abs (m)), 20.0 * std::log10 (std::abs (e)));
            }
        for (float presence : { 5.0f, 10.0f })
            for (float knob : { 5.0f, 10.0f })
                for (double hz : { 40.0, 250.0, 4000.0 })
                {
                    const auto m = measure (fs, presence, knob, hz);
                    const auto warped = fs / jb::dsp::kPi * std::tan (jb::dsp::kPi * hz / fs);
                    worstComplex = std::max (worstComplex, std::abs (m - reference (presence, knob, warped)));
                }
    }
    std::printf ("Resonance worst complex error %.9g; unwarped analog magnitude discrepancy %.6f dB\n", worstComplex, worstUnwarpedDb);
    check ("90 measured points vs loaded nodal model (incl. Presence)", finite && worstComplex < 1e-5 && worstUnwarpedDb < 0.1);

    // Reference sanity: monotone LF lift, HF -> unity, no LF lift at 0.
    check ("LF lift rises with knob, HF converges to reference",
           std::abs (reference (0.0, 0.0, 50.0) - Complex (1.0)) < 1e-12
           && std::abs (reference (0.0, 10.0, 50.0)) > std::abs (reference (0.0, 5.0, 50.0))
           && std::abs (reference (0.0, 5.0, 50.0)) > 1.5
           && std::abs (std::abs (reference (0.0, 10.0, 20000.0)) - 1.0) < 0.01);

    // Low-end table for docs: dB relative to 1 kHz, Presence 0.
    std::printf ("Resonance table (dB re 1 kHz, Presence 0):    40     60     80    100    150    250 | 1k abs\n");
    for (float knob : { 0.0f, 5.0f, 10.0f })
    {
        const auto ref1k = 20.0 * std::log10 (std::abs (referenceResonanceOnly (0.0, knob, 1000.0)));
        std::printf ("  Resonance %4.1f (VR8 %8.0f ohm)             ", (double) knob, vr8 (knob));
        for (double hz : { 40.0, 60.0, 80.0, 100.0, 150.0, 250.0 })
            std::printf ("%6.2f ", 20.0 * std::log10 (std::abs (referenceResonanceOnly (0.0, knob, hz))) - ref1k);
        std::printf ("| %+.2f dB\n", ref1k);
    }

    // 2) Exact reference at 0, each rate, including with Presence engaged.
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        bool exact = true;
        for (float presence : { 0.0f, 7.0f })
        {
            jb::dsp::LeadResonance filter; filter.prepare (fs, vr9 (presence));
            for (int i = 0; i < (int) fs; ++i)
            {
                const float x = (float) (0.2 * std::sin (2.0 * jb::dsp::kPi * 61.0 * i / fs) + 0.001 * (i % 7));
                filter.beginFrame (vr9 (presence));
                const auto y = filter.process (x, 0); filter.process (x, 1);
                exact = exact && std::memcmp (&x, &y, sizeof (float)) == 0;
            }
            // Return to 0 after motion: bit-identical again once the slew lands.
            filter.setPosition (10.0f);
            for (int i = 0; i < 4000; ++i) { filter.beginFrame (vr9 (presence)); filter.process (0.1f, 0); }
            filter.setPosition (0.0f);
            for (int i = 0; i < (int) fs; ++i)
            {
                const float x = (float) (0.2 * std::sin (2.0 * jb::dsp::kPi * 61.0 * i / fs));
                filter.beginFrame (vr9 (presence)); const auto y = filter.process (x, 0);
                if (i > (int) (0.031 * fs)) exact = exact && std::memcmp (&x, &y, sizeof (float)) == 0;
            }
        }
        check ("position zero is bit-identical (each rate, Presence 0/7)", exact);

        // 3) Rapid sweeps of both Resonance and Presence.
        jb::dsp::LeadPresence pres; pres.prepare (fs);
        jb::dsp::LeadResonance res; res.prepare (fs, pres.resistance());
        bool ok = true, stereo = true; float peak = 0.0f, maxStep = 0.0f, previous = 0.0f;
        for (int i = 0; i < (int) (2.0 * fs); ++i)
        {
            if (i % 97 == 0) res.setPosition ((float) ((i / 97) % 11));
            if (i % 389 == 0) pres.setPosition ((float) ((i / 389) % 11));
            pres.beginFrame(); res.beginFrame (pres.resistance());
            const float x = (float) (0.01 * std::sin (2.0 * jb::dsp::kPi * 110.0 * i / fs));
            const auto y = res.process (pres.process (x, 0), 0), right = res.process (pres.process (x, 1), 1);
            ok = ok && std::isfinite (y); stereo = stereo && std::abs (y - right) < 1e-7f;
            peak = std::max (peak, std::abs (y)); maxStep = std::max (maxStep, std::abs (y - previous)); previous = y;
        }
        check ("rapid sweeps stay finite/bounded/stereo/smooth", ok && stereo && peak < 0.05f && maxStep < 0.01f);

        // 4) Silence and DC.
        res.setPosition (10.0f); pres.setPosition (0.0f);
        float tail = 0.0f;
        for (int i = 0; i < (int) fs; ++i)
        {
            pres.beginFrame(); res.beginFrame (pres.resistance());
            const auto y = res.process (0.0f, 0); res.process (0.0f, 1);
            if (i > (int) fs / 2) tail = std::max (tail, std::abs (y));
        }
        check ("silence tail decays without residual DC", tail < 1e-8f);
        jb::dsp::LeadResonance fresh; fresh.setPosition (10.0f); fresh.prepare (fs); bool quiet = true;
        float dc = 0.0f;
        for (int i = 0; i < 1000; ++i) { fresh.beginFrame (10000.0); quiet = quiet && std::abs (fresh.process (0.0f, 0)) < 1e-12f; }
        for (int i = 0; i < (int) fs; ++i) { fresh.beginFrame (10000.0); dc = fresh.process (0.1f, 0); }
        check ("fresh silence zero; settled DC gain matches nodal DC", quiet
               && std::abs (dc / 0.1f - (float) std::abs (reference (0.0, 10.0, 1.0e-6))) < 1e-4f);
    }

    // 5) State: sessions without lead_resonance restore 0; new value recalls.
    {
        JuicyboiAudioProcessor p; set (p, "preamp_model", 1.0f); set (p, "lead_resonance", 8.0f); set (p, "lead_presence", 3.0f);
        auto state = p.apvts.copyState(); state.removeChild (state.getChildWithProperty ("id", "lead_resonance"), nullptr);
        juce::MemoryBlock binary; juce::AudioProcessor::copyXmlToBinary (*state.createXml(), binary);
        p.setStateInformation (binary.getData(), (int) binary.getSize());
        check ("62a1239 state migrates to Resonance 0, keeps Presence/model", get (p, "lead_resonance") < 0.01f
               && std::abs (get (p, "lead_presence") - 3.0f) < 0.02f && get (p, "preamp_model") > 0.5f);
        set (p, "lead_resonance", 6.4f); p.getStateInformation (binary);
        JuicyboiAudioProcessor restored; restored.setStateInformation (binary.getData(), (int) binary.getSize());
        check ("Resonance value recalls independently", std::abs (get (restored, "lead_resonance") - 6.4f) < 0.02f
               && std::abs (get (restored, "lead_presence") - 3.0f) < 0.02f);
        JuicyboiAudioProcessor fresh;
        check ("default lead_resonance is 0 (reference)", get (fresh, "lead_resonance") < 1.0e-9f);
    }

    // 6) Model crossfade endpoints + zero Rectifier leakage, EQ on/off.
    for (bool eq : { false, true })
    {
        JuicyboiAudioProcessor mixed, rect, lead, rectZero;
        configure (mixed, 0.0f, 10.0f, eq); configure (rect, 0.0f, 10.0f, eq);
        configure (lead, 1.0f, 10.0f, eq); configure (rectZero, 0.0f, 0.0f, eq);
        juce::SmoothedValue<float> blend; blend.reset (48000.0, 0.030); blend.setCurrentAndTargetValue (0.0f);
        juce::MidiBuffer midi; float error = 0.0f, leak = 0.0f, max = 0.0f; bool ok = true;
        for (int b = 0; b < 220; ++b)
        {
            if (b == 40 || b == 53 || b == 100 || b == 170)
            {
                const auto value = b == 40 || b == 100 ? 1.0f : 0.0f;
                set (mixed, "preamp_model", value); blend.setTargetValue (value);
            }
            juce::AudioBuffer<float> x (2, 64); x.clear();
            for (int i = 0; i < 64; ++i) x.setSample (0, i, (float) (0.18 * std::sin (2.0 * jb::dsp::kPi * 82.4 * (b * 64 + i) / 48000.0)));
            auto a = x, c = x, d = x;
            mixed.processBlock (x, midi); rect.processBlock (a, midi); lead.processBlock (c, midi); rectZero.processBlock (d, midi);
            for (int i = 0; i < 64; ++i)
            {
                const auto amount = blend.getNextValue();
                const auto expected = a.getSample (0, i) + amount * (c.getSample (0, i) - a.getSample (0, i));
                ok = ok && std::isfinite (x.getSample (0, i));
                error = std::max (error, std::abs (expected - x.getSample (0, i)));
                leak = std::max (leak, std::abs (a.getSample (0, i) - d.getSample (0, i)));
                max = std::max (max, std::abs (x.getSample (0, i)));
            }
        }
        std::printf ("Resonance switch EQ=%d error %.9g, Rect leakage %.9g peak %.4f\n", eq, (double) error, (double) leak, (double) max);
        check ("model transitions blend correct Resonance endpoints", ok && max < 2.0f && error < 3e-6f);
        check ("Lead Resonance has zero effect on Rectifier", leak < 1e-9f);
    }

    // 7) Whole-processor null vs output captured from unmodified 62a1239:
    //    5150 configs at default Resonance, and Rectifier Red/Orange at Bass 5
    //    with other tone knobs/boost/EQ non-default (FMV Bass-centre change).
    {
        juce::File file (juce::String (JUICYBOI_TEST_DIR) + "/lead-62a1239.f32");
        auto in = file.createInputStream();
        const auto perConfig = (size_t) (leadfixture::blocks * leadfixture::blockSize);
        bool ok = in != nullptr && in->getTotalLength() == (juce::int64) (perConfig * leadfixture::numConfigs * sizeof (float));
        for (int c = 0; ok && c < leadfixture::numConfigs; ++c)
        {
            float error = 0.0f; bool fin = true;
            for (auto f : leadfixture::render (c))
            {
                const auto r = in->readFloat();
                fin = fin && std::isfinite (f) && std::isfinite (r);
                error = std::max (error, std::abs (f - r));
            }
            std::printf ("62a1239 null config %d (%s): max error %.9g\n", c, c < 3 ? "5150" : "Rectifier Bass 5", (double) error);
            ok = ok && fin && error < 2.0e-6f;
        }
        check ("5150 default Resonance + Rect Bass 5 null vs 62a1239", ok);
    }

    // 8) Rectifier FMV Bass shelf now acts around 100 Hz; Bass 5 is identity.
    {
        jb::dsp::ToneStack tone; tone.prepare (48000.0); tone.setVoicing (jb::dsp::voicings::red());
        auto gainAt = [&] (float bass, double hz)
        {
            tone.setKnobs (bass, 5.0f, 5.0f, 0.0f); tone.reset();
            jb::dsp::ToneStack flat; flat.prepare (48000.0); flat.setVoicing (jb::dsp::voicings::red());
            flat.setKnobs (5.0f, 5.0f, 5.0f, 0.0f);
            double a = 0.0, b = 0.0;
            for (int i = 0; i < 96000; ++i)
            {
                const auto x = (float) std::sin (2.0 * jb::dsp::kPi * hz * i / 48000.0);
                const auto y = tone.process (x, 0), z = flat.process (x, 0);
                if (i >= 48000) { a += (double) y * y; b += (double) z * z; }
            }
            return 10.0 * std::log10 (a / b);
        };
        const auto b10 = gainAt (10.0f, 100.0), b0 = gainAt (0.0f, 100.0), b10k = gainAt (10.0f, 1000.0);
        std::printf ("Rect Bass shelf: Bass10 @100Hz %+.2f dB, Bass0 @100Hz %+.2f dB, Bass10 @1k %+.2f dB\n", b10, b0, b10k);
        bool exact = true;
        tone.setKnobs (5.0f, 3.0f, 8.0f, 6.0f); tone.reset();
        jb::dsp::ToneStack legacy; legacy.prepare (48000.0);
        auto v = jb::dsp::voicings::red(); v.bassHz = 40.0; legacy.setVoicing (v); legacy.setKnobs (5.0f, 3.0f, 8.0f, 6.0f);
        juce::Random rng (7);
        for (int i = 0; i < 48000; ++i)
        {
            const auto x = rng.nextFloat() * 2.0f - 1.0f;
            const auto y = tone.process (x, 0), z = legacy.process (x, 0);
            exact = exact && std::memcmp (&y, &z, sizeof (float)) == 0;
        }
        check ("Rect Bass centre ~100 Hz: Bass 10 ~+6 dB @100, Bass 5 exact", b10 > 5.5 && b0 < -5.5 && std::abs (b10k) < 0.5 && exact);
    }
    return failures;
}
