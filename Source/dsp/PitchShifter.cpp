#include "PitchShifter.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace jb::dsp
{
namespace
{
    constexpr double twoPi = 2.0 * kPi;

    inline double princarg (double a) noexcept
    {
        return a - twoPi * std::floor ((a + kPi) / twoPi);
    }

    int nextPow2 (int v)
    {
        int p = 1;
        while (p < v) p <<= 1;
        return p;
    }
}

//==============================================================================
// GrainShifter
//==============================================================================
void GrainShifter::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate;
    minDelay = std::max (4.0, 0.00025 * fs);
    const int ringSize = nextPow2 ((int) std::ceil (minDelay + 1.5 * maxGrainSeconds * fs) + 16);
    warmLimit = (double) ringSize + 256.0;
    ringMask = ringSize - 1;
    for (int c = 0; c < kMaxChannels; ++c)
        rings[(size_t) c].assign ((size_t) (c < std::max (1, numChannels) ? ringSize : 0), 0.0f);

    decim = std::max (1, (int) std::lround (fs / 12000.0));
    const double rate = fs / decim;
    detector.prepare (rate, 256, 512, 45.0, 1000.0);
    hop = 128;
    const auto fc = 0.4 * rate;
    aa1.setCoeffs (BiquadCoeffs::lowPass (fs, std::min (fc, 0.45 * fs), 0.5412));
    aa2.setCoeffs (BiquadCoeffs::lowPass (fs, std::min (fc, 0.45 * fs), 1.3066));
    hist.assign ((size_t) detector.frameLength(), 0.0f);
    frame.assign ((size_t) detector.frameLength(), 0.0f);
    reset();
}

void GrainShifter::reset() noexcept
{
    for (auto& r : rings) std::fill (r.begin(), r.end(), 0.0f);
    writePos = 0;
    written = 0.0;
    period = 0.0;
    sinceValid = 1.0e9;
    targetW = grainFor (0.0, 1.0);
    grain[0] = grain[1] = targetW;
    phase[0] = 0.0;
    phase[1] = 0.5;
    aa1.reset();
    aa2.reset();
    std::fill (hist.begin(), hist.end(), 0.0f);
    histPos = histFill = 0;
    decimPhase = sinceDetect = 0;
}

double GrainShifter::grainFor (double p, double ratio) const noexcept
{
    const double slope = std::abs (1.0 - ratio);
    const double wMin = minGrainSeconds * fs * std::clamp (slope, 1.0, 1.5);
    const double wMax = maxGrainSeconds * fs;
    if (p <= 0.0)
        return std::max (wMin, defaultGrainSeconds * fs);
    // An EVEN number of periods: the two taps sit W/2 apart, so W/2 must be a
    // whole number of periods too or the cross-fade would mix out-of-phase copies.
    const double two = 2.0 * p;
    if (two >= wMax)
        return two <= maxGrainSeconds * fs * 1.5 ? two : wMax;
    double w = two * std::ceil (wMin / two);
    if (w > wMax)
        w = two * std::floor (wMax / two);
    return w;
}

void GrainShifter::analyse (const float* const* chans, int numChannels, int n, double ratioNow) noexcept
{
    const int len = (int) hist.size();
    const float norm = 1.0f / (float) numChannels;
    for (int i = 0; i < n; ++i)
    {
        float v = 0.0f;
        for (int c = 0; c < numChannels; ++c) v += chans[c][i];
        v = aa2.process (aa1.process (v * norm, 0), 0);
        if (++decimPhase < decim) continue;
        decimPhase = 0;
        hist[(size_t) histPos] = v;
        histPos = (histPos + 1) % len;
        histFill = std::min (histFill + 1, len);
        if (++sinceDetect < hop || histFill < len) continue;
        sinceDetect = 0;
        for (int k = 0; k < len; ++k)
            frame[(size_t) k] = hist[(size_t) ((histPos + k) % len)];
        const auto r = detector.detect (frame.data());
        if (r.valid && r.clarity > 0.85)
        {
            period = fs / r.hz;
            sinceValid = 0.0;
        }
        else
        {
            sinceValid += hop * decim;
            if (sinceValid > 0.3 * fs) period = 0.0;
        }
    }
    targetW = grainFor (period, ratioNow);
}

inline float GrainShifter::tap (const std::vector<float>& ring, double delay) const noexcept
{
    const double pos = (double) writePos - delay;
    const double fl = std::floor (pos);
    const auto frac = (float) (pos - fl);
    const int i = (int) fl;
    const float xm1 = ring[(size_t) ((i - 1) & ringMask)];
    const float x0  = ring[(size_t) (i & ringMask)];
    const float x1  = ring[(size_t) ((i + 1) & ringMask)];
    const float x2  = ring[(size_t) ((i + 2) & ringMask)];
    // 4-point, 3rd-order Hermite
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * frac + c2) * frac + c1) * frac + x0;
}

