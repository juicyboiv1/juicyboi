// Rev2.4 per-amp state: v3 -> v4 migration and set independence (docs/per-amp-state.md).
//
// rev2.3-state-{0..3}.bin are getStateInformation() blobs saved ONCE by the Rev2.3
// build (commit 641dabc) and rev2.3-migration-rect.f32 is what that build rendered
// from each of them (state 3, a 5150 session, with preamp_model switched to the
// Rectifier). Rev3: the renders null against rev3-migration-rect.f32 and stay within
// 1.5 dB of the Rev2.3 renders (the solved triodes change the audio on purpose).
//   0: Red v2, SD-1, EQ, gate, Cab blend/level, Input 5.6, Master 7.1 (all non-default)
//   1: Red v1, OD-808, Master 6     2: Orange, EQ, gate
//   3: 5150 session: Gain 7.7, Bass 6.5, Mid 4.5, Treble 6, Presence 4, Resonance 3,
//      OD-808 (drive 1.5), EQ (200 Hz -2), gate, Master 7, Input 4.5
// Do not regenerate these from feature code to bless a failure.
#include "PluginProcessor.h"
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
using P = JuicyboiAudioProcessor;
int failures = 0;
void check (const char* what, bool ok)
{
    std::printf ("per-amp: %-66s %s\n", what, ok ? "ok" : "FAIL");
    if (! ok) ++failures;
}
void set (P& p, const char* id, float v) { if (auto* a = p.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v)); }
float get (P& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
bool near (float a, float b) { return std::abs (a - b) < 0.011f; }

double sig (long n)
{
    const double t = (double) n / 48000.0;
    const double env = std::fmod (t, 0.25) < 0.15 ? 1.0 : 0.002; // gated chugs (exercises the gate)
    return env * (0.18 * std::sin (2 * jb::dsp::kPi * 82.4 * t) + 0.05 * std::sin (2 * jb::dsp::kPi * 41.2 * t)
                  + 0.04 * std::sin (2 * jb::dsp::kPi * 733.0 * t));
}
constexpr int blocks = 48, bs = 256;
std::vector<float> render (P& p)
{
    p.setPlayConfigDetails (1, 2, 48000.0, bs);
    p.prepareToPlay (48000.0, bs);
    juce::MidiBuffer m; std::vector<float> out;
    for (int b = 0; b < blocks; ++b)
    {
        juce::AudioBuffer<float> buf (2, bs);
        for (int i = 0; i < bs; ++i) { buf.setSample (0, i, (float) sig (b * bs + i)); buf.setSample (1, i, 0.0f); }
        p.processBlock (buf, m);
        out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + bs);
    }
    return out;
}
juce::MemoryBlock blob (int c)
{
    juce::MemoryBlock mb;
    juce::File (juce::String (JUICYBOI_TEST_DIR) + "/rev2.3-state-" + juce::String (c) + ".bin").loadFileAsData (mb);
    return mb;
}
float maxDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.size() != b.size()) return 1.0e9f;
    float e = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) e = std::max (e, std::abs (a[i] - b[i]));
    return e;
}
} // namespace

