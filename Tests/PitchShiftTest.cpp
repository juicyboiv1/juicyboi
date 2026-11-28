// Rev3.3 PITCH pedal (docs/pitch-shift.md): shifted pitch for every interval
// (mono grain shifter + poly phase vocoder) measured with the tuner's
// PitchDetector and Goertzel/FFT bins, the toggle logic, true bypass and
// bit-identity vs v3.2.2 (rev3.2.2-bypass.f32), click-free parameter moves,
// NaN / denormal safety, latency, per-amp independence and CPU cost.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Parameters.h"
#include "dsp/PitchShifter.h"
#include "dsp/PitchDetector.h"
#include "BypassRender.h"
#include <chrono>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <vector>

namespace
{
int failures = 0;
void check (const char* what, bool ok)
{
    std::printf ("pitch: %-72s %s\n", what, ok ? "ok" : "FAIL");
    if (! ok) ++failures;
}
void check (const juce::String& what, bool ok) { check (what.toRawUTF8(), ok); }

constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
using Pedal = jb::dsp::PitchPedal;
using S = Pedal::Settings;

/** Guitar-ish harmonic tone (fundamental + 2nd + 3rd). */
std::vector<float> tone (double fs, double f0, int n, float amp = 0.3f)
{
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs;
        x[(size_t) i] = (float) (amp * (std::sin (kTwoPi * f0 * t) + 0.5 * std::sin (kTwoPi * 2 * f0 * t + 0.3)
                                        + 0.25 * std::sin (kTwoPi * 3 * f0 * t + 1.1)) / 1.75);
    }
    return x;
}

std::vector<float> runPedal (Pedal& p, std::vector<float> x, int block = 256, int chans = 1)
{
    std::vector<float> y2 (x);
    for (size_t s = 0; s < x.size(); s += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, x.size() - s);
        float* ch[2] { x.data() + s, y2.data() + s };
        p.process (ch, chans, n);
    }
    return x;
}

Pedal makePedal (double fs, const S& s, int maxBlock = 256, int chans = 1)
{
    Pedal p;
    p.setParameters (s);
    p.prepare (fs, maxBlock, chans);
    return p;
}

double detectHz (const std::vector<float>& y, double fs, int from)
{
    jb::dsp::PitchDetector d;
    d.prepare (fs, 2048, 4096, 25.0, 2100.0);
    if ((int) y.size() < from + d.frameLength()) return 0.0;
    // median of several frames
    std::vector<double> hz;
    for (int k = 0; k < 5; ++k)
    {
        const int st = from + k * 2400;
        if (st + d.frameLength() > (int) y.size()) break;
        const auto r = d.detect (y.data() + st);
        if (r.valid) hz.push_back (r.hz);
    }
    if (hz.empty()) return 0.0;
    std::sort (hz.begin(), hz.end());
    return hz[hz.size() / 2];
}

double goertzelDb (const std::vector<float>& y, double fs, double f, int from, int len)
{
    double sr = 0, si = 0;
    for (int i = 0; i < len; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (kTwoPi * i / (len - 1));
        const double ph = kTwoPi * f * (from + i) / fs;
        sr += w * y[(size_t) (from + i)] * std::cos (ph);
        si -= w * y[(size_t) (from + i)] * std::sin (ph);
    }
    return 20.0 * std::log10 (std::sqrt (sr * sr + si * si) / len + 1e-20);
}

double rmsDb (const std::vector<float>& y, int from, int len)
{
    double e = 0;
    for (int i = 0; i < len; ++i) e += (double) y[(size_t) (from + i)] * y[(size_t) (from + i)];
    return 10.0 * std::log10 (e / len + 1e-30);
}

double cents (double hz, double ref) { return 1200.0 * std::log2 (hz / ref); }