void GrainShifter::process (float* const* chans, int numChannels, int n, const float* ratio) noexcept
{
    numChannels = std::min (numChannels, kMaxChannels);
    if (n <= 0 || numChannels <= 0) return;
    analyse (chans, numChannels, n, ratio[n - 1]);

    for (int i = 0; i < n; ++i)
    {
        for (int c = 0; c < numChannels; ++c)
            rings[(size_t) c][(size_t) writePos] = chans[c][i];

        const double d0 = minDelay + phase[0] * grain[0];
        const double d1 = minDelay + phase[1] * grain[1];
        const double s0 = std::sin (kPi * phase[0]), s1 = std::sin (kPi * phase[1]);
        auto g0 = (float) (s0 * s0), g1 = (float) (s1 * s1);
        const float nrm = 1.0f / std::max (g0 + g1, 0.25f);
        if (written < warmLimit)
        {
            // after a reset a tap only fades in once the line holds signal at its delay
            written += 1.0;
            g0 *= (float) std::clamp ((written - d0 - 4.0) / 128.0, 0.0, 1.0);
            g1 *= (float) std::clamp ((written - d1 - 4.0) / 128.0, 0.0, 1.0);
        }
        for (int c = 0; c < numChannels; ++c)
        {
            const auto& ring = rings[(size_t) c];
            chans[c][i] = (g0 * tap (ring, d0) + g1 * tap (ring, d1)) * nrm;
        }

        const double slope = 1.0 - (double) ratio[i]; // delay change per sample
        phase[0] += slope / grain[0];
        phase[1] += slope / grain[1];
        for (int t = 0; t < 2; ++t)
        {
            // (both taps advanced first: the re-sync below compares same-instant phases)
            bool wrapped = false;
            if (phase[t] >= 1.0) { phase[t] -= 1.0; wrapped = true; }
            else if (phase[t] < 0.0) { phase[t] += 1.0; wrapped = true; }
            if (wrapped)
            {
                // the tap is silent here: take the new (pitch-synchronous) grain
                // and pull it gently back to half a cycle from the other tap
                grain[t] = targetW;
                const double want = phase[1 - t] + 0.5;
                const double err = princarg (twoPi * (want - phase[t])) / twoPi;
                phase[t] += std::clamp (err, -0.02, 0.02);
                phase[t] -= std::floor (phase[t]);
            }
        }
        writePos = (writePos + 1) & ringMask;
    }
}

//==============================================================================
// PhaseVocoderShifter
//==============================================================================
void PhaseVocoderShifter::prepare (double sampleRate, int numChannels)
{
    const int mult = nextPow2 (std::max (1, (int) std::lround (sampleRate / 48000.0)));
    order = 11;
    for (int m = mult; m > 1; m >>= 1) ++order;
    fftSize = 1 << order;
    hopSize = fftSize / 8;
    bins = fftSize / 2 + 1;
    fft = std::make_unique<juce::dsp::FFT> (order);
    window.resize ((size_t) fftSize);
    double sumSq = 0.0;
    for (int i = 0; i < fftSize; ++i)
    {
        window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (twoPi * i / fftSize));
        sumSq += (double) window[(size_t) i] * window[(size_t) i];
    }
    olaScale = (float) (hopSize / sumSq); // analysis x synthesis Hann at N/H overlap
    for (int c = 0; c < kMaxChannels; ++c)
    {
        auto& s = ch[(size_t) c];
        const bool used = c < std::max (1, numChannels);
        s.inFifo.assign (used ? (size_t) fftSize : 0, 0.0f);
        s.outFifo.assign (used ? (size_t) fftSize : 0, 0.0f);
        s.outAccum.assign (used ? (size_t) fftSize * 2 : 0, 0.0f);
        s.work.assign (used ? (size_t) fftSize * 2 : 0, 0.0f);
        s.prevSpec.assign (used ? (size_t) bins : 0, {});
        s.synth.assign (used ? (size_t) bins : 0, {});
        s.rotByBin.assign (used ? (size_t) bins : 0, 0.0f);
        s.newRot.assign (used ? (size_t) bins : 0, 0.0f);
    }
    mag2.assign ((size_t) bins, 0.0f);
    peaks.reserve ((size_t) bins);
    reset();
}

void PhaseVocoderShifter::reset() noexcept
{
    for (auto& s : ch)
    {
        std::fill (s.inFifo.begin(), s.inFifo.end(), 0.0f);
        std::fill (s.outFifo.begin(), s.outFifo.end(), 0.0f);
        std::fill (s.outAccum.begin(), s.outAccum.end(), 0.0f);
        std::fill (s.prevSpec.begin(), s.prevSpec.end(), std::complex<float> {});
        std::fill (s.rotByBin.begin(), s.rotByBin.end(), 0.0f);
    }
    rover = fftSize - hopSize;
}

