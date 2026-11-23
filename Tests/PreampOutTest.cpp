// Preamp Out (preamp_out): Full Amp vs a preamp-only path for a real amp's FX return.
// Default Full Amp must stay bit-identical to the previous build (existing fixtures).
#include "PluginProcessor.h"
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
using P = JuicyboiAudioProcessor;
int failures = 0;
void check (const char* what, bool ok)
{
    std::printf ("preamp-out: %-64s %s\n", what, ok ? "ok" : "FAIL");
    if (! ok) ++failures;
}
void set (P& p, const char* id, float v)
{
    if (auto* a = p.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v));
}
float get (P& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

constexpr int blocks = 48, bs = 256;
double tone (long n, double hz = 110.0)
{
    const double t = (double) n / 48000.0;
    return 0.18 * std::sin (2 * jb::dsp::kPi * hz * t);
}
std::vector<float> render (P& p, int nblocks = blocks)
{
    p.setPlayConfigDetails (1, 2, 48000.0, bs);
    p.prepareToPlay (48000.0, bs);
    juce::MidiBuffer m;
    std::vector<float> out;
    for (int b = 0; b < nblocks; ++b)
    {
        juce::AudioBuffer<float> buf (2, bs);
        for (int i = 0; i < bs; ++i)
        {
            buf.setSample (0, i, (float) tone (b * bs + i));
            buf.setSample (1, i, 0.0f);
        }
        p.processBlock (buf, m);
        out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + bs);
    }
    return out;
}
float maxDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.size() != b.size()) return 1.0e9f;
    float e = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) e = std::max (e, std::abs (a[i] - b[i]));
    return e;
}
void rmsPeak (const std::vector<float>& y, double& rms, double& peak)
{
    double s = 0.0; peak = 0.0;
    for (auto v : y) { s += (double) v * v; peak = std::max (peak, (double) std::abs (v)); }
    rms = std::sqrt (s / (double) y.size());
}
} // namespace

