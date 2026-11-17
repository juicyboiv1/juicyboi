// Input/Master 0–10 knob scales (state version 2): mapping anchors, round trips,
// hard mute, legacy dB-state migration (same gain), no double conversion of
// new states, master gain applied after the nonlinear chain, smoothing kept.
#include "PluginProcessor.h"
#if defined(__GNUC__)
 #pragma GCC diagnostic ignored "-Wfloat-equal" // exact unity/mute checks are intended
#endif
#include "LevelTaper.h"
#include <cstdio>

namespace
{
int failures = 0;
void check (const char* what, bool ok)
{
    std::printf ("level taper: %-62s %s\n", what, ok ? "ok" : "FAIL");
    if (! ok) ++failures;
}
void set (JuicyboiAudioProcessor& p, const char* id, float v)
{
    auto* param = p.apvts.getParameter (id); param->setValueNotifyingHost (param->convertTo0to1 (v));
}
float raw (JuicyboiAudioProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
bool relClose (float a, float b, float tol = 2e-6f)
{
    return a == b || std::abs (a - b) <= tol * std::max (std::abs (a), std::abs (b));
}

/** Simulate a state saved before this change: no version property, dB values. */
juce::MemoryBlock legacyState (JuicyboiAudioProcessor& src, float inputDb, float masterDb)
{
    auto state = src.apvts.copyState();
    state.removeProperty (JuicyboiAudioProcessor::stateVersionKey, nullptr);
    state.getChildWithProperty ("id", "input_trim").setProperty ("value", inputDb, nullptr);
    state.getChildWithProperty ("id", "master").setProperty ("value", masterDb, nullptr);
    juce::MemoryBlock b; juce::AudioProcessor::copyXmlToBinary (*state.createXml(), b);
    return b;
}

std::vector<float> render (JuicyboiAudioProcessor& p, int blocks)
{
    juce::MidiBuffer midi; std::vector<float> out; double ph = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        juce::AudioBuffer<float> buf (2, 64);
        for (int i = 0; i < 64; ++i)
        {
            buf.setSample (0, i, (float) (0.2 * std::sin (ph) + 0.05 * std::sin (3.1 * ph)));
            buf.setSample (1, i, 0.0f);
            ph += 2.0 * juce::MathConstants<double>::pi * 110.0 / 48000.0;
        }
        p.processBlock (buf, midi);
        for (int i = 0; i < 64; ++i) out.push_back (buf.getSample (0, i));
    }
    return out;
}
}

