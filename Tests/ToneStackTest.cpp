// Passive TMB tone stack (Source/dsp/ToneStack.h) validation:
//  1) closed-form analog H(s) vs an independent nodal (MNA) solve of the network
//  2) digital response vs the analog H evaluated directly at s = j*w, at 44.1/48/96/192 kHz,
//     both from the coefficients and from sines run through process()
//  3) knob interaction (Mid moves the bass response, all-max is not flat, real insertion loss)
//  4) stability / NaN / denormals at knob extremes and every sample rate, rapid automation
//  5) smooth (click-free) knob moves and Red/Orange component switches
//  6) default-knob voicing vs the Rev1 biquad stand-in, and whole-plugin level at defaults
//     vs the pre-tone-stack renders (legacy-348fc68.f32 / lead-62a1239.f32)
#include "PluginProcessor.h"
#include "LeadFixture.h"
#include <complex>
#include <cstdio>
#include <vector>

namespace
{
using Complex = std::complex<double>;
using namespace jb::dsp;
int failures = 0;
void check (const char* what, bool ok)
{
    std::printf ("[%s] %s\n", ok ? "ok" : "FAIL", what);
    if (! ok) ++failures;
}

// Independent nodal solve. Nodes: A (Ct / treble-pot top), B (treble-pot bottom),
// X (slope-resistor end), M (mid-pot top), W (mid wiper), O (output). Source = 1 V.
Complex nodal (const TmbComponents& c, double t, double m, double l, double hz)
{
    const Complex s (0.0, 2.0 * kPi * hz);
    auto g = [] (double r) { return Complex (1.0 / std::max (r, 1.0e-4), 0.0); };
    enum { A, B, X, M, W, O, N };
    struct E { int a, b; Complex y; };          // -1 = source, -2 = ground
    const std::vector<E> el {
        { -1, A, s * c.ct }, { A, O, g ((1.0 - t) * c.rt) }, { O, B, g (t * c.rt) },
        { -1, X, g (c.rs) }, { X, B, s * c.cb }, { X, W, s * c.cm },
        { B, M, g (l * c.rb) }, { M, W, g ((1.0 - m) * c.rm) }, { W, -2, g (m * c.rm) } };
    Complex Y[N][N] {}, I[N] {};
    for (auto& e : el)
    {
        if (e.a >= 0) Y[e.a][e.a] += e.y;
        if (e.b >= 0) Y[e.b][e.b] += e.y;
        if (e.a >= 0 && e.b >= 0) { Y[e.a][e.b] -= e.y; Y[e.b][e.a] -= e.y; }
        if (e.a == -1 && e.b >= 0) I[e.b] += e.y; // source at 1 V
    }
    for (int k = 0; k < N; ++k)                    // Gaussian elimination, partial pivot
    {
        int p = k;
        for (int r = k + 1; r < N; ++r) if (std::abs (Y[r][k]) > std::abs (Y[p][k])) p = r;
        for (int j = 0; j < N; ++j) std::swap (Y[k][j], Y[p][j]);
        std::swap (I[k], I[p]);
        for (int r = k + 1; r < N; ++r)
        {
            const auto f = Y[r][k] / Y[k][k];
            for (int j = k; j < N; ++j) Y[r][j] -= f * Y[k][j];
            I[r] -= f * I[k];
        }
    }
    Complex v[N];
    for (int k = N - 1; k >= 0; --k)
    {
        Complex acc = I[k];
        for (int j = k + 1; j < N; ++j) acc -= Y[k][j] * v[j];
        v[k] = acc / Y[k][k];
    }
    return v[O];
}

double db (Complex h) { return 20.0 * std::log10 (std::abs (h)); }

double analogDb (const TmbComponents& c, float bass, float mid, float treble, double hz)
{
    return db (tmb::response (tmb::analog (c, tmb::treblePosition (treble), tmb::midPosition (mid),
                                           tmb::bassPosition (bass)), hz))
           + c.makeupDb;
}

PassiveToneStack makeStack (const TmbComponents& c, double fs, float b, float m, float t)
{
    PassiveToneStack st; st.setComponents (c); st.setKnobs (b, m, t); st.prepare (fs);
    return st;
}

/** Steady-state sine gain through process() (dB). */
double sineDb (const TmbComponents& c, double fs, float b, float m, float t, double hz)
{
    auto st = makeStack (c, fs, b, m, t);
    const int settle = (int) (fs * 0.5), meas = (int) (fs * 0.25);
    double in = 0.0, out = 0.0;
    for (int i = 0; i < settle + meas; ++i)
    {
        const auto x = std::sin (2.0 * kPi * hz * i / fs);
        st.beginFrame();
        const auto y = (double) st.process ((float) x, 0);
        if (i >= settle) { in += x * x; out += y * y; }
    }
    return 10.0 * std::log10 (out / in);
}

void setP (JuicyboiAudioProcessor& p, const char* id, float v)
{
    auto* a = p.apvts.getParameter (id); a->setValueNotifyingHost (a->convertTo0to1 (v));
}

double rmsDb (const float* d, size_t n)
{
    double s = 0.0; for (size_t i = 0; i < n; ++i) s += (double) d[i] * d[i];
    return 10.0 * std::log10 (s / (double) n + 1e-30);
}

const double rates[] { 44100.0, 48000.0, 96000.0, 192000.0 };
const float knobsGrid[] { 0.0f, 2.5f, 5.0f, 7.5f, 10.0f };
} // namespace

