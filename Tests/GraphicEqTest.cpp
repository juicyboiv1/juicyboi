// Rev3.2 GEQ-7 (component-level GE-7-style graphic EQ): analytic model vs ngspice,
// digital (4x, trapezoidal companion models) vs ngspice, slider law, true bypass,
// click-free switching / slider moves, CPU.
#include "dsp/GraphicEq.h"

#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
int failures = 0;
void check (const char* what, bool ok)
{
    std::printf ("[%s] geq7: %s\n", ok ? "ok" : "FAIL", what);
    if (! ok) ++failures;
}
using jb::dsp::GraphicEq;
using W = std::array<double, GraphicEq::numBands>;

struct RefSet { W w {}; double wl = 0.5; std::vector<double> f, db; };

std::vector<RefSet> loadRef()
{
    std::vector<RefSet> sets;
    std::ifstream in (std::string (JUICYBOI_TEST_DIR) + "/geq7-ngspice-ref.txt");
    std::string line;
    while (std::getline (in, line))
    {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss (line);
        W w {}; double wl, f, db;
        for (auto& x : w) ss >> x;
        ss >> wl >> f >> db;
        if (sets.empty() || sets.back().w != w || sets.back().wl != wl)
            sets.push_back ({ w, wl, {}, {} });
        sets.back().f.push_back (f);
        sets.back().db.push_back (db);
    }
    return sets;
}

/** Digital response from a long impulse response (DFT at the given frequencies). */
std::vector<double> digitalDb (const W& w, double wl, double fs, const std::vector<double>& freqs)
{
    GraphicEq eq;
    eq.prepare (fs, 512);
    eq.setWipers (true, w, wl);
    const int n = 1 << 17;
    std::vector<float> x ((size_t) n, 0.0f);
    x[0] = 1.0f;
    for (int i = 0; i < n; i += 512)
    {
        float* p[1] { x.data() + i };
        eq.process (p, 1, std::min (512, n - i));
    }
    std::vector<double> out;
    for (auto f : freqs)
    {
        std::complex<double> acc (0.0);
        const auto rot = std::polar (1.0, -2.0 * jb::dsp::kPi * f / fs);
        std::complex<double> z (1.0);
        for (int i = 0; i < n; ++i) { acc += (double) x[(size_t) i] * z; z *= rot; }
        out.push_back (20.0 * std::log10 (std::abs (acc)));
    }
    return out;
}

double rmsDb (const std::vector<float>& v, size_t from)
{
    double a = 0.0;
    for (size_t i = from; i < v.size(); ++i) a += (double) v[i] * v[i];
    return 10.0 * std::log10 (a / (double) (v.size() - from) + 1e-30);
}
} // namespace