void PhaseVocoderShifter::process (float* const* chans, int numChannels, int n, const float* ratio) noexcept
{
    numChannels = std::min (numChannels, kMaxChannels);
    const int latency = fftSize - hopSize;
    for (int i = 0; i < n; ++i)
    {
        for (int c = 0; c < numChannels; ++c)
        {
            auto& s = ch[(size_t) c];
            s.inFifo[(size_t) rover] = chans[c][i];
            chans[c][i] = s.outFifo[(size_t) (rover - latency)];
        }
        if (++rover >= fftSize)
        {
            rover = latency;
            for (int c = 0; c < numChannels; ++c)
                processFrame (c, ratio[i]);
        }
    }
}

void PhaseVocoderShifter::processFrame (int c, float ratio) noexcept
{
    auto& s = ch[(size_t) c];
    const int N = fftSize, H = hopSize;
    auto* w = s.work.data();
    for (int k = 0; k < N; ++k) w[k] = s.inFifo[(size_t) k] * window[(size_t) k];
    std::fill (w + N, w + 2 * N, 0.0f);
    fft->performRealOnlyForwardTransform (w, true);
    auto* spec = reinterpret_cast<std::complex<float>*> (w); // bins 0..N/2

    float maxMag = 0.0f;
    for (int k = 0; k < bins; ++k)
    {
        mag2[(size_t) k] = std::norm (spec[k]);
        maxMag = std::max (maxMag, mag2[(size_t) k]);
    }
    const float thresh = std::max (maxMag * 1.0e-8f, 1.0e-14f);
    peaks.clear();
    for (int k = 1; k < bins - 1; ++k)
    {
        const auto m = mag2[(size_t) k];
        if (m > thresh && m >= mag2[(size_t) k - 1] && m > mag2[(size_t) k + 1])
            peaks.push_back (k); // 3-point maxima: chord partials 2 bins apart stay separate
    }

    std::fill (s.synth.begin(), s.synth.end(), std::complex<float> {});
    std::fill (s.newRot.begin(), s.newRot.end(), 0.0f);
    const double binW = twoPi / N;
    const auto np = (int) peaks.size();
    for (int j = 0; j < np; ++j)
    {
        const int p = peaks[(size_t) j];
        const int lo = j == 0 ? 0 : (peaks[(size_t) j - 1] + p) / 2 + 1;
        const int hi = j == np - 1 ? bins - 1 : (p + peaks[(size_t) j + 1]) / 2;

        const double dphi = std::arg (spec[p] * std::conj (s.prevSpec[(size_t) p]));
        const double omega = p * binW + princarg (dphi - p * binW * H) / H; // rad / sample
        const double dOmega = omega * (ratio - 1.0);
        const int shift = (int) std::lround (dOmega / binW);
        const auto rot = (float) princarg (s.rotByBin[(size_t) p] + H * dOmega);
        const std::complex<float> z (std::cos (rot), std::sin (rot));
        for (int b = lo; b <= hi; ++b)
        {
            s.newRot[(size_t) b] = rot;
            const int t = b + shift;
            if (t >= 0 && t < bins)
                s.synth[(size_t) t] += spec[b] * z;
        }
    }
    for (int k = 0; k < bins; ++k) s.prevSpec[(size_t) k] = spec[k];
    std::swap (s.rotByBin, s.newRot);

    s.synth[0] = { s.synth[0].real(), 0.0f };
    s.synth[(size_t) bins - 1] = { s.synth[(size_t) bins - 1].real(), 0.0f };
    for (int k = 0; k < bins; ++k) spec[k] = s.synth[(size_t) k];
    std::fill (w + 2 * bins, w + 2 * N, 0.0f);
    fft->performRealOnlyInverseTransform (w);

    auto* acc = s.outAccum.data();
    for (int k = 0; k < N; ++k) acc[k] += w[k] * window[(size_t) k] * olaScale;
    for (int k = 0; k < H; ++k) s.outFifo[(size_t) k] = acc[k];
    std::memmove (acc, acc + H, sizeof (float) * (size_t) (2 * N - H));
    std::fill (acc + 2 * N - H, acc + 2 * N, 0.0f);
    std::memmove (s.inFifo.data(), s.inFifo.data() + H, sizeof (float) * (size_t) (N - H));
}