int preampOutValidation()
{
    failures = 0;

    {
        P p;
        auto* last = p.getParameters().getLast();
        auto* po = dynamic_cast<juce::AudioProcessorParameterWithID*> (p.apvts.getParameter ("preamp_out"));
        check ("preamp_out is appended last, default Full Amp, gain is unity",
               po != nullptr && last == po && get (p, "preamp_out") < 0.5f
                   && P::preampOutGain == 1.0f);
    }

    auto prep = [] (P& p, int model, int redVoice)
    {
        set (p, "preamp_model", (float) model);
        set (p, "red_voice", (float) redVoice);
        set (p, "channel", 0.0f);
        set (p, "cab_enable", 0.0f);
        set (p, "m5150_cab_enable", 0.0f);
        set (p, "gate_enable", 0.0f);
        set (p, "m5150_gate_enable", 0.0f);
    };

    // Red v1 / Orange: no speaker load, cab off, unity trim -> Preamp Out is bit-identical
    {
        P full, pre;
        prep (full, 0, 0); prep (pre, 0, 0);
        set (pre, "preamp_out", 1.0f);
        check ("Red v1 (cab off) Preamp Out is bit-identical to Full Amp", maxDiff (render (full), render (pre)) == 0.0f);

        P of, op;
        prep (of, 0, 1); set (of, "channel", 1.0f);
        prep (op, 0, 1); set (op, "channel", 1.0f); set (op, "preamp_out", 1.0f);
        check ("Orange (cab off) Preamp Out is bit-identical to Full Amp", maxDiff (render (of), render (op)) == 0.0f);
    }

    // Red v2 and 5150: speaker-load (and 5150 NFB) come out, so the paths must differ
    {
        P full, pre;
        prep (full, 0, 1); prep (pre, 0, 1); set (pre, "preamp_out", 1.0f);
        const auto e = maxDiff (render (full), render (pre));
        std::printf ("preamp-out: Red v2 Full Amp vs Preamp Out max error %.6g\n", (double) e);
        check ("Red v2 Preamp Out differs (SpeakerLoad bypassed)", e > 1.0e-4f);
    }
    {
        P full, pre;
        prep (full, 1, 1); prep (pre, 1, 1); set (pre, "preamp_out", 1.0f);
        const auto e = maxDiff (render (full), render (pre));
        std::printf ("preamp-out: 5150 Full Amp vs Preamp Out max error %.6g\n", (double) e);
        check ("5150 Preamp Out differs (speaker load / Presence / Resonance bypassed)", e > 1.0e-4f);
    }

    // Bypassed controls are silent in Preamp Out; they still move Full Amp
    {
        P a, b;
        prep (a, 1, 1); prep (b, 1, 1);
        set (a, "preamp_out", 1.0f); set (b, "preamp_out", 1.0f);
        set (b, "lead_presence", 10.0f); set (b, "lead_resonance", 10.0f);
        const auto eNfb = maxDiff (render (a), render (b));
        P cabs; prep (cabs, 1, 1); set (cabs, "preamp_out", 1.0f);
        set (cabs, "m5150_cab_enable", 1.0f); set (cabs, "m5150_cab_level", 9.0f);
        const auto eCab = maxDiff (render (a), render (cabs));
        check ("Preamp Out ignores 5150 Presence, Resonance and cab", eNfb == 0.0f && eCab == 0.0f);

        P c, d;
        prep (c, 0, 1); prep (d, 0, 1);
        set (c, "preamp_out", 1.0f); set (d, "preamp_out", 1.0f);
        set (d, "cab_enable", 1.0f); set (d, "cab_level", 9.0f);
        check ("Preamp Out ignores the Rectifier cab", maxDiff (render (c), render (d)) == 0.0f);
    }

    // Levels at default knobs (cab/gate off), 110 Hz sine. Unity gain: Preamp Out
    // is the post-EQ preamp level, a sensible FX-return line level.
    auto report = [&] (const char* name, int model, int redVoice)
    {
        P full, pre;
        prep (full, model, redVoice); prep (pre, model, redVoice);
        set (pre, "preamp_out", 1.0f);
        double rF = 0, pF = 0, rP = 0, pP = 0;
        rmsPeak (render (full), rF, pF);
        rmsPeak (render (pre), rP, pP);
        const auto db = [] (double x) { return x > 1.0e-12 ? 20.0 * std::log10 (x) : -200.0; };
        const auto dRms = db (rP) - db (rF);
        std::printf ("preamp-out: %-12s  Full Amp   rms %6.2f dBFS  peak %6.2f dBFS\n",
                     name, db (rF), db (pF));
        std::printf ("preamp-out: %-12s  Preamp Out rms %6.2f dBFS  peak %6.2f dBFS  (%+.2f dB rms vs Full Amp)\n",
                     name, db (rP), db (pP), dRms);
        return std::isfinite (rF) && std::isfinite (rP) && rP > 1.0e-4 && rF > 1.0e-4 && pP < 1.0;
    };
    check ("Red v2 default levels are finite, unclipped, sensible for an FX return", report ("Red v2", 0, 1));
    check ("5150 Lead default levels are finite, unclipped, sensible for an FX return", report ("5150 Lead", 1, 1));

    // Click-free 25 ms fade on a 110 Hz tone (same 2nd-difference test as SpeakerLoadTest)
    {
        P proc;
        prep (proc, 0, 1); // Red v2, so the speaker load actually leaves
        proc.setPlayConfigDetails (1, 2, 48000.0, bs);
        proc.prepareToPlay (48000.0, bs);
        juce::MidiBuffer midi;
        std::vector<float> y;
        for (int b = 0; b < 160; ++b)
        {
            if (b == 80) set (proc, "preamp_out", 1.0f);
            juce::AudioBuffer<float> buf (2, bs); buf.clear();
            for (int i = 0; i < bs; ++i)
                buf.setSample (0, i, (float) tone (b * bs + i));
            proc.processBlock (buf, midi);
            y.insert (y.end(), buf.getReadPointer (0), buf.getReadPointer (0) + bs);
        }
        double fade = 0, before = 0, after = 0; bool finite = true;
        for (size_t i = 2; i < y.size(); ++i)
        {
            finite = finite && std::isfinite (y[i]);
            const auto d2 = std::abs ((double) y[i] - 2.0 * y[i - 1] + y[i - 2]);
            const auto blk = i / (size_t) bs;
            if (blk >= 40 && blk < 80) before = std::max (before, d2);
            else if (blk >= 80 && blk < 110) fade = std::max (fade, d2);
            else if (blk >= 120) after = std::max (after, d2);
        }
        std::printf ("preamp-out: Full Amp -> Preamp Out 2nd difference during fade %.5f (steady Full %.5f, Pre %.5f)\n",
                     fade, before, after);
        check ("Full Amp -> Preamp Out switch is finite and click-free",
               finite && fade < 1.25 * std::max (before, after) + 1e-4);
    }

    // Sessions saved before preamp_out load as Full Amp
    {
        P p;
        set (p, "pre_gain", 8.0f);
        auto tree = p.apvts.copyState();
        tree.removeChild (tree.getChildWithProperty ("id", "preamp_out"), nullptr);
        juce::MemoryBlock mb;
        juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), mb);
        set (p, "preamp_out", 1.0f);
        P q;
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        check ("old sessions without preamp_out load as Full Amp",
               get (q, "preamp_out") < 0.5f && std::abs (get (q, "pre_gain") - 8.0f) < 0.02f);
    }
    return failures;
}