/** Spectral peak (Hann-windowed DFT magnitude maximised around `guess`, golden search). */
double spectralPeakHz (const std::vector<float>& y, double fs, double guess, int from, int len)
{
    auto mag = [&] (double f) { return goertzelDb (y, fs, f, from, len); };
    double a = guess * std::pow (2.0, -40.0 / 1200.0), b = guess * std::pow (2.0, 40.0 / 1200.0);
    // coarse scan first
    double best = a, bm = -1e9;
    for (int k = 0; k <= 80; ++k) { const double f = a * std::pow (b / a, k / 80.0); const double m = mag (f); if (m > bm) { bm = m; best = f; } }
    a = best * std::pow (2.0, -1.0 / 1200.0); b = best * std::pow (2.0, 1.0 / 1200.0);
    for (int it = 0; it < 40; ++it)
    {
        const double c = b - (b - a) * 0.618, d = a + (b - a) * 0.618;
        if (mag (c) > mag (d)) b = d; else a = c;
    }
    return 0.5 * (a + b);
}

bool finiteNoDenormal (const std::vector<float>& y)
{
    for (auto v : y)
        if (! std::isfinite (v) || (v != 0.0f && std::abs (v) < FLT_MIN)) return false;
    return true;
}

float maxStep (const std::vector<float>& y, int from, int to)
{
    float m = 0.0f;
    for (int i = std::max (1, from); i < to; ++i) m = std::max (m, std::abs (y[(size_t) i] - y[(size_t) i - 1]));
    return m;
}

/** Energy-centroid delay of a windowed tone burst (samples). */
double burstDelay (Pedal& p, double fs, double f)
{
    const int n = (int) (fs * 0.4), b0 = (int) (fs * 0.1), bl = (int) (fs * 0.04);
    std::vector<float> x ((size_t) n, 0.0f);
    for (int i = 0; i < bl; ++i)
        x[(size_t) (b0 + i)] = (float) (0.3 * std::sin (kTwoPi * f * i / fs) * (0.5 - 0.5 * std::cos (kTwoPi * i / (bl - 1))));
    const auto y = runPedal (p, x);
    double ex = 0, tx = 0, ey = 0, ty = 0;
    for (int i = 0; i < n; ++i)
    {
        ex += (double) x[(size_t) i] * x[(size_t) i]; tx += i * (double) x[(size_t) i] * x[(size_t) i];
        ey += (double) y[(size_t) i] * y[(size_t) i]; ty += i * (double) y[(size_t) i] * y[(size_t) i];
    }
    return ty / ey - tx / ex;
}

void setP (JuicyboiAudioProcessor& p, const char* id, float plain) { jbtest::bypassSet (p, id, plain); }

/** JB_PITCH_ONLY=4,7 runs only those sections (development aid). */
bool section (int k)
{
    const auto* only = std::getenv ("JB_PITCH_ONLY");
    if (only == nullptr) return true;
    return juce::StringArray::fromTokens (only, ",", "").contains (juce::String (k));
}
} // namespace

