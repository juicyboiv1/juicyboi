// Rev3 drive-staging probe: the solved triode cascades vs the Rev2.4 static
// curves (same voicing, `behavioural = true`), cascade output only (before the
// tone stack), at the 4x rate of 48 kHz. Plain C++, no JUCE.
// usage: juicyboi_triode_tune <red2|red1|orange|lead|rhythm> [d0 d1 d2 ... per-stage drive trims]
//        juicyboi_triode_tune <voice> search    -> coordinate search on the trims
// Prints per Gain/input: level, THD, harmonics within -40 dBc, >=2 kHz energy,
// even-odd balance, for both models, and the cost the search minimises.
#include "../Source/dsp/PreampCascade.h"
#include "../Source/dsp/Amp5150.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace jb::dsp;

namespace
{
constexpr double fs = 192000.0, f0 = 110.0;
struct M { double lvl, thd, n, hf, eo; };

M analyse (const std::vector<float>& y)
{
    const size_t N = y.size();
    double h[41] {};
    for (int k = 1; k <= 40; ++k)
    {
        double re = 0, im = 0;
        const auto w = 2.0 * kPi * f0 * k / fs;
        for (size_t i = 0; i < N; ++i) { re += y[i] * std::cos (w * (double) i); im += y[i] * std::sin (w * (double) i); }
        h[k] = std::sqrt (re * re + im * im) * 2.0 / (double) N;
    }
    M m {};
    double s = 0, ev = 0, od = 0, hf = 0;
    int n = 0;
    for (int k = 2; k <= 40; ++k)
    {
        const auto p = h[k] * h[k];
        s += p; (k % 2 ? od : ev) += p;
        if (k * f0 >= 2000.0) hf += p;
        if (h[k] > h[1] * 0.01) ++n;
    }
    const auto p1 = h[1] * h[1] + 1e-30;
    m.lvl = 10 * std::log10 (p1 / 2 + 1e-30);
    m.thd = 10 * std::log10 (s / p1 + 1e-30);
    m.n = n; m.hf = 10 * std::log10 (hf / p1 + 1e-30);
    m.eo = 10 * std::log10 ((ev + 1e-30) / (od + 1e-30));
    return m;
}

struct Voice { std::string name; bool is5150; ChannelVoicing cv; Voicing5150 v5; bool rhythm; };

Voice makeVoice (const std::string& n)
{
    Voice v; v.name = n; v.is5150 = (n == "lead" || n == "rhythm"); v.rhythm = n == "rhythm";
    if (n == "red2") v.cv = voicings::redV2(); else if (n == "red1") v.cv = voicings::redV1(); else if (n == "orange") v.cv = voicings::orange();
    v.v5 = defaultVoicing5150();
    return v;
}

M run (Voice v, bool behavioural, const std::vector<double>& drive, double gain, double amp)
{
    std::vector<float> y;
    const long settle = (long) (0.4 * fs), len = (long) (fs * 50.0 / f0) ; // 50 whole cycles
    if (v.is5150)
    {
        v.v5.behavioural = behavioural;
        for (size_t i = 0; i < drive.size() && i < 6; ++i) v.v5.triodeDrive[i] = (float) drive[i];
        Cascade5150 c; c.configure (v.v5, v.rhythm); c.setPreGain ((float) gain); c.prepare (fs); c.snap();
        for (long i = 0; i < settle + len; ++i)
        {
            c.beginFrame();
            const auto o = c.process ((float) (amp * std::sin (2 * kPi * f0 * (double) i / fs)), 0);
            if (i >= settle) y.push_back (o);
        }
    }
    else
    {
        v.cv.behavioural = behavioural;
        for (size_t i = 0; i < drive.size() && i < 4; ++i) v.cv.triodeDrive[i] = (float) drive[i];
        PreampCascade c; c.prepare (fs, v.cv); c.setGain ((float) (gain / 10.0)); c.prepare (fs, v.cv);
        for (long i = 0; i < settle + len; ++i)
        {
            const auto o = c.process ((float) (amp * std::sin (2 * kPi * f0 * (double) i / fs)), 0);
            if (i >= settle) y.push_back (o);
        }
    }
    return analyse (y);
}

const double gains[] { 3, 5, 7, 10 }, amps[] { 0.05, 0.25 };

double cost (const Voice& v, const std::vector<M>& ref, const std::vector<double>& d, bool print)
{
    double c = 0; size_t j = 0;
    for (auto g : gains) for (auto a : amps)
    {
        const auto m = run (v, false, d, g, a);
        const auto& r = ref[j++];
        const auto e = (m.thd - r.thd) * (m.thd - r.thd) + 0.25 * (m.eo - r.eo) * (m.eo - r.eo)
                     + 0.1 * (std::max (m.hf, -60.0) - std::max (r.hf, -60.0)) * (std::max (m.hf, -60.0) - std::max (r.hf, -60.0))
                     + 0.25 * (m.n - r.n) * (m.n - r.n);
        c += e * (g >= 7 ? 1.0 : g >= 5 ? 0.5 : 0.25); // the hi-gain settings matter most
        if (print)
            std::printf ("%-7s g%-4g a%-5g | rev2.4 lvl %6.1f thd %6.1f n %3.0f hf %6.1f e-o %5.1f | rev3 lvl %6.1f thd %6.1f n %3.0f hf %6.1f e-o %5.1f\n",
                         v.name.c_str(), g, a, r.lvl, r.thd, r.n, r.hf, r.eo, m.lvl, m.thd, m.n, m.hf, m.eo);
    }
    return c;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2) { std::fprintf (stderr, "usage: %s voice [drives...|search]\n", argv[0]); return 2; }
    const auto v = makeVoice (argv[1]);
    const size_t ns = v.is5150 ? 6 : 4;
    std::vector<double> d (ns, 1.0);
    const bool search = argc > 2 && std::strcmp (argv[2], "search") == 0;
    if (! search) for (int i = 2; i < argc && (size_t) (i - 2) < ns; ++i) d[(size_t) (i - 2)] = std::atof (argv[i]);
    std::vector<M> ref;
    for (auto g : gains) for (auto a : amps) ref.push_back (run (v, true, d, g, a));
    if (search)
    {
        auto best = cost (v, ref, d, false);
        for (double step : { 2.0, 1.41, 1.19, 1.09 })
            for (int pass = 0; pass < 2; ++pass)
                for (size_t i = 0; i < ns; ++i)
                    for (double f : { step, 1.0 / step })
                    {
                        auto t = d; t[i] *= f;
                        if (t[i] < 0.4 || t[i] > 4.0) continue;
                        const auto c = cost (v, ref, t, false);
                        if (c < best) { best = c; d = t; std::fprintf (stderr, "cost %.2f :", c); for (auto x : d) std::fprintf (stderr, " %.3f", x); std::fprintf (stderr, "\n"); }
                    }
    }
    const auto c = cost (v, ref, d, true);
    std::printf ("cost %.2f drives:", c);
    for (auto x : d) std::printf (" %.3f", x);
    std::printf ("\n");
    return 0;
}