int graphicEqValidation()
{
    failures = 0;

    // 1) analytic model and the digital circuit vs ngspice
    const auto sets = loadRef();
    check ("ngspice reference loaded (16 settings)", sets.size() == 16);
    double anaErr = 0.0;
    std::map<double, std::pair<double, double>> digErr; // fs -> (to 10 kHz, to 20 kHz)
    for (auto& s : sets)
    {
        for (size_t i = 0; i < s.f.size(); ++i)
        {
            const auto a = 20.0 * std::log10 (std::abs (GraphicEq::circuitResponse (s.f[i], s.w, s.wl)));
            anaErr = std::max (anaErr, std::abs (a - s.db[i]));
        }
        for (double fs : { 4 * 44100.0, 4 * 48000.0, 4 * 96000.0 })
        {
            const auto d = digitalDb (s.w, s.wl, fs, s.f);
            auto& e = digErr[fs];
            for (size_t i = 0; i < s.f.size(); ++i)
            {
                const auto err = std::abs (d[i] - s.db[i]);
                if (s.f[i] <= 10000.0) e.first = std::max (e.first, err);
                e.second = std::max (e.second, err);
            }
        }
    }
    std::printf ("geq7: analytic model vs ngspice (16 settings x 121 points, 4558/TL022 op-amps, BJT follower): %.4f dB\n", anaErr);
    check ("analytic circuit model within 0.01 dB of ngspice", anaErr < 0.01);
    for (auto& [fs, e] : digErr)
        std::printf ("geq7: digital at %.1f kHz vs ngspice: %.4f dB to 10 kHz, %.4f dB to 20 kHz\n", fs / 1000.0, e.first, e.second);
    check ("digital (4x of 44.1/48/96 kHz) within 0.1 dB of ngspice to 10 kHz",
           digErr[4 * 44100.0].first < 0.1 && digErr[4 * 48000.0].first < 0.1 && digErr[4 * 96000.0].first < 0.1);
    check ("digital (4x of 44.1/48/96 kHz) within 0.25 dB of ngspice to 20 kHz",
           digErr[4 * 44100.0].second < 0.25 && digErr[4 * 48000.0].second < 0.25 && digErr[4 * 96000.0].second < 0.25);

    // 2) slider law
    {
        bool ends = true, mono = true, law = true;
        double lawErr = 0.0;
        for (int k = 0; k < GraphicEq::numBands; ++k)
        {
            ends = ends && GraphicEq::bandWiper (k, 15.0) == 1.0 && GraphicEq::bandWiper (k, -15.0) == 0.0
                        && std::abs (GraphicEq::bandWiper (k, 0.0) - 0.5) < 1e-12;
            double prev = -1.0;
            for (double v = -15.0; v <= 15.0; v += 0.1)
            {
                const auto w = GraphicEq::bandWiper (k, v);
                mono = mono && w >= prev;
                prev = w;
                W ws; ws.fill (0.5); ws[(size_t) k] = w;
                const auto g = 20.0 * std::log10 (std::abs (GraphicEq::bandStageResponse (GraphicEq::bandRefHz (k), ws)));
                lawErr = std::max (lawErr, std::abs (g - v / 15.0 * GraphicEq::bandMaxDb (k)));
            }
            W ws; ws.fill (0.5); ws[(size_t) k] = GraphicEq::bandWiper (k, 6.0);
            std::printf ("geq7: band %4.0f Hz: ref %7.1f Hz, full boost %+.2f dB, slider +6 -> wiper %.4f -> %+.2f dB\n",
                         GraphicEq::bandHz[(size_t) k], GraphicEq::bandRefHz (k), GraphicEq::bandMaxDb (k), ws[(size_t) k],
                         20.0 * std::log10 (std::abs (GraphicEq::bandStageResponse (GraphicEq::bandRefHz (k), ws))));
        }
        law = lawErr < 0.08; // only the 6.4k cut end is 0.07 dB short (-12.11 vs +12.18 dB)
        std::printf ("geq7: Level full boost %+.2f dB; slider law max error %.4f dB\n", GraphicEq::levelMaxDb(), lawErr);
        check ("slider ends = wiper ends, 0 dB = centre", ends);
        check ("slider law monotonic", mono);
        check ("band peak gain = value/15 x band maximum (within 0.08 dB)", law);
        check ("band maxima 13.5..16 dB at the gyrator bands, Level 14.9 dB",
               GraphicEq::bandMaxDb (0) > 13.5 && GraphicEq::bandMaxDb (5) < 16.0 && std::abs (GraphicEq::levelMaxDb() - 14.88) < 0.05);
    }

    // 3) flat response (all sliders 0 dB): the real pedal's small loss
    {
        W w; w.fill (0.5);
        double lo = 1e9, hi = -1e9;
        for (double f = 40.0; f <= 15000.0; f *= 1.05)
        {
            const auto g = 20.0 * std::log10 (std::abs (GraphicEq::circuitResponse (f, w, 0.5)));
            lo = std::min (lo, g); hi = std::max (hi, g);
        }
        std::printf ("geq7: flat (all 0 dB) 40 Hz..15 kHz: %+.2f .. %+.2f dB\n", lo, hi);
        check ("flat settings within -0.8..0 dB (input divider, de-emphasis, follower, coupling caps)", lo > -0.8 && hi < 0.0);
    }

    // 4) true bypass, click-free enable and slider moves
    {
        const double fs = 192000.0;
        const int n = (int) (0.3 * fs);
        std::vector<float> x ((size_t) n);
        for (int i = 0; i < n; ++i) x[(size_t) i] = 0.3f * (float) std::sin (2.0 * jb::dsp::kPi * 220.0 * i / fs);
        auto maxCurv = [&] (const std::vector<float>& y)
        {
            double m = 0.0;
            for (int i = 512 * 30; i < n; ++i)
                m = std::max (m, (double) std::abs (y[(size_t) i] - 2 * y[(size_t) i - 1] + y[(size_t) i - 2]));
            return m;
        };

        GraphicEq eq;
        std::array<float, 7> g {}; g.fill (0.0f);
        eq.setParameters (false, g, 0.0f);
        eq.prepare (fs, 512);
        auto y = x;
        for (int i = 0; i < n; i += 512) { float* p[1] { y.data() + i }; eq.process (p, 1, std::min (512, n - i)); }
        check ("off = true bypass (bit-identical, isActive false)", y == x && ! eq.isActive());

        // static +12 dB at 200 Hz (reference curvature)
        GraphicEq st; g[1] = 12.0f; st.setParameters (true, g, 0.0f); st.prepare (fs, 512);
        auto ys = x;
        for (int i = 0; i < n; i += 512) { float* p[1] { ys.data() + i }; st.process (p, 1, std::min (512, n - i)); }
        const auto ref = maxCurv (ys);

        // enable mid-stream with +12 dB at 200 Hz, then sweep the 200 Hz slider +12 -> -12 dB
        y = x;
        for (int i = 0; i < n; i += 512)
        {
            if (i == 512 * 40) eq.setParameters (true, g, 0.0f);
            if (i > 512 * 70) { g[1] = (float) (12.0 - 24.0 * ((i - 512 * 70) / (double) (n - 512 * 70))); eq.setParameters (true, g, 0.0f); }
            float* p[1] { y.data() + i };
            eq.process (p, 1, std::min (512, n - i));
        }
        const auto got = maxCurv (y);
        std::printf ("geq7: enable fade + 200 Hz slider sweep: max |2nd diff| %.3e vs steady +12 dB %.3e\n", got, ref);
        check ("enable fade and slider sweep click-free (no curvature beyond the steady +12 dB tone)", got <= 1.05 * ref);

        // the plugin path matches the circuit: 200 Hz slider +15 on a tone at the band peak
        const auto f0 = GraphicEq::bandRefHz (1);
        for (int i = 0; i < n; ++i) x[(size_t) i] = 0.1f * (float) std::sin (2.0 * jb::dsp::kPi * f0 * i / fs);
        GraphicEq e2; std::array<float, 7> g2 {}; g2.fill (0.0f); g2[1] = 15.0f;
        e2.setParameters (true, g2, 0.0f); e2.prepare (fs, 512);
        auto z = x;
        for (int i = 0; i < n; i += 512) { float* p[1] { z.data() + i }; e2.process (p, 1, std::min (512, n - i)); }
        const auto boost = rmsDb (z, (size_t) n / 2) - rmsDb (x, (size_t) n / 2);
        W w2; w2.fill (0.5); w2[1] = 1.0;
        const auto expect = 20.0 * std::log10 (std::abs (GraphicEq::circuitResponse (f0, w2, 0.5)));
        std::printf ("geq7: 200 Hz slider +15 at %.1f Hz: %+.2f dB (circuit %+.2f dB = band %+.2f dB + flat-path loss)\n",
                     f0, boost, expect, GraphicEq::bandMaxDb (1));
        check ("200 Hz slider +15 matches the circuit at the band peak (0.05 dB)", std::abs (boost - expect) < 0.05);
    }

    // 5) CPU (per channel, at 4x of 48 kHz)
    {
        GraphicEq eq; std::array<float, 7> g { 3, -2, 0, 5, -6, 2, 4 };
        eq.setParameters (true, g, 1.0f); eq.prepare (192000.0, 512);
        std::vector<float> x (512);
        for (size_t i = 0; i < x.size(); ++i) x[i] = 0.1f * (float) std::sin (0.01 * (double) i);
        const int blocks = 192000 / 512 * 5;
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b) { float* p[1] { x.data() }; eq.process (p, 1, 512); }
        const auto sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("geq7: CPU %.3f %% of one core per channel (4x of 48 kHz, 5 s of audio in %.3f s)\n", sec / 5.0 * 100.0, sec);
        check ("GEQ-7 CPU below 3 % of a core per channel", sec / 5.0 < 0.03);
    }

    return failures;
}