int levelTaperValidation()
{
    using namespace jb::taper;
    failures = 0;

    // 1) Anchors
    check ("Input 0/5/10 = -24/0/+24 dB", inputKnobToDb (0) == -24.0 && inputKnobToDb (5) == 0.0 && inputKnobToDb (10) == 24.0);
    check ("Input 5 = unity gain (exact)", inputKnobToGain (5.0f) == 1.0f);
    check ("Master 6.5 = -6 dB, 7.5 = 0 dB, 10 = +12 dB",
           std::abs (masterKnobToDb (6.5) + 6.0) < 1e-12 && std::abs (masterKnobToDb (7.5)) < 1e-12
           && std::abs (masterKnobToDb (10.0) - 12.0) < 1e-12);
    check ("Master 7.5 = unity gain", masterKnobToGain (7.5f) == 1.0f);
    check ("Master 0 = hard mute (gain exactly 0)", masterKnobToGain (0.0f) == 0.0f && masterKnobToGain (-1.0f) == 0.0f);
    check ("Master 0.01 audible but tiny (no -60 dB step artefact)", masterKnobToGain (0.01f) > 0.0f && masterKnobToGain (0.01f) < 0.0011f);
    {
        bool mono = true; double prev = masterKnobToDb (0.0), minStep = 1e9, maxStep = 0;
        for (int i = 1; i <= 10000; ++i)
        {
            const auto d = masterKnobToDb (i * 0.001);
            mono = mono && d > prev; prev = d;
        }
        for (int k = 0; k < 10; ++k)
        {
            const auto step = masterKnobToDb (k + 1) - masterKnobToDb (k);
            minStep = std::min (minStep, step); maxStep = std::max (maxStep, step);
        }
        check ("Master strictly monotonic over 0..10", mono);
        check ("Master 3.5..10.5 dB per number (low end not crammed)", minStep > 3.5 && maxStep < 10.5);
    }

    // 2) Round trips
    {
        bool okIn = true, okMa = true;
        for (int i = 0; i <= 1000; ++i)
        {
            const double x = i * 0.01;
            okIn = okIn && std::abs (inputDbToKnob (inputKnobToDb (x)) - x) < 1e-12;
            if (x > 0) okMa = okMa && std::abs (masterDbToKnob (masterKnobToDb (x)) - x) < 1e-9;
        }
        for (int i = -599; i <= 120; ++i)
        {
            const double d = i * 0.1;
            okMa = okMa && std::abs (masterKnobToDb (masterDbToKnob (d)) - d) < 1e-9;
        }
        check ("Input knob <-> dB round trip", okIn);
        check ("Master knob <-> dB round trip", okMa);
        check ("Master legacy -60 dB (and below) -> knob 0", masterDbToKnob (-60.0) == 0.0 && masterDbToKnob (-80.0) == 0.0);
    }

    // 3) Every legacy 0.1 dB step migrates to the same gain (via float knob)
    {
        bool okIn = true, okMa = true; float worst = 0;
        for (int i = -240; i <= 240; ++i)
        {
            const auto d = (float) i * 0.1f;
            okIn = okIn && relClose (inputKnobToGain ((float) inputDbToKnob (d)), legacyInputGain (d));
        }
        for (int i = -600; i <= 120; ++i)
        {
            const auto d = (float) i * 0.1f;
            const auto a = masterKnobToGain ((float) masterDbToKnob (d)), b = legacyMasterGain (d);
            if (b > 0) worst = std::max (worst, std::abs (a / b - 1.0f));
            okMa = okMa && relClose (a, b, 1e-5f);
        }
        check ("legacy Input dB grid -> same gain", okIn);
        check ("legacy Master dB grid -> same gain (<1e-5 rel, -100 dB)", okMa);
        std::printf ("level taper: worst Master legacy gain error %.2e (rel)\n", worst);
    }

    // 4) Defaults equal old defaults (Input 0 dB, Master -6 dB)
    {
        JuicyboiAudioProcessor p;
        check ("default Input knob 5.0 (= old 0 dB)", raw (p, "input_trim") == 5.0f && inputKnobToGain (raw (p, "input_trim")) == legacyInputGain (0.0f));
        check ("default Master knob 6.5 (= old -6 dB)", raw (p, "master") == 6.5f
               && relClose (masterKnobToGain (raw (p, "master")), legacyMasterGain (-6.0f)));
        check ("display one decimal", p.apvts.getParameter ("master")->getCurrentValueAsText() == "6.5"
               && p.apvts.getParameter ("input_trim")->getCurrentValueAsText() == "5.0");
    }

    // 5) Legacy state migration through setStateInformation
    {
        JuicyboiAudioProcessor src;
        struct Case { float in, ma; } cases[] = { { 0.0f, -6.0f }, { 7.3f, -17.4f }, { -24.0f, 12.0f }, { 24.0f, -60.0f }, { -3.1f, 0.0f }, { 11.9f, -59.9f } };
        bool ok = true;
        for (auto c : cases)
        {
            auto b = legacyState (src, c.in, c.ma);
            JuicyboiAudioProcessor p; set (p, "master", 2.0f); set (p, "input_trim", 9.0f); // load over non-defaults
            p.setStateInformation (b.getData(), (int) b.getSize());
            const auto gi = inputKnobToGain (raw (p, "input_trim")), gm = masterKnobToGain (raw (p, "master"));
            const bool caseOk = relClose (gi, legacyInputGain (c.in), 4e-6f) && relClose (gm, legacyMasterGain (c.ma), 1e-5f);
            if (! caseOk) std::printf ("  case in %.1f dB ma %.1f dB -> knobs %.6f %.6f gains %.9g/%.9g vs %.9g/%.9g\n",
                                       c.in, c.ma, raw (p, "input_trim"), raw (p, "master"), gi, gm, legacyInputGain (c.in), legacyMasterGain (c.ma));
            ok = ok && caseOk;
        }
        check ("legacy sessions load with identical Input/Master gain", ok);

        auto b = legacyState (src, 4.8f, -60.0f);
        JuicyboiAudioProcessor p; p.setStateInformation (b.getData(), (int) b.getSize());
        check ("legacy 4.8 dB -> Input 6.0, legacy -60 dB -> Master 0 (mute)",
               std::abs (raw (p, "input_trim") - 6.0f) < 1e-5f && raw (p, "master") == 0.0f);

        // Re-saving a migrated session must not convert again.
        juce::MemoryBlock again; p.getStateInformation (again);
        JuicyboiAudioProcessor q; q.setStateInformation (again.getData(), (int) again.getSize());
        check ("migrated session re-saves as v2 (no double conversion)",
               std::abs (raw (q, "input_trim") - 6.0f) < 1e-5f && raw (q, "master") == 0.0f
               && (int) q.apvts.state.getProperty (JuicyboiAudioProcessor::stateVersionKey) == stateVersion);

        // New-format states (incl. bare apvts.copyState()) keep knob values.
        JuicyboiAudioProcessor r; set (r, "input_trim", 3.3f); set (r, "master", 8.2f);
        auto st = r.apvts.copyState();
        juce::MemoryBlock nb; juce::AudioProcessor::copyXmlToBinary (*st.createXml(), nb);
        JuicyboiAudioProcessor s; s.setStateInformation (nb.getData(), (int) nb.getSize());
        check ("v2 state recalls knob values unchanged",
               std::abs (raw (s, "input_trim") - 3.3f) < 1e-5f && std::abs (raw (s, "master") - 8.2f) < 1e-5f);
    }

    // 6) Audio: migrated Master is a pure post-chain gain; mute is silent; smoothing kept.
    {
        JuicyboiAudioProcessor ref; ref.setPlayConfigDetails (1, 2, 48000.0, 64); ref.prepareToPlay (48000.0, 64);
        JuicyboiAudioProcessor src;
        auto b = legacyState (src, 0.0f, -18.0f);
        JuicyboiAudioProcessor mig; mig.setStateInformation (b.getData(), (int) b.getSize());
        mig.setPlayConfigDetails (1, 2, 48000.0, 64); mig.prepareToPlay (48000.0, 64);
        const auto a = render (ref, 400), m = render (mig, 400);
        const float ratio = legacyMasterGain (-18.0f) / legacyMasterGain (-6.0f);
        double err = 0, peak = 0;
        for (size_t i = 4800; i < a.size(); ++i) { err = std::max (err, (double) std::abs (m[i] - a[i] * ratio)); peak = std::max (peak, (double) std::abs (a[i])); }
        check ("legacy -18 dB session output = default * -12 dB", peak > 1e-3 && err < 2e-5 * peak);

        set (mig, "master", 0.0f); render (mig, 50);
        const auto silent = render (mig, 20);
        bool zero = true; for (auto v : silent) zero = zero && v == 0.0f;
        check ("Master 0 renders exact silence", zero);

        JuicyboiAudioProcessor z; z.setPlayConfigDetails (1, 2, 48000.0, 64); z.prepareToPlay (48000.0, 64);
        const auto before = render (z, 200);
        float pk = 0; for (size_t i = before.size() - 1200; i < before.size(); ++i) pk = std::max (pk, std::abs (before[i]));
        set (z, "master", 0.0f);
        const auto after = render (z, 30);
        float firstPk = 0; for (int i = 0; i < 32; ++i) firstPk = std::max (firstPk, std::abs (after[(size_t) i]));
        float tail = 0; for (size_t i = after.size() - 256; i < after.size(); ++i) tail = std::max (tail, std::abs (after[i]));
        check ("Master 6.5 -> 0 jump is smoothed (no zipper step), then silent", pk > 1e-3f && firstPk > 0.5f * pk && tail == 0.0f);
    }
    return failures;
}