int toneStackValidation()
{
    failures = 0;
    const TmbComponents sets[] { voicings::redTmb(), voicings::orangeTmb(), leadTmb() };
    const char* names[] { "Red", "Orange", "Lead" };

    // 1) Closed form == independent nodal solve (pot ends included)
    {
        double worst = 0.0;
        for (auto& c : sets)
            for (double t : { 0.0, 0.18, 0.5, 1.0 })
                for (double m : { 0.0, 0.4, 0.73, 1.0 })
                    for (double l : { 0.0, 0.15, 0.6, 1.0 })
                        for (double hz = 10.0; hz < 30000.0; hz *= 1.37)
                        {
                            const auto h = tmb::response (tmb::analog (c, t, m, l), hz);
                            const auto r = nodal (c, t, m, l, hz);
                            if (std::abs (r) < 1e-7) continue; // deep nulls (all-zero pots)
                            worst = std::max (worst, std::abs (h - r) / std::abs (r));
                        }
        std::printf ("TMB closed form vs nodal solve: worst relative error %.3g\n", worst);
        check ("closed-form 3rd-order TMB matches an independent nodal solve (rel < 1e-6)", worst < 1e-6);
    }

    // 2) Digital vs analog s = j*w. (a) Coefficients equal analog at the bilinear-warped
    //    frequency (exact discretisation check). (b) vs analog at the same frequency:
    //    warping bound per rate. (c) sines through process() agree with the coefficients.
    {
        bool exact = true, close = true, sines = true;
        double worstExact = 0.0;
        for (int si = 0; si < 3; ++si)
            for (auto fs : rates)
            {
                double worstWarp = 0.0;
                const auto fw = std::min (tmb::prewarpHz, 0.25 * fs);
                const double limit = fs <= 48000.0 ? 1.0 : (fs <= 96000.0 ? 0.25 : 0.06);
                for (float b : knobsGrid) for (float m : knobsGrid) for (float t : knobsGrid)
                {
                    auto st = makeStack (sets[si], fs, b, m, t);
                    const auto h = tmb::analog (sets[si], tmb::treblePosition (t), tmb::midPosition (m), tmb::bassPosition (b));
                    for (double hz = 20.0; hz < std::min (20000.0, 0.45 * fs); hz *= 1.25)
                    {
                        const auto dig = db (tmb::response (st.transferFunction(), hz, fs)) + 20.0 * std::log10 (st.makeupGain());
                        const auto warped = fw * std::tan (kPi * hz / fs) / std::tan (kPi * fw / fs);
                        const auto e = std::abs (dig - (db (tmb::response (h, warped)) + sets[si].makeupDb));
                        worstExact = std::max (worstExact, e);
                        if (hz <= 8000.0)
                            worstWarp = std::max (worstWarp, std::abs (dig - (db (tmb::response (h, hz)) + sets[si].makeupDb)));
                    }
                }
                exact = exact && worstExact < 1e-6;
                close = close && worstWarp < limit;
                std::printf ("%s @ %.1f kHz: digital vs analog (s=jw) worst %.3f dB up to 8 kHz (limit %.2f)\n",
                             names[si], fs / 1000.0, worstWarp, limit);
                // (c) real sines through the processing path
                for (auto [b, m, t] : { std::array<float, 3> { 5.0f, 4.0f, 5.5f }, { 10.0f, 0.0f, 10.0f },
                                        { 0.0f, 10.0f, 0.0f }, { 10.0f, 10.0f, 10.0f } })
                    for (double hz : { 60.0, 150.0, 500.0, 1500.0, 4000.0 })
                    {
                        auto st = makeStack (sets[si], fs, b, m, t);
                        const auto expect = db (tmb::response (st.transferFunction(), hz, fs)) + 20.0 * std::log10 (st.makeupGain());
                        const auto got = sineDb (sets[si], fs, b, m, t, hz);
                        const auto an = analogDb (sets[si], b, m, t, hz);
                        if (std::abs (got - expect) > 0.02 || std::abs (got - an) > limit) sines = false;
                    }
            }
        std::printf ("digital coefficients vs analog at warped frequency: worst %.3g dB\n", worstExact);
        check ("bilinear (prewarped 3 kHz) == analog H at the warped frequency (< 1e-6 dB)", exact);
        check ("digital vs analog H(jw) to 8 kHz: < 1 dB @44.1/48k, < 0.25 dB @96k, < 0.06 dB @192k", close);
        check ("sines through process() match the coefficients (0.02 dB) and analog H(jw)", sines);
    }

    // 3) Interaction and passive behaviour (analog, Red set)
    {
        const auto& c = sets[0];
        auto range80 = [&] (float mid) { return analogDb (c, 10.0f, mid, 5.5f, 80.0) - analogDb (c, 0.0f, mid, 5.5f, 80.0); };
        const auto bassRangeMid0 = range80 (0.0f), bassRangeMid10 = range80 (10.0f);
        const auto bassAt1k = analogDb (c, 10.0f, 4.0f, 5.5f, 1000.0) - analogDb (c, 0.0f, 4.0f, 5.5f, 1000.0);
        const auto trebleAt500 = analogDb (c, 5.0f, 4.0f, 10.0f, 500.0) - analogDb (c, 5.0f, 4.0f, 0.0f, 500.0);
        double lo = 1e9, hi = -1e9;
        for (double hz = 40.0; hz < 10000.0; hz *= 1.1)
        {
            const auto v = analogDb (c, 10.0f, 10.0f, 10.0f, hz) - c.makeupDb;
            lo = std::min (lo, v); hi = std::max (hi, v);
        }
        std::printf ("Interaction (Red): Bass range @80 Hz %.2f dB at Mid 0 vs %.2f dB at Mid 10; Bass 0->10 moves 1 kHz %+.2f dB; "
                     "Treble 0->10 moves 500 Hz %+.2f dB; all-max spread %.2f dB, peak %+.2f dB\n",
                     bassRangeMid0, bassRangeMid10, bassAt1k, trebleAt500, hi - lo, hi);
        check ("Mid knob changes what the Bass knob does (80 Hz Bass range differs > 2 dB, Mid 0 vs 10)", std::abs (bassRangeMid0 - bassRangeMid10) > 2.0);
        check ("Bass moves the mids (> 0.75 dB at 1 kHz) and Treble moves the low-mids (> 0.75 dB at 500 Hz)",
               std::abs (bassAt1k) > 0.75 && std::abs (trebleAt500) > 0.75);
        check ("everything maxed is not flat (> 3 dB spread) and passive (never above 0 dB)", hi - lo > 3.0 && hi <= 0.0);

        bool ranges = true;
        for (int si = 0; si < 3; ++si)
        {
            const auto& cs = sets[si];
            const auto rb = analogDb (cs, 10, 4, 5.5f, 80) - analogDb (cs, 0, 4, 5.5f, 80);
            const auto rm = analogDb (cs, 5, 10, 5.5f, 500) - analogDb (cs, 5, 0, 5.5f, 500);
            const auto rt = analogDb (cs, 5, 4, 10, 4000) - analogDb (cs, 5, 4, 0, 4000);
            std::printf ("%s knob ranges: Bass @80 Hz %.1f dB, Mid @500 Hz %.1f dB, Treble @4 kHz %.1f dB\n", names[si], rb, rm, rt);
            ranges = ranges && rb > 5.0 && rm > 5.0 && rt > 8.0;
        }
        check ("each knob has a clear range on the sourced parts (Bass/Mid > 5 dB, Treble > 8 dB)", ranges);
    }

    // 4) Stability, NaN, denormals: extremes x rates, poles, impulse tails, automation
    {
        bool stable = true, finite = true, noDenormal = true;
        for (auto& c : sets)
            for (auto fs : rates)
                for (float b : { 0.0f, 10.0f }) for (float m : { 0.0f, 10.0f }) for (float t : { 0.0f, 10.0f })
                {
                    auto st = makeStack (c, fs, b, m, t);
                    const auto& d = st.transferFunction();
                    // Jury test for a monic cubic: a0=1 + a1 z^-1 + a2 z^-2 + a3 z^-3
                    const auto p1 = 1.0 + d.a[1] + d.a[2] + d.a[3], p_1 = 1.0 - d.a[1] + d.a[2] - d.a[3];
                    const bool jury = p1 > 0.0 && p_1 > 0.0 && std::abs (d.a[3]) < 1.0
                                      && std::abs (1.0 - d.a[3] * d.a[3]) > std::abs (d.a[2] - d.a[3] * d.a[1]);
                    stable = stable && jury;
                    // impulse, then 2 s of silence: tail must decay to exact zero (no subnormals)
                    float y = st.process (1.0f, 0);
                    for (int i = 0; i < (int) (2.0 * fs); ++i)
                    {
                        st.beginFrame();
                        y = st.process (0.0f, 0);
                        finite = finite && std::isfinite (y);
                        noDenormal = noDenormal && std::fpclassify (y) != FP_SUBNORMAL;
                    }
                    stable = stable && std::abs (y) < 1e-12f;
                }
        // Rapid automation: every block jump all knobs between random extremes + Red<->Orange
        juce::Random rng (11);
        for (auto fs : rates)
        {
            PassiveToneStack st; st.setComponents (sets[0]); st.setKnobs (5, 4, 5.5f); st.prepare (fs);
            float peak = 0.0f;
            for (int blk = 0; blk < 400; ++blk)
            {
                st.setKnobs (rng.nextBool() ? 10.0f : 0.0f, rng.nextBool() ? 10.0f : 0.0f, rng.nextBool() ? 10.0f : 0.0f);
                st.setComponents (sets[rng.nextInt (3)]);
                for (int i = 0; i < 64; ++i)
                {
                    st.beginFrame();
                    const auto y = st.process (rng.nextFloat() * 2.0f - 1.0f, 0);
                    finite = finite && std::isfinite (y);
                    noDenormal = noDenormal && std::fpclassify (y) != FP_SUBNORMAL;
                    peak = std::max (peak, std::abs (y));
                }
            }
            stable = stable && peak < 20.0f; // white noise in, passive stack * <= 4.2x makeup
        }
        check ("all knob extremes stable (Jury) at 44.1/48/96/192 kHz, impulse tails decay", stable);
        check ("no NaN/Inf under extremes and rapid automation", finite);
        check ("no denormal outputs (tails flushed to exact 0)", noDenormal);
    }

    // 5) Click-free knob moves and component (channel) switches at 48 kHz. A click is a
    //    broadband burst, so the detector is the second difference y[n]-2y[n-1]+y[n-2]
    //    (+12 dB/oct: it ignores slow level/low-frequency settling but magnifies any
    //    discontinuity). Smoothing moves all positions/parts along one straight path, so
    //    the click-free bound is the largest steady-state second difference of the 0.5
    //    amplitude 220 Hz sine anywhere on that path (+10 %). Unsmoothed jumps are reported.
    {
        const double fs = 48000.0, hz = 220.0;
        auto run = [&] (const TmbComponents& c0, std::array<float, 3> k0, const TmbComponents& c1, std::array<float, 3> k1, bool smooth)
        {
            PassiveToneStack st; st.setComponents (c0); st.setKnobs (k0[0], k0[1], k0[2]); st.prepare (fs);
            float prev = 0.0f, prev2 = 0.0f, worst = 0.0f;
            const int len = 48000;
            for (int i = 0; i < len; ++i)
            {
                if (i == len / 2) { st.setComponents (c1); st.setKnobs (k1[0], k1[1], k1[2]); if (! smooth) st.snap(); }
                st.beginFrame();
                const auto y = st.process ((float) (0.5 * std::sin (2.0 * kPi * hz * i / fs)), 0);
                if (i > len / 4) worst = std::max (worst, std::abs (y - 2.0f * prev + prev2));
                prev2 = prev; prev = y;
            }
            return worst;
        };
        auto pathBound = [&] (const TmbComponents& c0, std::array<float, 3> k0, const TmbComponents& c1, std::array<float, 3> k1)
        {
            const double p0[] { tmb::treblePosition (k0[2]), tmb::midPosition (k0[1]), tmb::bassPosition (k0[0]) };
            const double p1[] { tmb::treblePosition (k1[2]), tmb::midPosition (k1[1]), tmb::bassPosition (k1[0]) };
            double g = 0.0;
            for (double a = 0.0; a <= 1.0001; a += 0.01)
            {
                auto mixv = [a] (double x, double y) { return x + a * (y - x); };
                TmbComponents c { mixv (c0.rt, c1.rt), mixv (c0.rb, c1.rb), mixv (c0.rm, c1.rm), mixv (c0.rs, c1.rs),
                                  mixv (c0.ct, c1.ct), mixv (c0.cb, c1.cb), mixv (c0.cm, c1.cm), 0.0 };
                const auto mk = mixv (std::pow (10.0, c0.makeupDb / 20.0), std::pow (10.0, c1.makeupDb / 20.0));
                const auto d = tmb::bilinear (tmb::analog (c, mixv (p0[0], p1[0]), mixv (p0[1], p1[1]), mixv (p0[2], p1[2])), fs);
                g = std::max (g, std::abs (tmb::response (d, hz, fs)) * mk);
            }
            return (float) (g * 0.5 * std::pow (2.0 * std::sin (kPi * hz / fs), 2.0));
        };
        const std::array<float, 3> lo { 0.0f, 0.0f, 0.0f }, hi { 10.0f, 10.0f, 10.0f }, def { 5.0f, 4.0f, 5.5f };
        const auto knobSmooth = run (sets[0], lo, sets[0], hi, true), knobJump = run (sets[0], lo, sets[0], hi, false);
        const auto knobBound = pathBound (sets[0], lo, sets[0], hi);
        const auto chSmooth = run (sets[0], def, sets[1], def, true), chJump = run (sets[0], def, sets[1], def, false);
        const auto chBound = pathBound (sets[0], def, sets[1], def);
        std::printf ("Smoothing (2nd difference): knobs 0 -> 10 max %.5f (path bound %.5f, unsmoothed %.5f); Red -> Orange %.5f (bound %.5f, unsmoothed %.5f)\n",
                     knobSmooth, knobBound, knobJump, chSmooth, chBound, chJump);
        check ("knob jumps 0 -> 10 are smoothed (no click)", knobSmooth <= 1.1f * knobBound);
        check ("Red/Orange component switch is smoothed (no click)", chSmooth <= 1.1f * chBound);
    }

    // 6a) Default-knob voicing vs the Rev1 biquad stand-in (exact Rev1 formulas, 48 kHz)
    {
        const double fs = 48000.0;
        auto rev1Rect = [&] (double hz)
        {
            Complex h (1.0, 0.0);
            for (auto c : { BiquadCoeffs::lowShelf (fs, 100.0, 0.707, 0.0), BiquadCoeffs::peak (fs, 400.0, 0.75, -2.4),
                            BiquadCoeffs::highShelf (fs, 4500.0, 0.707, 1.2) })
            {
                const auto z1 = std::polar (1.0, -2.0 * kPi * hz / fs);
                h *= ((double) c.b0 + z1 * ((double) c.b1 + z1 * (double) c.b2)) / (1.0 + z1 * ((double) c.a1 + z1 * (double) c.a2));
            }
            return db (h);
        };
        auto rev1Lead = [&] (double hz)
        {
            Complex h (1.0, 0.0);
            for (auto c : { BiquadCoeffs::peak (fs, 620.0, 0.65, -3.0), BiquadCoeffs::lowShelf (fs, 130.0, 0.707, 0.0),
                            BiquadCoeffs::peak (fs, 760.0, 0.72, -2.0), BiquadCoeffs::highShelf (fs, 2300.0, 0.707, 0.9) })
            {
                const auto z1 = std::polar (1.0, -2.0 * kPi * hz / fs);
                h *= ((double) c.b0 + z1 * ((double) c.b1 + z1 * (double) c.b2)) / (1.0 + z1 * ((double) c.a1 + z1 * (double) c.a2));
            }
            return db (h);
        };
        bool ok = true;
        for (int si = 0; si < 3; ++si)
        {
            auto st = makeStack (sets[si], fs, 5.0f, 4.0f, 5.5f);
            double sum = 0.0, sumSq = 0.0, worst = 0.0; int n = 0;
            std::printf ("%s defaults vs Rev1 (dB, new - old):", names[si]);
            for (double hz = 70.0; hz <= 6000.0; hz *= 1.05)
            {
                const auto e = db (tmb::response (st.transferFunction(), hz, fs)) + 20.0 * std::log10 (st.makeupGain())
                               - (si == 2 ? rev1Lead (hz) : rev1Rect (hz));
                sum += e; sumSq += e * e; worst = std::max (worst, std::abs (e)); ++n;
            }
            for (double hz : { 80.0, 160.0, 315.0, 630.0, 1250.0, 2500.0, 5000.0 })
                std::printf (" %g:%+.2f", hz, db (tmb::response (st.transferFunction(), hz, fs)) + 20.0 * std::log10 (st.makeupGain())
                                                  - (si == 2 ? rev1Lead (hz) : rev1Rect (hz)));
            std::printf ("\n   mean %+.2f dB, RMS %.2f dB, worst %.2f dB (70 Hz-6 kHz)\n", sum / n, std::sqrt (sumSq / n), worst);
            ok = ok && std::abs (sum / n) < 1.0 && worst < 3.5;
        }
        check ("default-knob TMB (sourced parts + makeup) tracks the Rev1 voicing: mean < 1 dB, worst < 3.5 dB, 70 Hz-6 kHz", ok);
    }

    // 6b) Whole-plugin level at default knobs vs the pre-tone-stack renders
    {
        bool ok = true;
        juce::File legacy (juce::String (JUICYBOI_TEST_DIR) + "/legacy-348fc68.f32");
        auto in = legacy.createInputStream();
        const size_t per = 32 * 256, settle = 8 * 256;
        for (int voice = 0; voice < 3 && in != nullptr; ++voice)
            for (int boost = 0; boost < 3; ++boost)
            {
                JuicyboiAudioProcessor p;
                setP (p, "channel", voice == 2 ? 1.0f : 0.0f); setP (p, "red_voice", voice == 1 ? 1.0f : 0.0f);
                setP (p, "od_enable", boost == 0 ? 0.0f : 1.0f); setP (p, "od_type", boost == 2 ? 1.0f : 0.0f);
                p.setPlayConfigDetails (1, 2, 48000.0, 256); p.prepareToPlay (48000.0, 256);
                juce::MidiBuffer midi; std::vector<float> out, ref;
                for (int b = 0; b < 32; ++b)
                {
                    juce::AudioBuffer<float> buffer (2, 256); buffer.clear();
                    for (int i = 0; i < 256; ++i)
                    {
                        const auto t = (double) (b * 256 + i) / 48000.0;
                        buffer.setSample (0, i, (float) (0.18 * std::sin (2.0 * kPi * 82.4 * t) + 0.04 * std::sin (2.0 * kPi * 733.0 * t)));
                    }
                    p.processBlock (buffer, midi);
                    out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 256);
                }
                for (size_t i = 0; i < per; ++i) ref.push_back (in->readFloat());
                const auto d = rmsDb (out.data() + settle, per - settle) - rmsDb (ref.data() + settle, per - settle);
                static const char* vn[] { "Red v1", "Red v2", "Orange" };
                static const char* bn[] { "Boost off", "OD-808", "SD-1" };
                std::printf ("Level at defaults vs 348fc68: %-6s %-9s %+.2f dB\n", vn[voice], bn[boost], d);
                ok = ok && std::abs (d) < 2.0;
            }
        // 5150 defaults (config 0 of lead-62a1239.f32)
        juce::File lead (juce::String (JUICYBOI_TEST_DIR) + "/lead-62a1239.f32");
        auto li = lead.createInputStream();
        const auto out = leadfixture::render (0);
        std::vector<float> ref;
        for (size_t i = 0; i < out.size() && li != nullptr; ++i) ref.push_back (li->readFloat());
        const auto d = ref.size() == out.size() ? rmsDb (out.data() + settle, out.size() - settle) - rmsDb (ref.data() + settle, ref.size() - settle) : 99.0;
        std::printf ("Level at defaults vs 62a1239: 5150 Lead %+.2f dB\n", d);
        ok = ok && in != nullptr && std::abs (d) < 2.0;
        check ("whole-plugin level at default knobs within 2 dB of the pre-tone-stack build (all voices/boosts + 5150)", ok);
    }

    // 7) Whole processor stays finite with tone knobs at extremes at every rate
    {
        bool ok = true;
        for (auto fs : rates)
            for (float model : { 0.0f, 1.0f })
                for (float k : { 0.0f, 10.0f })
                {
                    JuicyboiAudioProcessor p;
                    setP (p, "preamp_model", model); setP (p, "pre_bass", k); setP (p, "pre_mid", 10.0f - k);
                    setP (p, "pre_treble", k); setP (p, "pre_gain", 10.0f);
                    p.setPlayConfigDetails (1, 2, fs, 512); p.prepareToPlay (fs, 512);
                    juce::MidiBuffer midi; juce::Random rng (3);
                    for (int b = 0; b < 40; ++b)
                    {
                        juce::AudioBuffer<float> buf (2, 512);
                        for (int i = 0; i < 512; ++i) buf.setSample (0, i, b < 30 ? rng.nextFloat() * 0.6f - 0.3f : 0.0f);
                        if (b == 15) { setP (p, "pre_bass", 10.0f - k); setP (p, "pre_treble", 10.0f - k); setP (p, "channel", 1.0f); }
                        p.processBlock (buf, midi);
                        for (int i = 0; i < 512; ++i) ok = ok && std::isfinite (buf.getSample (0, i)) && std::abs (buf.getSample (0, i)) < 8.0f;
                    }
                }
        check ("whole plugin finite/bounded with tone knobs at extremes at 44.1/48/96/192 kHz (both models)", ok);
    }
    return failures;
}
