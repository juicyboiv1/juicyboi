// Low-end / palm-mute probe for the Rectifier Red v2 bass work (Rev2.3).
// usage: juicyboi_bassprobe [csv-dir|-] [cfg-filter]
// Full processor, Cab/EQ/Gate/Boost off, default knobs except Bass.
// Prints, per config (Red v2 / Red v1 / Orange at Bass 5 and Bass 10):
//   * small-signal response 40..200 Hz (linear region, dB re in) + 1 kHz ref
//   * driven response (0.1 FS sines): fundamental out level vs frequency
//   * low-E sine (82.5 Hz, 0.25 FS): THD and harmonics within -40 dBc
//   * two-tone power chord E2+B2 (82.5 + 123.75 Hz, 0.1 FS each): IMD (all
//     products off the f1/f2 harmonic grids, re f1+f2) and the 41.25 Hz
//     difference tone
//   * SMPTE-style IMD: 82.5 Hz (0.16) + 2 kHz (0.04): sidebands 2k±n*82.5 re 2k
//   * palm-mute chugs (synthetic damped low-E, 8ths at 140 bpm): band
//     energies and "flub" = <=200 Hz energy left in the muted gap re the hit
// Same file builds against Rev2.2 and Rev2.3 (parameter IDs stable).
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Parameters.h"
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <numeric>
#include <vector>

extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
constexpr double sr = 48000.0;
constexpr double twoPi = 6.283185307179586;

void setParam (juce::AudioProcessor& p, const char* id, float plain)
{
    auto& proc = dynamic_cast<JuicyboiAudioProcessor&> (p);
    if (auto* param = proc.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (plain));
}

struct Cfg { const char* name; int channel, redVoice; float bass; };

std::vector<double> render (const Cfg& c, const std::function<double (long)>& in, long n)
{
    std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
    const int bs = 512;
    p->setPlayConfigDetails (1, 2, sr, bs);
    setParam (*p, jb::params::id::preampModel, 0.0f);
    setParam (*p, jb::params::id::channel, (float) c.channel);
    setParam (*p, jb::params::id::redVoice, (float) c.redVoice);
    setParam (*p, jb::params::id::bass, c.bass);
    setParam (*p, jb::params::id::cabEnable, 0.0f);
    setParam (*p, jb::params::id::eqEnable, 0.0f);
    setParam (*p, jb::params::id::gateEnable, 0.0f);
    setParam (*p, jb::params::id::odEnable, 0.0f);
    p->prepareToPlay (sr, bs);
    std::vector<double> out; out.reserve ((size_t) n + bs);
    juce::AudioBuffer<float> buf (2, bs);
    juce::MidiBuffer midi;
    long t = 0;
    while ((long) out.size() < n)
    {
        buf.clear();
        for (int i = 0; i < bs; ++i, ++t) buf.setSample (0, i, (float) in (t));
        p->processBlock (buf, midi);
        for (int i = 0; i < bs; ++i) out.push_back (buf.getSample (0, i));
    }
    out.resize ((size_t) n);
    return out;
}

// amplitude of frequency f over y[a, a+n) (n = whole cycles)
double tone (const std::vector<double>& y, long a, long n, double f)
{
    double re = 0, im = 0; const double w = twoPi * f / sr;
    for (long i = 0; i < n; ++i) { re += y[(size_t) (a + i)] * std::cos (w * i); im -= y[(size_t) (a + i)] * std::sin (w * i); }
    return 2.0 * std::sqrt (re * re + im * im) / (double) n;
}

double db (double x) { return 20.0 * std::log10 (x + 1e-20); }

// radix-2 FFT power spectrum (Hann), returns |X|^2 per bin
std::vector<double> powerSpectrum (const std::vector<double>& y, long a, int order)
{
    const size_t N = (size_t) 1 << order;
    std::vector<std::complex<double>> x (N);
    for (size_t i = 0; i < N; ++i) x[i] = y[(size_t) a + i] * 0.5 * (1.0 - std::cos (twoPi * (double) i / (double) N));
    for (size_t i = 1, j = 0; i < N; ++i) { size_t bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (x[i], x[j]); }
    for (size_t len = 2; len <= N; len <<= 1)
    {
        const auto wl = std::polar (1.0, -twoPi / (double) len);
        for (size_t i = 0; i < N; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t k = 0; k < len / 2; ++k, w *= wl) { auto u = x[i + k], v = x[i + k + len / 2] * w; x[i + k] = u + v; x[i + k + len / 2] = u - v; }
        }
    }
    std::vector<double> P (N / 2);
    for (size_t i = 0; i < N / 2; ++i) P[i] = std::norm (x[i]);
    return P;
}