int pitchShiftValidation()
{
    failures = 0;
    juce::ScopedNoDenormals noDenormals;
    using PS = jb::params::PitchSlot;

    // 1) parameters: appended after the cab filters, pedal off by default
    if (section (1))
    {
        JuicyboiAudioProcessor p;
        p.stopTunerThread();
        const auto& ps = p.getParameters();
        int idxLpf5 = -1;
        for (int i = 0; i < ps.size(); ++i)
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (ps[i]))
                if (r->getParameterID() == "m5150_cab_lpf") idxLpf5 = i;
        bool order = idxLpf5 >= 0 && ps.size() == idxLpf5 + 1 + 2 * jb::params::numPitchSlots;
        for (int a = 0; order && a < 2; ++a)
            for (int s = 0; s < jb::params::numPitchSlots; ++s)
            {
                auto* r = dynamic_cast<juce::RangedAudioParameter*> (ps[idxLpf5 + 1 + a * jb::params::numPitchSlots + s]);
                order = order && r != nullptr && r->getParameterID() == jb::params::ampPitchId (a, (PS) s);
            }
        check ("18 PITCH parameters appended last (after m5150_cab_lpf), amp 0 then m5150_", order);
        auto get = [&] (const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
        const bool defs = get ("pitch_on") < 0.5f && get ("m5150_pitch_on") < 0.5f && get ("pitch_shift") == 12.0f
                          && get ("pitch_transpose") > 0.5f && get ("pitch_whammy") < 0.5f && get ("pitch_poly") < 0.5f
                          && get ("pitch_harmony") < 0.5f && get ("pitch_sweep") == 1.0f && get ("pitch_detune") == 0.0f;
        check ("defaults: off; Transpose on, Shift +12, Sweep toe, Detune 0, Mix 50", defs && get ("pitch_mix") == 50.0f);
        const auto latBefore = (p.prepareToPlay (48000.0, 256), p.getLatencySamples());
        setP (p, "pitch_on", 1.0f); setP (p, "pitch_poly", 1.0f);
        p.prepareToPlay (48000.0, 256);
        check (juce::String ("host latency unchanged by PITCH / Poly (") + juce::String (latBefore) + " samples)", p.getLatencySamples() == latBefore);
    }

    // 2) old sessions load with the pedal off
    if (section (2))
    {
        JuicyboiAudioProcessor p;
        p.stopTunerThread();
        auto xml = p.apvts.copyState().createXml();
        for (int a = 0; a < 2; ++a)
            for (int s = 0; s < jb::params::numPitchSlots; ++s)
                if (auto* e = xml->getChildByAttribute ("id", jb::params::ampPitchId (a, (PS) s)))
                    xml->removeChildElement (e, true);
        juce::MemoryBlock mb;
        juce::AudioProcessor::copyXmlToBinary (*xml, mb);
        JuicyboiAudioProcessor q;
        q.stopTunerThread();
        setP (q, "pitch_on", 1.0f); setP (q, "m5150_pitch_on", 1.0f); setP (q, "pitch_shift", -5.0f);
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool off = q.apvts.getRawParameterValue ("pitch_on")->load() < 0.5f
                         && q.apvts.getRawParameterValue ("m5150_pitch_on")->load() < 0.5f
                         && q.apvts.getRawParameterValue ("pitch_shift")->load() == 12.0f;
        check ("a pre-Rev3.3 session loads with both PITCH pedals off (defaults)", off);
    }

    // 3) bit-identity vs v3.2.2 with the pedal off (fixture captured once from 297dbd4)
    if (section (3))
    {
        juce::MemoryBlock ref;
        const bool have = juce::File (juce::String (JUICYBOI_TEST_DIR) + "/rev3.2.2-bypass.f32").loadFileAsData (ref)
                          && ref.getSize() == sizeof (float) * (size_t) jbtest::bypassSamples * jbtest::bypassConfigs;
        bool identDefault = have, identTweaked = have;
        for (int c = 0; have && c < jbtest::bypassConfigs; ++c)
        {
            const auto* r = static_cast<const float*> (ref.getData()) + (size_t) c * jbtest::bypassSamples;
            const auto y = jbtest::bypassRender (c);
            // PITCH settings changed but the pedal left off: still untouched
            const auto z = jbtest::bypassRender (c, [] (JuicyboiAudioProcessor& p)
            {
                for (int a = 0; a < 2; ++a)
                {
                    setP (p, jb::params::ampPitchId (a, PS::shift), -7.0f);
                    setP (p, jb::params::ampPitchId (a, PS::poly), 1.0f);
                    setP (p, jb::params::ampPitchId (a, PS::whammy), 1.0f);
                    setP (p, jb::params::ampPitchId (a, PS::harmony), 1.0f);
                    setP (p, jb::params::ampPitchId (a, PS::sweep), 0.3f);
                }
            });
            for (int i = 0; i < jbtest::bypassSamples; ++i)
            {
                identDefault = identDefault && std::memcmp (&y[(size_t) i], &r[i], sizeof (float)) == 0;
                identTweaked = identTweaked && std::memcmp (&z[(size_t) i], &r[i], sizeof (float)) == 0;
            }
        }
        check ("PITCH off: 4 scenarios (Red v2, Orange+SD-1+EQ+cab, 5150 Lead, Rhythm/Preamp Out) bit-identical to v3.2.2", identDefault);
        check ("PITCH off with every PITCH knob / mode changed: still bit-identical to v3.2.2", identTweaked);
    }

    // 4) shifted pitch for every interval, mono and poly
    if (section (4))
    {
        const double fs = 48000.0;
        const int n = (int) (fs * 0.8);
        const int ints[] = { -24, -12, -7, -5, -2, -1, 1, 2, 5, 7, 12, 24 };
        for (int engine = 0; engine < 2; ++engine)
        {
            double worst = 0.0, worstLvl = 0.0;
            bool allOk = true;
            juce::String detail;
            for (int st : ints)
            {
                const double f0 = st < -12 ? 330.0 : (st > 12 ? 110.0 : 196.0);
                S s; s.on = true; s.transpose = true; s.shift = (float) st; s.poly = engine == 1;
                auto p = makePedal (fs, s);
                const auto x = tone (fs, f0, n);
                const auto y = runPedal (p, x);
                const auto hz = detectHz (y, fs, (int) (fs * 0.25));
                const auto want = f0 * std::pow (2.0, st / 12.0);
                const auto err = hz > 0 ? cents (hz, want) : 9999.0;
                const auto errSpec = cents (spectralPeakHz (y, fs, want, (int) (fs * 0.3), (int) (fs * 0.45)), want);
                const auto lvl = rmsDb (y, (int) (fs * 0.3), (int) (fs * 0.4)) - rmsDb (x, (int) (fs * 0.3), (int) (fs * 0.4));
                const bool ok = std::abs (err) < 1.0 && std::abs (errSpec) < 1.0 && std::abs (lvl) < 3.0 && finiteNoDenormal (y);
                allOk = allOk && ok;
                worst = std::max ({ worst, std::abs (err), std::abs (errSpec) });
                worstLvl = std::max (worstLvl, std::abs (lvl));
                detail << (st > 0 ? "+" : "") << st << ":" << juce::String (err, 2) << "c ";
            }
            std::printf ("pitch: %s cents error per interval: %s\n", engine ? "poly" : "mono", detail.toRawUTF8());
            check (juce::String (engine ? "poly" : "mono") + ": -24..+24 st within 1 cent by detector AND spectral peak (worst "
                       + juce::String (worst, 2) + " c), level within 3 dB (worst " + juce::String (worstLvl, 2) + " dB)", allOk);
        }

        // Poly chord: A2 + E3 + A3 up an octave: the old partials must be gone
        {
            S s; s.on = true; s.transpose = true; s.shift = 12.0f; s.poly = true;
            auto p = makePedal (fs, s);
            std::vector<float> x ((size_t) n);
            const double fr[] = { 110.0, 164.81, 220.0 * 1.0 };
            for (int i = 0; i < n; ++i)
            {
                double v = 0;
                for (double f : fr) v += std::sin (kTwoPi * f * i / fs) + 0.4 * std::sin (kTwoPi * 3 * f * i / fs);
                x[(size_t) i] = (float) (0.1 * v);
            }
            const auto y = runPedal (p, x);
            const int from = (int) (fs * 0.3), len = 16384;
            double newMin = 1e9, oldMax = -1e9;
            for (double f : { 220.0, 329.62, 440.0 }) newMin = std::min (newMin, goertzelDb (y, fs, f, from, len));
            for (double f : { 110.0, 164.81 }) oldMax = std::max (oldMax, goertzelDb (y, fs, f, from, len));
            const auto inRef = goertzelDb (x, fs, 110.0, from, len);
            check (juce::String ("poly chord +12: new partials ") + juce::String (newMin - inRef, 1) + " dB re input, old ones "
                       + juce::String (oldMax - inRef, 1) + " dB (> 30 dB apart)", newMin - oldMax > 30.0 && newMin - inRef > -4.0);
        }
    }

    // 5) toggle logic: Detune, Whammy sweep, Harmony mix, all-off passthrough
    if (section (5))
    {
        const double fs = 48000.0;
        const int n = (int) (fs * 0.8);
        const auto x = tone (fs, 196.0, n);
        auto measure = [&] (S s)
        {
            s.on = true;
            auto p = makePedal (fs, s);
            const auto y = runPedal (p, x);
            return cents (detectHz (y, fs, (int) (fs * 0.25)), 196.0);
        };
        { S s; s.shift = 0; s.detuneCents = 50; check (juce::String ("Transpose 0 st + Detune +50 c -> ") + juce::String (measure (s), 2) + " c", std::abs (measure (s) - 50.0) < 3.0); }
        { S s; s.shift = -2; s.detuneCents = -25; check (juce::String ("Transpose -2 st + Detune -25 c -> ") + juce::String (measure (s), 2) + " c (want -225)", std::abs (measure (s) + 225.0) < 3.0); }
        { S s; s.whammy = true; s.transpose = false; s.shift = 12; s.sweep = 0.5f; check (juce::String ("Whammy Shift +12, Sweep 50% -> ") + juce::String (measure (s), 1) + " c (want +600)", std::abs (measure (s) - 600.0) < 4.0); }
        { S s; s.whammy = true; s.transpose = false; s.shift = -12; s.sweep = 1.0f; check ("Whammy Shift -12, toe -> -1200 c", std::abs (measure (s) + 1200.0) < 3.0); }
        { S s; s.whammy = true; s.transpose = false; s.shift = 7; s.sweep = 0.75f; s.detuneCents = 40; // Detune ignored without Transpose
          check (juce::String ("Whammy alone ignores Detune: Shift +7 x 0.75 -> ") + juce::String (measure (s), 1) + " c (want +525)", std::abs (measure (s) - 525.0) < 4.0); }
        { S s; s.whammy = true; s.transpose = true; s.shift = 12; s.sweep = 0.5f; s.detuneCents = 20;
          check ("Whammy + Transpose: Sweep x Shift + Detune (+620 c)", std::abs (measure (s) - 620.0) < 4.0); }

        auto exactDry = [&] (S s, bool idle = true)
        {
            s.on = true;
            auto p = makePedal (fs, s);
            const auto y = runPedal (p, x);
            bool same = true;
            for (int i = (int) (fs * 0.1); i < n; ++i) same = same && y[(size_t) i] == x[(size_t) i];
            return same && (! idle || (! p.isMonoRunning() && ! p.isPolyRunning()));
        };
        { S s; s.whammy = false; s.transpose = false; s.harmony = false; s.poly = false; check ("all four modes off, pedal on: exact dry passthrough, no engine running", exactDry (s)); }
        { S s; s.whammy = false; s.transpose = false; s.harmony = true; s.poly = true; check ("Harmony + Poly only (no interval source): exact dry passthrough", exactDry (s)); }
        { S s; s.whammy = true; s.transpose = false; s.shift = 12; s.sweep = 0.0f; check ("Whammy heel (Sweep 0): exact dry signal, no latency (engine kept warm)", exactDry (s, false)); }
        { S s; s.transpose = true; s.shift = 7; s.harmony = true; s.mix = 0.0f; s.on = true;
          auto p = makePedal (fs, s); const auto y = runPedal (p, x);
          bool same = true; for (int i = 0; i < n; ++i) same = same && y[(size_t) i] == x[(size_t) i];
          check ("Harmony Mix 0%: exact dry", same); }
        { S s; s.transpose = true; s.shift = 12; s.harmony = true; s.mix = 50.0f; s.on = true;
          auto p = makePedal (fs, s); const auto y = runPedal (p, x);
          const int from = (int) (fs * 0.3), len = 16384;
          const auto dryDb = goertzelDb (y, fs, 196.0, from, len), upDb = goertzelDb (y, fs, 392.0 * 2.0 /* 3rd harmonic of dry = 588; 4th = 784 */ * 1.0, from, len);
          const auto inF = goertzelDb (x, fs, 196.0, from, len);
          // 784 Hz is only in the shifted copy (2nd harmonic of 392); the 196 Hz fundamental only in the dry
          check (juce::String ("Harmony 50%: dry fundamental ") + juce::String (dryDb - inF, 1) + " dB, shifted-only partial present ("
                     + juce::String (upDb - inF, 1) + " dB)", std::abs (dryDb - inF + 6.0) < 1.0 && upDb - inF > -20.0); }
        { S s; s.transpose = true; s.shift = 12; s.harmony = false; s.on = true;
          auto p = makePedal (fs, s); const auto y = runPedal (p, x);
          const auto dryDb = goertzelDb (y, fs, 196.0, (int) (fs * 0.3), 16384) - goertzelDb (x, fs, 196.0, (int) (fs * 0.3), 16384);
          check (juce::String ("Harmony off: 100% wet (dry fundamental ") + juce::String (dryDb, 1) + " dB)", dryDb < -40.0); }
    }

    // 6) true bypass: off -> 15 ms fade -> untouched buffer, engines stopped
    if (section (6))
    {
        const double fs = 48000.0;
        S s; s.on = true; s.shift = -5; s.poly = true;
        auto p = makePedal (fs, s);
        auto x = tone (fs, 196.0, 9600);
        runPedal (p, x);
        s.on = false;
        p.setParameters (s);
        runPedal (p, tone (fs, 196.0, (int) (fs * 0.016) + 256));
        auto y = tone (fs, 220.0, 4096);
        const auto ref = y;
        float* ch[1] { y.data() };
        p.process (ch, 1, 4096);
        check ("off after the 15 ms fade: buffer untouched (bit-identical), pedal inactive", y == ref && ! p.isActive());
    }

    // 7) click-free: every parameter move on a running sine
    if (section (7))
    {
        const double fs = 48000.0;
        const int n = (int) (fs * 3.0);
        std::vector<float> x ((size_t) n);
        for (int i = 0; i < n; ++i) x[(size_t) i] = (float) (0.5 * std::sin (kTwoPi * 220.0 * i / fs));
        // a click is a jump far above the steepest legitimate slope (880 Hz at 0.5 = 0.058/sample)
        const float limit = 0.12f;
        struct Move { const char* name; std::function<void (S&)> f; };
        const std::vector<Move> moves {
            { "engage on", [] (S& s) { s.on = true; } },
            { "Shift +12 -> -12", [] (S& s) { s.shift = -12; } },
            { "Shift -12 -> +24", [] (S& s) { s.shift = 24; } },
            { "Shift +24 -> -24", [] (S& s) { s.shift = -24; } },
            { "Detune -50", [] (S& s) { s.detuneCents = -50; } },
            { "Whammy on (Sweep 0)", [] (S& s) { s.whammy = true; s.sweep = 0.0f; } },
            { "Sweep 0 -> 1 (CC jump)", [] (S& s) { s.sweep = 1.0f; } },
            { "Harmony on (Mix 70)", [] (S& s) { s.harmony = true; s.mix = 70.0f; } },
            { "Poly on", [] (S& s) { s.poly = true; } },
            { "Shift -24 -> +7 (poly)", [] (S& s) { s.shift = 7; } },
            { "Poly off", [] (S& s) { s.poly = false; } },
            { "Transpose + Whammy off", [] (S& s) { s.transpose = false; s.whammy = false; } },
            { "Transpose on", [] (S& s) { s.transpose = true; } },
            { "engage off", [] (S& s) { s.on = false; } },
        };
        S s; s.on = false; s.shift = 12;
        auto p = makePedal (fs, s);
        std::vector<float> y (x);
        const int seg = n / (int) (moves.size() + 1);
        float worst = 0.0f; juce::String worstName;
        for (size_t m = 0; m <= moves.size(); ++m)
        {
            if (m > 0) { moves[m - 1].f (s); p.setParameters (s); }
            const int segEnd = m == moves.size() ? n : (int) (m + 1) * seg;
            for (int off = (int) m * seg; off < segEnd; off += 256)
            {
                const int len = std::min (256, segEnd - off);
                float* ch[1] { y.data() + off };
                p.process (ch, 1, len);
            }
            if (m > 0)
            {
                const auto st = maxStep (y, (int) m * seg - 1, (int) m * seg + (int) (fs * 0.06));
                if (st > worst) { worst = st; worstName = moves[m - 1].name; }
            }
        }
        // whammy automation: sweep ramp written every 256-sample block (host / MIDI CC rate)
        {
            S w; w.on = true; w.whammy = true; w.transpose = false; w.shift = 12; w.sweep = 0.0f;
            auto q = makePedal (fs, w);
            std::vector<float> z (x);
            for (int off = 0; off < n; off += 256)
            {
                w.sweep = (float) std::round (127.0 * 0.5 * (1.0 - std::cos (kTwoPi * off / (fs * 1.5)))) / 127.0f; // 7-bit CC steps
                q.setParameters (w);
                float* ch[1] { z.data() + off };
                q.process (ch, 1, std::min (256, n - off));
            }
            const auto st = maxStep (z, 0, n);
            if (st > worst) { worst = st; worstName = "Sweep automation (7-bit CC)"; }
        }
        check (juce::String ("no clicks over 14 moves + 7-bit Sweep automation: largest step ") + juce::String (worst, 4) + " (" + worstName + ") < 0.12", worst < limit && finiteNoDenormal (y));
    }

    // 8) NaN / denormal / sample rates / odd blocks / stereo
    if (section (8))
    {
        bool ok = true;
        juce::String rates;
        for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
            for (int poly = 0; poly < 2; ++poly)
                for (int blk : { 1, 13, 256, 4096 })
                {
                    S s; s.on = true; s.shift = poly ? -24.0f : 24.0f; s.harmony = true; s.mix = 80; s.poly = poly == 1;
                    auto p = makePedal (fs, s, blk, 2);
                    const int n = (int) (fs * 0.3);
                    std::vector<float> l ((size_t) n), r ((size_t) n);
                    juce::Random rng (7);
                    for (int i = 0; i < n; ++i)
                    {
                        const bool loud = i < n / 3;
                        l[(size_t) i] = loud ? rng.nextFloat() * 2.0f - 1.0f : 0.0f;
                        r[(size_t) i] = loud ? (float) std::sin (kTwoPi * 82.4 * i / fs) : 0.0f;
                    }
                    for (int off = 0; off < n; off += blk)
                    {
                        float* ch[2] { l.data() + off, r.data() + off };
                        p.process (ch, 2, std::min (blk, n - off));
                    }
                    ok = ok && finiteNoDenormal (l) && finiteNoDenormal (r);
                    // silence after the burst decays to exact zero (no denormal tails)
                    ok = ok && l.back() == 0.0f && r.back() == 0.0f;
                }
        check ("44.1/48/96/192 kHz x mono/poly x blocks 1/13/256/4096, stereo, noise -> silence: finite, no denormals, exact-zero tail", ok);
    }

    // 9) latency (energy-centroid delay of a tone burst)
    if (section (9))
    {
        const double fs = 48000.0;
        auto lat = [&] (float st, bool poly)
        {
            S s; s.on = true; s.shift = st; s.poly = poly;
            auto p = makePedal (fs, s);
            return burstDelay (p, fs, 440.0) / fs * 1000.0;
        };
        const auto m12 = lat (-12, false), p12 = lat (12, false), mUp24 = lat (24, false), m2 = lat (-2, false);
        const auto pm12 = lat (-12, true), pp12 = lat (12, true);
        S s; s.on = true; s.poly = true;
        auto pp = makePedal (fs, s);
        std::printf ("pitch: latency @48k: mono -2 st %.1f ms, -12 st %.1f ms, +12 st %.1f ms, +24 st %.1f ms; poly -12 %.1f ms, +12 %.1f ms (N = %d samples = %.1f ms)\n",
                     m2, m12, p12, mUp24, pm12, pp12, pp.getPolyLatencySamples(), pp.getPolyLatencySamples() / fs * 1000.0);
        const auto polyMs = pp.getPolyLatencySamples() / fs * 1000.0;
        check ("mono shifter latency <= 15 ms for every interval; poly = N samples (42.7 ms)",
               std::max ({ m2, m12, p12, mUp24 }) < 15.0 && std::abs (pm12 - polyMs) < 1.5 && std::abs (pp12 - polyMs) < 1.5);
    }

    // 10) in the processor: per-amp independence, only the audible amp runs
    if (section (10))
    {
        auto render = [] (std::function<void (JuicyboiAudioProcessor&)> f)
        {
            return jbtest::bypassRender (0, f);
        };
        const auto base = render ([] (JuicyboiAudioProcessor&) {});
        const auto with5150On = render ([] (JuicyboiAudioProcessor& p) { setP (p, "m5150_pitch_on", 1.0f); setP (p, "m5150_pitch_shift", -12.0f); });
        const auto withRectOn = render ([] (JuicyboiAudioProcessor& p) { setP (p, "pitch_on", 1.0f); setP (p, "pitch_shift", -12.0f); });
        check ("5150's PITCH has no effect on the Rectifier (bit-identical)", with5150On == base);
        check ("Rectifier PITCH on changes the Rectifier", withRectOn != base);
        JuicyboiAudioProcessor p; p.stopTunerThread();
        p.setPlayConfigDetails (1, 2, 48000.0, 256);
        setP (p, "pitch_on", 1.0f); setP (p, "m5150_pitch_on", 1.0f);
        p.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> b (2, 256); juce::MidiBuffer midi;
        for (int k = 0; k < 20; ++k) { b.clear(); b.setSample (0, 3, 0.5f); p.processBlock (b, midi); }
        check ("only the selected amp's PITCH engine runs (Rectifier yes, 5150 idle)", p.getPitchPedal (0).isMonoRunning() && ! p.getPitchPedal (1).isMonoRunning());
    }

    // 11) CPU (one core, 48 kHz, 256-sample blocks)
    if (section (11))
    {
        const double fs = 48000.0;
        const int n = (int) (fs * 4.0);
        auto cpu = [&] (S s, int chans)
        {
            auto p = makePedal (fs, s, 256, chans);
            auto x = tone (fs, 110.0, n);
            std::vector<float> y2 (x);
            const auto t0 = std::chrono::steady_clock::now();
            for (int off = 0; off < n; off += 256)
            {
                float* ch[2] { x.data() + off, y2.data() + off };
                p.process (ch, chans, 256);
            }
            const auto sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            return 100.0 * sec / (n / fs);
        };
        S m; m.on = true; m.shift = -12;
        S pl = m; pl.poly = true;
        S off; off.on = false;
        const auto cm = cpu (m, 1), cp = cpu (pl, 1), co = cpu (off, 1), cp2 = cpu (pl, 2);
        std::printf ("pitch: CPU (one core) mono %.2f%%, poly %.2f%% (stereo-in poly %.2f%%), off %.4f%%\n", cm, cp, cp2, co);
        check ("CPU: mono < 3%, poly < 6% of a core, off ~0", cm < 3.0 && cp < 6.0 && co < 0.05);
    }

    std::printf ("pitch: %s\n", failures == 0 ? "all ok" : "FAILURES");
    return failures;
}