int perAmpValidation()
{
    failures = 0;

    // 1) Rev2.3 sessions -> Rectifier. Rev3 changes the audio (solved triodes), so the
    //    renders must null against rev3-migration-rect.f32 (captured ONCE from the Rev3
    //    build) and stay within 1.5 dB of what Rev2.3 rendered (rev2.3-migration-rect.f32).
    {
        juce::MemoryBlock ref, ref3;
        juce::File (juce::String (JUICYBOI_TEST_DIR) + "/rev2.3-migration-rect.f32").loadFileAsData (ref);
        const juce::File f3 (juce::String (JUICYBOI_TEST_DIR) + "/rev3-migration-rect.f32");
        const auto per = (size_t) (blocks * bs);
        std::vector<float> all;
        bool boosted[4] {}, eqOn[4] {};
        for (int c = 0; c < 4; ++c)
        {
            auto mb = blob (c);
            P q; q.setStateInformation (mb.getData(), (int) mb.getSize());
            if (c == 3) set (q, "preamp_model", 0.0f);
            boosted[c] = get (q, "od_enable") > 0.5f;
            eqOn[c] = get (q, "eq_enable") > 0.5f;
            const auto y = render (q);
            all.insert (all.end(), y.begin(), y.end());
        }
        // Rev3.1: rev3.1-migration-rect.f32 captured ONCE from the Rev3.1 build; Boost-off
        // states must stay bit-identical to Rev3, boosted ones within 1 dB of Rev3.
        const juce::File f31 (juce::String (JUICYBOI_TEST_DIR) + "/rev3.1-migration-rect.f32");
        if (std::getenv ("JB_CAPTURE_REV31") != nullptr && ! f31.existsAsFile())
            f31.replaceWithData (all.data(), all.size() * sizeof (float));
        juce::MemoryBlock ref31, ref32;
        f31.loadFileAsData (ref31);
        // Rev3.2: rev3.2-migration-rect.f32 captured ONCE from the Rev3.2 build (GEQ-7
        // circuit EQ); EQ-on states within 2 dB of Rev3.1 (the GE-7 bands are broad and
        // the circuit loses ~0.5 dB flat, so a -2 dB 200 Hz cut in front of the Rectifier reads -1.5 dB), EQ-off ones still null vs Rev3.1.
        const juce::File f32 (juce::String (JUICYBOI_TEST_DIR) + "/rev3.2-migration-rect.f32");
        if (std::getenv ("JB_CAPTURE_REV32") != nullptr && ! f32.existsAsFile())
            f32.replaceWithData (all.data(), all.size() * sizeof (float));
        f32.loadFileAsData (ref32);
        f3.loadFileAsData (ref3);
        bool ok = ref.getSize() == per * 4 * sizeof (float) && ref3.getSize() == ref.getSize() && ref31.getSize() == ref.getSize() && ref32.getSize() == ref.getSize();
        for (int c = 0; ok && c < 4; ++c)
        {
            const auto* r = static_cast<const float*> (ref.getData()) + per * (size_t) c;
            const auto* r3 = static_cast<const float*> (ref3.getData()) + per * (size_t) c;
            const auto* r31 = static_cast<const float*> (ref31.getData()) + per * (size_t) c;
            const auto* r32 = static_cast<const float*> (ref32.getData()) + per * (size_t) c;
            float e = 0.0f, e31 = 0.0f, e32 = 0.0f; double s = 0.0, a = 0.0, s3 = 0.0, s31 = 0.0;
            for (size_t i = 0; i < per; ++i)
            {
                const auto y = all[per * (size_t) c + i];
                e = std::max (e, std::abs (y - r3[i]));
                e31 = std::max (e31, std::abs (y - r31[i]));
                e32 = std::max (e32, std::abs (y - r32[i]));
                if (i >= per / 2) { s += (double) r[i] * r[i]; a += (double) y * y; s3 += (double) r3[i] * r3[i]; s31 += (double) r31[i] * r31[i]; }
            }
            const auto d = 10.0 * std::log10 (a / s), d3 = 10.0 * std::log10 (a / s3), d31 = 10.0 * std::log10 (a / s31);
            std::printf ("per-amp: Rev2.3 state %d -> Rectifier (%s, EQ %s): null vs Rev3.2 %.9g, vs Rev3.1 %.9g (%+.2f dB), vs Rev3 %.9g (%+.2f dB), level vs Rev2.3 %+.2f dB\n",
                         c, boosted[c] ? "boost on" : "boost off", eqOn[c] ? "on" : "off", (double) e32, (double) e31, d31, (double) e, d3, d);
            ok = ok && e32 < 2.0e-6f && (eqOn[c] ? std::abs (d31) < 2.0 : e31 < 2.0e-6f)
                 && (boosted[c] ? (eqOn[c] || std::abs (d3) < 1.0) : (eqOn[c] || ! (e > 0.0f))) && std::abs (d) < (eqOn[c] ? 3.0 : 1.5) && s > 0.5;
        }
        check ("Rev2.3 sessions load: Rectifier nulls vs Rev3.2 (EQ-off null vs Rev3.1, EQ-on within 2 dB), within 1.5 dB of Rev2.3 (EQ-on 3 dB)", ok);
    }

    // 2) migration copies the old shared values into the 5150 set
    {
        auto mb = blob (0);
        P q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        check ("v3 Red v2 session: 5150 set inherits boost/EQ/cab/gate/input",
               get (q, "m5150_od_enable") > 0.5f && get (q, "m5150_od_type") > 0.5f && near (get (q, "m5150_od_drive"), 4.2f)
                   && near (get (q, "m5150_od_tone"), 6.1f) && near (get (q, "m5150_od_level"), 7.3f) && near (get (q, "m5150_input_trim"), 5.6f)
                   && get (q, "m5150_eq_enable") > 0.5f && near (get (q, "m5150_eq_400"), -4.5f) && near (get (q, "m5150_eq_level"), 1.5f)
                   && near (get (q, "m5150_cab_mic_blend"), 0.3f) && near (get (q, "m5150_cab_level"), 5.4f)
                   && get (q, "m5150_gate_enable") > 0.5f && near (get (q, "m5150_gate_threshold"), -52.0f)
                   && near (get (q, "m5150_gate_hold"), 40.0f) && near (get (q, "m5150_gate_release"), 90.0f));
        check ("v3 session: amp knobs map Gain->Lead Pre, Bass/Mid/Treble->Low/Mid/High",
               near (get (q, "m5150_lead_pre"), 7.3f) && near (get (q, "m5150_low"), 6.2f) && near (get (q, "m5150_mid"), 3.1f)
                   && near (get (q, "m5150_high"), 6.8f));
        check ("v3 session: Master -> Lead/Rhythm Post (same small-signal gain), Lead channel",
               near (get (q, "m5150_lead_post"), P::leadPostForMaster (7.1f)) && near (get (q, "m5150_rhythm_post"), P::leadPostForMaster (7.1f))
                   && get (q, "m5150_channel") > 0.5f && get (q, "m5150_bright") < 0.5f && get (q, "m5150_crunch") < 0.5f
                   && near (get (q, "m5150_rhythm_pre"), 5.0f));

        auto mb3 = blob (3);
        P r; r.setStateInformation (mb3.getData(), (int) mb3.getSize());
        check ("v3 5150 session: stays on the 5150 with its knobs, boost, EQ, gate",
               get (r, "preamp_model") > 0.5f && near (get (r, "m5150_lead_pre"), 7.7f) && near (get (r, "m5150_low"), 6.5f)
                   && near (get (r, "m5150_mid"), 4.5f) && near (get (r, "m5150_high"), 6.0f) && near (get (r, "lead_presence"), 4.0f)
                   && near (get (r, "lead_resonance"), 3.0f) && get (r, "m5150_od_enable") > 0.5f && near (get (r, "m5150_od_drive"), 1.5f)
                   && near (get (r, "m5150_eq_200"), -2.0f) && get (r, "m5150_gate_enable") > 0.5f && near (get (r, "m5150_input_trim"), 4.5f)
                   && near (get (r, "m5150_lead_post"), P::leadPostForMaster (7.0f))
                   // ...and the Rectifier set is untouched
                   && near (get (r, "pre_gain"), 7.7f) && near (get (r, "master"), 7.0f) && get (r, "od_enable") > 0.5f);
        check ("Master 6.5 (old default) <-> Post 5 (new default)", std::abs (P::leadPostForMaster (6.5f) - 5.0f) < 0.01f);

        // re-saved v4 state is not migrated again
        set (r, "m5150_lead_pre", 2.0f); set (r, "m5150_od_enable", 0.0f);
        juce::MemoryBlock again; r.getStateInformation (again);
        P r2; r2.setStateInformation (again.getData(), (int) again.getSize());
        check ("re-saved v4 session keeps independent sets (no re-migration)",
               near (get (r2, "m5150_lead_pre"), 2.0f) && get (r2, "m5150_od_enable") < 0.5f && near (get (r2, "pre_gain"), 7.7f)
                   && get (r2, "od_enable") > 0.5f && (int) r2.apvts.state.getProperty (P::stateVersionKey) == 4);
    }

    // 3) the inactive set has no effect on the active amp's audio (both directions)
    {
        auto changeRect = [] (P& p)
        {
            // input_trim is the shared calibration (not a Rectifier-only knob)
            for (auto* id : { "pre_gain", "pre_bass", "pre_mid", "pre_treble", "pre_presence", "master" }) set (p, id, 9.0f);
            set (p, "od_enable", 1.0f); set (p, "od_type", 1.0f); set (p, "eq_enable", 1.0f); set (p, "eq_100", 12.0f);
            set (p, "gate_enable", 1.0f); set (p, "gate_threshold", -20.0f); set (p, "cab_level", 9.0f); set (p, "channel", 1.0f);
        };
        auto change5150 = [] (P& p)
        {
            for (auto* id : { "m5150_input_trim", "m5150_lead_pre", "m5150_rhythm_pre", "m5150_low", "m5150_mid", "m5150_high",
                              "m5150_lead_post", "m5150_rhythm_post", "lead_presence", "lead_resonance" }) set (p, id, 9.0f);
            set (p, "m5150_od_enable", 1.0f); set (p, "m5150_eq_enable", 1.0f); set (p, "m5150_eq_100", 12.0f);
            set (p, "m5150_gate_enable", 1.0f); set (p, "m5150_gate_threshold", -20.0f); set (p, "m5150_cab_level", 9.0f);
            set (p, "m5150_channel", 0.0f); set (p, "m5150_crunch", 1.0f); set (p, "m5150_bright", 1.0f);
        };
        P a, b, c, d;
        set (a, "preamp_model", 1.0f); set (b, "preamp_model", 1.0f); changeRect (b);
        check ("Rectifier settings do not touch the 5150's audio", maxDiff (render (a), render (b)) == 0.0f);
        change5150 (d);
        check ("5150 settings do not touch the Rectifier's audio", maxDiff (render (c), render (d)) == 0.0f);
    }

    // 4) switching amps recalls each amp's own settings
    {
        P p;
        set (p, "pre_gain", 3.0f); set (p, "od_enable", 1.0f); set (p, "eq_800", 4.0f);
        set (p, "preamp_model", 1.0f);
        set (p, "m5150_lead_pre", 8.0f); set (p, "m5150_od_enable", 0.0f); set (p, "m5150_eq_800", -6.0f);
        set (p, "preamp_model", 0.0f);
        check ("switching amps keeps each amp's knobs, boost and EQ",
               near (get (p, "pre_gain"), 3.0f) && get (p, "od_enable") > 0.5f && near (get (p, "eq_800"), 4.0f)
                   && near (get (p, "m5150_lead_pre"), 8.0f) && get (p, "m5150_od_enable") < 0.5f && near (get (p, "m5150_eq_800"), -6.0f));
    }

    // 5) shared input calibration (input_trim); m5150_input_trim is unused
    {
        auto withTrim = [] (int model, float shared, float unused5150)
        {
            P p;
            set (p, "preamp_model", (float) model);
            set (p, "input_trim", shared);
            set (p, "m5150_input_trim", unused5150);
            return render (p);
        };
        check ("m5150_input_trim does not touch the Rectifier's audio",
               maxDiff (withTrim (0, 5.6f, 2.0f), withTrim (0, 5.6f, 9.0f)) == 0.0f);
        check ("m5150_input_trim does not touch the 5150's audio",
               maxDiff (withTrim (1, 5.6f, 2.0f), withTrim (1, 5.6f, 9.0f)) == 0.0f);
        check ("input_trim is shared: it changes the 5150's audio",
               maxDiff (withTrim (1, 5.6f, 5.6f), withTrim (1, 2.0f, 5.6f)) > 1.0e-4f);
        check ("input_trim is shared: it changes the Rectifier's audio",
               maxDiff (withTrim (0, 5.6f, 5.6f), withTrim (0, 2.0f, 5.6f)) > 1.0e-4f);

        // A Rev2.4 session that had a different 5150 Input now uses the shared
        // (Rectifier / legacy input_trim) value.
        P diverged;
        set (diverged, "input_trim", 5.6f);
        set (diverged, "m5150_input_trim", 2.0f);
        juce::MemoryBlock v4; diverged.getStateInformation (v4);
        P loaded; loaded.setStateInformation (v4.getData(), (int) v4.getSize());
        set (loaded, "preamp_model", 1.0f);
        P expected; set (expected, "preamp_model", 1.0f); set (expected, "input_trim", 5.6f); set (expected, "m5150_input_trim", 9.0f);
        check ("v4 session with a different 5150 Input uses the shared (Rectifier) input_trim",
               near (get (loaded, "input_trim"), 5.6f) && maxDiff (render (loaded), render (expected)) == 0.0f);
    }
    return failures;
}