double bandPow (const std::vector<double>& P, int order, double f0, double f1)
{
    const double df = sr / (double) ((size_t) 1 << order); double s = 0;
    for (size_t i = (size_t) std::ceil (f0 / df); i < P.size() && (double) i * df < f1; ++i) s += P[i];
    return s;
}

double peakPow (const std::vector<double>& P, int order, double f) // +-2 bins (Hann main lobe)
{
    const double df = sr / (double) ((size_t) 1 << order); const long c = std::lround (f / df); double s = 0;
    for (long i = c - 2; i <= c + 2; ++i) if (i > 0 && i < (long) P.size()) s += P[(size_t) i];
    return s;
}

struct Biquad // RBJ low-pass
{
    double b0, b1, b2, a1, a2, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    Biquad (double f, double q) { const double w = twoPi * f / sr, al = std::sin (w) / (2 * q), cw = std::cos (w), a0 = 1 + al;
        b0 = (1 - cw) / 2 / a0; b1 = (1 - cw) / a0; b2 = b0; a1 = -2 * cw / a0; a2 = (1 - al) / a0; }
    double operator() (double x) { const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; }
};
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const Cfg cfgs[] = { { "RedV2-B5", 0, 1, 5.0f }, { "RedV2-B10", 0, 1, 10.0f },
                         { "RedV1-B5", 0, 0, 5.0f }, { "Orange-B5", 1, 0, 5.0f } };
    const char* dir = (argc > 1 && argv[1][0] != '-') ? argv[1] : nullptr;
    const char* only = argc > 2 ? argv[2] : nullptr;
    const double freqs[] = { 40, 50, 60, 70, 80, 90, 100, 110, 125, 150, 175, 200, 300, 500, 1000 };

    for (const auto& c : cfgs)
    {
        if (only != nullptr && std::strstr (c.name, only) == nullptr) continue;
        std::printf ("=== %s ===\n", c.name);
        FILE* csv = nullptr;
        if (dir != nullptr) { char path[512]; std::snprintf (path, sizeof path, "%s/%s.csv", dir, c.name); csv = std::fopen (path, "w"); }
        if (csv) std::fprintf (csv, "f,small_db,driven_db\n");

        std::printf ("  f(Hz)  small(dB re in)  driven(dBFS @0.1)\n");
        for (double f : freqs)
        {
            const long n = 96000; // 1 s settle + 1 s window (integer f -> whole cycles)
            const double aS = 1.0e-5, aD = 0.1;
            auto yS = render (c, [&] (long t) { return aS * std::sin (twoPi * f * (double) t / sr); }, n);
            auto yD = render (c, [&] (long t) { return aD * std::sin (twoPi * f * (double) t / sr); }, n);
            const double gS = db (tone (yS, 48000, 48000, f) / aS), lD = db (tone (yD, 48000, 48000, f));
            std::printf ("  %5.0f  %10.2f  %12.2f\n", f, gS, lD);
            if (csv) std::fprintf (csv, "%g,%.4f,%.4f\n", f, gS, lD);
        }
        if (csv) std::fclose (csv);

        { // low-E THD, 2 s window = 165 cycles of 82.5 Hz
            const double f0 = 82.5;
            auto y = render (c, [&] (long t) { return 0.25 * std::sin (twoPi * f0 * (double) t / sr); }, 48000 + 96000);
            const double h1 = tone (y, 48000, 96000, f0); double hs = 0; int dense = 0;
            for (int k = 2; k <= 40; ++k) { const double h = tone (y, 48000, 96000, f0 * k); hs += h * h; if (h > 0.01 * h1) ++dense; }
            std::printf ("  lowE THD: %.2f dB (%.1f%%)  n(-40dBc)=%d  H1=%.2f dBFS\n", db (std::sqrt (hs) / h1), 100 * std::sqrt (hs) / h1, dense, db (h1));
        }
        const int ord = 17; // 2.73 s
        { // two-tone power chord
            const double f1 = 82.5, f2 = 123.75, g = 41.25;
            auto y = render (c, [&] (long t) { return 0.1 * std::sin (twoPi * f1 * t / sr) + 0.1 * std::sin (twoPi * f2 * t / sr); }, 48000 + (1L << ord));
            auto P = powerSpectrum (y, 48000, ord);
            const double ref = peakPow (P, ord, f1) + peakPow (P, ord, f2);
            double im = 0;
            for (int k = 1; k * g < 6000.0; ++k) if (k % 2 != 0 && k % 3 != 0) im += peakPow (P, ord, k * g);
            std::printf ("  2-tone IMD (off-grid products re f1+f2): %.2f dB   diff tone 41.25 Hz: %.2f dBc   sub-60Hz total: %.2f dBc\n",
                         10 * std::log10 (im / ref), 10 * std::log10 (peakPow (P, ord, g) / ref), 10 * std::log10 (bandPow (P, ord, 20, 60) / ref));
        }
        { // SMPTE-style
            const double fl = 82.5, fh = 2000.0;
            auto y = render (c, [&] (long t) { return 0.16 * std::sin (twoPi * fl * t / sr) + 0.04 * std::sin (twoPi * fh * t / sr); }, 48000 + (1L << ord));
            auto P = powerSpectrum (y, 48000, ord);
            double sb = 0; for (int k = 1; k <= 4; ++k) sb += peakPow (P, ord, fh - k * fl) + peakPow (P, ord, fh + k * fl);
            std::printf ("  SMPTE IMD (2k +-n*82.5, n<=4, re 2k): %.2f dB\n", 10 * std::log10 (sb / peakPow (P, ord, fh)));
        }
        { // palm-mute chugs
            const double f0 = 82.41, period = 60.0 / 140.0 / 2.0; // 8ths at 140 bpm = 0.214 s
            const double hold = 0.15;
            auto in = [&] (long t) {
                const double ts = (double) t / sr; const long hit = (long) std::floor (ts / period); const double u = ts - hit * period;
                if (hit < 2) return 0.0; // 0.43 s of silence first
                double s = 0;
                for (int k = 1; k <= 24; ++k) s += std::pow ((double) k, -1.1) * std::exp (-u * (8.0 + 6.0 * k)) * std::sin (twoPi * f0 * k * u + 0.3 * k);
                const double att = std::min (1.0, u / 0.002), mute = u < hold ? 1.0 : std::max (0.0, 1.0 - (u - hold) / 0.005);
                return 0.30 * s * att * mute / 1.6;
            };
            const long n = (long) (sr * period * 18);
            auto y = render (c, in, n);
            auto x = std::vector<double> ((size_t) n); for (long t = 0; t < n; ++t) x[(size_t) t] = in (t);
            Biquad l1 (200.0, 0.5412), l2 (200.0, 1.3066); // 4th-order Butterworth LP
            std::vector<double> lf ((size_t) n); for (long t = 0; t < n; ++t) lf[(size_t) t] = l2 (l1 (y[(size_t) t]));
            auto rms = [] (const std::vector<double>& v, long a, long b) { double s = 0; for (long i = a; i < b; ++i) s += v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / (double) (b - a)); };
            double hitLf = 0, gapLf = 0, hitAll = 0, gapAll = 0; int cnt = 0;
            for (int h = 6; h < 17; ++h)
            {
                const long a = (long) (h * period * sr), g0 = (long) ((h * period + hold + 0.012) * sr), g1 = (long) ((h + 1) * period * sr) - 48;
                hitLf += rms (lf, a, a + (long) (0.06 * sr)); gapLf += rms (lf, g0, g1);
                hitAll += rms (y, a, a + (long) (0.06 * sr)); gapAll += rms (y, g0, g1); ++cnt;
            }
            const long a0 = (long) (6 * period * sr);
            auto P = powerSpectrum (y, a0, 15);
            const double tot = bandPow (P, 15, 20, 20000);
            std::printf ("  palm mute: LF(<=200) hit %.2f dBFS  gap-flub %.2f dB re hit (full-band gap %.2f dB)\n",
                         db (hitLf / cnt), db (gapLf / hitLf), db (gapAll / hitAll));
            std::printf ("  palm mute bands re total: 20-60 %.2f | 60-120 %.2f | 120-250 %.2f | 250-800 %.2f | 0.8-5k %.2f dB   total %.2f dBFS(rel)\n",
                         10 * std::log10 (bandPow (P, 15, 20, 60) / tot), 10 * std::log10 (bandPow (P, 15, 60, 120) / tot), 10 * std::log10 (bandPow (P, 15, 120, 250) / tot),
                         10 * std::log10 (bandPow (P, 15, 250, 800) / tot), 10 * std::log10 (bandPow (P, 15, 800, 5000) / tot), db (rms (y, a0, n)));
            if (dir != nullptr) { char path[512]; std::snprintf (path, sizeof path, "%s/%s-palm.f32", dir, c.name);
                if (auto* f = std::fopen (path, "wb")) { std::vector<float> fy (y.begin(), y.end()); std::fwrite (fy.data(), 4, fy.size(), f); std::fclose (f); } }
            (void) x;
        }
    }
    return 0;
}
