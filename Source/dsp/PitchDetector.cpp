#include "PitchDetector.h"

#include <algorithm>
#include <cmath>

namespace jb::dsp
{
namespace { constexpr double pi = 3.14159265358979323846; }

void PitchDetector::prepare (double analysisRate, int window, int maxLag, double minHz, double maxHz)
{
    rate = analysisRate;
    w = window;
    lagMax = maxLag;
    fMin = minHz;
    fMax = maxHz;
    n = 1;
    while (n < w + lagMax) n <<= 1;
    tauMin = std::max (2, (int) std::floor (rate / fMax));
    tauFirstMax = std::min (lagMax - 2, (int) std::ceil (rate / fMin));

    fa.assign ((size_t) n, {});
    fb.assign ((size_t) n, {});
    x.assign ((size_t) (w + lagMax), 0.0);
    corr.assign ((size_t) lagMax + 1, 0.0);
    e2.assign ((size_t) lagMax + 1, 0.0);
    rho.assign ((size_t) lagMax + 1, 0.0);
    cmnd.assign ((size_t) lagMax + 1, 1.0);

    int bits = 0;
    while ((1 << bits) < n) ++bits;
    bitrev.assign ((size_t) n, 0);
    for (int i = 0; i < n; ++i)
    {
        int r = 0;
        for (int b = 0; b < bits; ++b)
            if (i & (1 << b)) r |= 1 << (bits - 1 - b);
        bitrev[(size_t) i] = r;
    }
    twiddle.resize ((size_t) n / 2);
    for (int i = 0; i < n / 2; ++i)
        twiddle[(size_t) i] = std::polar (1.0, -2.0 * pi * i / n);
}

void PitchDetector::fft (std::vector<std::complex<double>>& a, bool inverse) noexcept
{
    for (int i = 0; i < n; ++i)
        if (i < bitrev[(size_t) i]) std::swap (a[(size_t) i], a[(size_t) bitrev[(size_t) i]]);
    for (int len = 2; len <= n; len <<= 1)
    {
        const int step = n / len;
        for (int i = 0; i < n; i += len)
            for (int j = 0; j < len / 2; ++j)
            {
                auto t = twiddle[(size_t) (j * step)];
                if (inverse) t = std::conj (t);
                const auto u = a[(size_t) (i + j)];
                const auto v = a[(size_t) (i + j + len / 2)] * t;
                a[(size_t) (i + j)] = u + v;
                a[(size_t) (i + j + len / 2)] = u - v;
            }
    }
}

PitchDetector::Result PitchDetector::detect (const float* frame) noexcept
{
    Result res;
    const int len = w + lagMax;

    double mean = 0.0;
    for (int i = 0; i < len; ++i) mean += frame[i];
    mean /= len;
    double ms = 0.0;
    for (int i = 0; i < len; ++i)
    {
        x[(size_t) i] = frame[i] - mean;
        ms += x[(size_t) i] * x[(size_t) i];
    }
    res.rmsDb = 10.0 * std::log10 (ms / len + 1e-30);
    if (res.rmsDb < silenceDb)
        return res;

    // c(tau) = sum_{j<W} x_j x_{j+tau}  via FFT cross-correlation
    for (int i = 0; i < n; ++i)
    {
        fa[(size_t) i] = { i < w ? x[(size_t) i] : 0.0, 0.0 };
        fb[(size_t) i] = { i < len ? x[(size_t) i] : 0.0, 0.0 };
    }
    fft (fa, false);
    fft (fb, false);
    for (int i = 0; i < n; ++i)
        fa[(size_t) i] = std::conj (fa[(size_t) i]) * fb[(size_t) i];
    fft (fa, true);
    for (int t = 0; t <= lagMax; ++t)
        corr[(size_t) t] = fa[(size_t) t].real() / n;

    // window energies: e1 = sum_{j<W} x_j^2, e2(tau) = sum_{j<W} x_{j+tau}^2
    e1 = 0.0;
    for (int j = 0; j < w; ++j) e1 += x[(size_t) j] * x[(size_t) j];
    {
        double e = e1;
        e2[0] = e;
        for (int t = 1; t <= lagMax; ++t)
        {
            const auto out = x[(size_t) (t - 1)], in = x[(size_t) (t + w - 1)];
            e += in * in - out * out;
            e2[(size_t) t] = e;
        }
    }
    for (int t = 0; t <= lagMax; ++t)
        rho[(size_t) t] = corr[(size_t) t] / std::sqrt (std::max (e1 * e2[(size_t) t], 1e-30));

    // YIN cumulative-mean-normalised difference
    cmnd[0] = 1.0;
    double run = 0.0;
    for (int t = 1; t <= tauFirstMax + 1; ++t)
    {
        const auto d = std::max (0.0, e1 + e2[(size_t) t] - 2.0 * corr[(size_t) t]);
        run += d;
        cmnd[(size_t) t] = run > 0.0 ? d * t / run : 1.0;
    }

    int tau = -1;
    for (int t = tauMin; t <= tauFirstMax; ++t)
    {
        if (cmnd[(size_t) t] < yinThreshold)
        {
            while (t + 1 <= tauFirstMax && cmnd[(size_t) t + 1] < cmnd[(size_t) t]) ++t;
            tau = t;
            break;
        }
    }
    if (tau < 0)
    {
        // no dip below threshold: take the global minimum if it is reasonably periodic
        int best = tauMin;
        for (int t = tauMin; t <= tauFirstMax; ++t)
            if (cmnd[(size_t) t] < cmnd[(size_t) best]) best = t;
        if (cmnd[(size_t) best] > 0.35)
            return res;
        tau = best;
    }

    double tau0 = tau;
    if (tau > 1 && tau < tauFirstMax + 1)
    {
        const auto a = cmnd[(size_t) tau - 1], b = cmnd[(size_t) tau], c = cmnd[(size_t) tau + 1];
        const auto den = a - 2.0 * b + c;
        if (den > 0.0) tau0 += 0.5 * (a - c) / den;
    }

    res.clarity = 1.0 - cmnd[(size_t) tau];
    const auto refined = refine (tau0);
    res.hz = rate / refined;
    res.valid = res.hz >= fMin * 0.97 && res.hz <= fMax * 1.03;
    return res;
}

double PitchDetector::refine (double tau0) const noexcept
{
    // Largest multiple of the period inside the lag range, then a parabolic
    // peak of the normalised cross-correlation there: error / k.
    int k = std::max (1, (int) std::floor ((lagMax - 3) / tau0));
    k = std::min (k, 64);
    for (; k >= 1; k = (k > 1 ? k / 2 : 0))
    {
        const auto centre = k * tau0;
        const int radius = std::max (2, (int) std::ceil (0.03 * tau0));
        int best = -1;
        double bestV = -2.0;
        for (int t = (int) std::floor (centre) - radius; t <= (int) std::ceil (centre) + radius; ++t)
        {
            if (t < 1 || t >= lagMax) continue;
            if (rho[(size_t) t] > bestV) { bestV = rho[(size_t) t]; best = t; }
        }
        if (best < 1 || bestV < 0.6) continue;
        const auto a = rho[(size_t) best - 1], b = rho[(size_t) best], c = rho[(size_t) best + 1];
        const auto den = a - 2.0 * b + c;
        double peak = best;
        if (den < 0.0) peak += 0.5 * (a - c) / den;
        if (std::abs (peak - centre) > radius) continue;
        return peak / k;
    }
    return tau0;
}
} // namespace jb::dsp