//==============================================================================
// PitchPedal
//==============================================================================
float PitchPedal::targetSemitones (const Settings& s) noexcept
{
    if (! (s.whammy || s.transpose))
        return 0.0f;
    const float interval = (s.whammy ? std::clamp (s.sweep, 0.0f, 1.0f) : 1.0f) * s.shift;
    return interval + (s.transpose ? s.detuneCents * 0.01f : 0.0f);
}

void PitchPedal::setParameters (const Settings& s) noexcept
{
    engage.setTargetValue (s.on ? 1.0f : 0.0f);
    const auto st = targetSemitones (s);
    semis.setTargetValue (st);
    park.setTargetValue (std::abs (st) < 0.005f ? 1.0f : 0.0f);
    wet.setTargetValue (s.harmony ? std::clamp (s.mix * 0.01f, 0.0f, 1.0f) : 1.0f);
    polyMix.setTargetValue (s.poly ? 1.0f : 0.0f);
    shiftSource = s.whammy || s.transpose;
}

void PitchPedal::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    maxBlock = std::max (1, maxBlockSize);
    mono.prepare (sampleRate, numChannels);
    poly.prepare (sampleRate, numChannels);
    for (int c = 0; c < kMaxChannels; ++c)
    {
        dry[(size_t) c].assign ((size_t) maxBlock, 0.0f);
        polyOut[(size_t) c].assign ((size_t) maxBlock, 0.0f);
    }
    ratio.assign ((size_t) maxBlock, 1.0f);
    // reset() snaps every smoother to its target: no fade on load
    engage.reset (sampleRate, engageSeconds);
    semis.reset (sampleRate, glideSeconds);
    park.reset (sampleRate, glideSeconds);
    wet.reset (sampleRate, 0.020);
    polyMix.reset (sampleRate, engineFadeSeconds);
    monoRunning = polyRunning = false;
}

void PitchPedal::process (float* const* chans, int numChannels, int n) noexcept
{
    if (! isActive())
        return; // off and settled: true bypass, the buffer is not touched
    numChannels = std::min (numChannels, kMaxChannels);

    for (int off = 0; off < n; off += maxBlock)
    {
        const int len = std::min (maxBlock, n - off);
        float* x[kMaxChannels] {};
        float* p[kMaxChannels] {};
        for (int c = 0; c < numChannels; ++c)
        {
            x[c] = chans[c] + off;
            p[c] = polyOut[(size_t) c].data();
            std::copy (x[c], x[c] + len, dry[(size_t) c].data());
        }

        const bool parkedDry = park.getCurrentValue() >= 1.0f && ! park.isSmoothing();
        const bool allPoly   = polyMix.getCurrentValue() >= 1.0f && ! polyMix.isSmoothing();
        const bool allMono   = polyMix.getCurrentValue() <= 0.0f && ! polyMix.isSmoothing();
        // With an interval source on (Whammy / Transpose) the engine stays warm
        // even while parked on the dry signal (Whammy heel), so a sweep away
        // from the heel starts from a full delay line instead of a cold start.
        const bool needMono = (! parkedDry || shiftSource) && ! allPoly;
        const bool needPoly = (! parkedDry || shiftSource) && ! allMono;
        if (needMono && ! monoRunning) mono.reset();
        if (needPoly && ! polyRunning) poly.reset();
        monoRunning = needMono;
        polyRunning = needPoly;

        if (needMono || needPoly)
        {
            for (int i = 0; i < len; ++i)
                ratio[(size_t) i] = std::exp2 (semis.getNextValue() * (1.0f / 12.0f));
        }
        else
            semis.skip (len);

        if (needPoly)
        {
            for (int c = 0; c < numChannels; ++c) std::copy (x[c], x[c] + len, p[c]);
            poly.process (p, numChannels, len, ratio.data());
        }
        if (needMono)
            mono.process (x, numChannels, len, ratio.data());

        for (int i = 0; i < len; ++i)
        {
            const auto e = engage.getNextValue();
            const auto pk = park.getNextValue();
            const auto wt = wet.getNextValue();
            const auto pm = polyMix.getNextValue();
            for (int c = 0; c < numChannels; ++c)
            {
                const auto xd = dry[(size_t) c][(size_t) i];
                const auto m  = x[c][i];                       // mono engine (or dry)
                const auto pv = needPoly ? p[c][i] : xd;
                const auto eng = ! needMono ? pv : (pm <= 0.0f ? m : (pm >= 1.0f ? pv : m + pm * (pv - m)));
                const auto shifted = pk >= 1.0f ? xd : (pk <= 0.0f ? eng : eng + pk * (xd - eng));
                const auto ped = wt >= 1.0f ? shifted : (wt <= 0.0f ? xd : xd + wt * (shifted - xd));
                x[c][i] = e >= 1.0f ? ped : (e <= 0.0f ? xd : xd + e * (ped - xd));
            }
        }
    }
}
} // namespace jb::dsp
