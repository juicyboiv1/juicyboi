#include "Tuner.h"

#include <algorithm>
#include <cmath>

namespace jb::dsp
{
namespace { constexpr double pi = 3.14159265358979323846; }

void Tuner::prepare (double sampleRate, int)
{
    std::lock_guard<std::mutex> lk (analysisLock);
    decim = std::max (1, (int) std::ceil (sampleRate / 50000.0));
    analysisRate = sampleRate / decim;
    decimPhase = 0;

    // Anti-alias low-pass before decimation: 4th-order Butterworth at 0.4 x the analysis rate
    for (size_t i = 0; i < aa.size(); ++i)
    {
        auto& b = aa[i];
        if (decim == 1) { b = Biquad {}; continue; }
        const double q = i == 0 ? 0.5411961 : 1.3065630;
        const double k = std::tan (pi * 0.4 * analysisRate / sampleRate);
        const double norm = 1.0 / (1.0 + k / q + k * k);
        b.b0 = k * k * norm; b.b1 = 2.0 * b.b0; b.b2 = b.b0;
        b.a1 = 2.0 * (k * k - 1.0) * norm; b.a2 = (1.0 - k / q + k * k) * norm;
        b.z1 = b.z2 = 0.0;
    }

    detector.prepare (analysisRate, 2048, 4096);
    frame.assign ((size_t) detector.frameLength(), 0.0f);
    int size = 1;
    while (size < 4 * detector.frameLength()) size <<= 1;
    std::vector<std::atomic<float>> fresh ((size_t) size);
    for (auto& v : fresh) v.store (0.0f, std::memory_order_relaxed);
    ring.swap (fresh);
    ringMask = size - 1;
    writePos.store (0);

    muteStep = (float) (1.0 / (0.010 * sampleRate));   // 10 ms mute fade
    muteGain = muteTarget = isEnabled() ? 0.0f : 1.0f;
    recentCount = 0;
    smoothValid = false;
    dHas.store (false);
}

void Tuner::push (const float* mono, int n) noexcept
{
    if (ring.empty()) return;
    auto wp = writePos.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        double v = mono[i];
        if (decim > 1)
        {
            v = aa[1].run (aa[0].run (v));
            if (++decimPhase < decim) continue;
            decimPhase = 0;
        }
        ring[(size_t) (wp & ringMask)].store ((float) v, std::memory_order_relaxed);
        ++wp;
    }
    writePos.store (wp, std::memory_order_release);
}

void Tuner::applyMute (float* const* chans, int numCh, int n) noexcept
{
    if (muteGain >= 1.0f && muteTarget >= 1.0f)
        return; // tuner off and settled: untouched (bit-identical)

    for (int i = 0; i < n; ++i)
    {
        if (muteGain < muteTarget)      muteGain = std::min (muteTarget, muteGain + muteStep);
        else if (muteGain > muteTarget) muteGain = std::max (muteTarget, muteGain - muteStep);
        // raised-cosine shape of the linear ramp: smooth start and end
        const auto g = 0.5f - 0.5f * std::cos ((float) pi * muteGain);
        for (int c = 0; c < numCh; ++c)
            chans[c][i] *= g;
    }
}

void Tuner::resetDisplay()
{
    std::lock_guard<std::mutex> lk (analysisLock);
    recentCount = 0;
    smoothValid = false;
    sinceValid = 1.0e9;
    dHas.store (false);
    dRaw.store (0.0);
}

void Tuner::analyse (double elapsed)
{
    std::lock_guard<std::mutex> lk (analysisLock);
    if (ring.empty()) return;

    const int len = (int) frame.size();
    const auto wp = writePos.load (std::memory_order_acquire);
    if (wp < len) return;
    for (int i = 0; i < len; ++i)
        frame[(size_t) i] = ring[(size_t) ((wp - len + i) & ringMask)].load (std::memory_order_relaxed);

    const auto r = detector.detect (frame.data());
    const auto ref = refHz.load (std::memory_order_relaxed);
    dRaw.store (r.valid ? r.hz : 0.0, std::memory_order_relaxed);

    if (! r.valid)
    {
        sinceValid += elapsed;
        if (sinceValid > 0.4)
        {
            smoothValid = false;
            recentCount = 0;
            dHas.store (false, std::memory_order_relaxed);
        }
        return;
    }
    sinceValid = 0.0;

    const auto cents = 1200.0 * std::log2 (r.hz / ref);   // re A4

    // A different note must show up on two frames in a row before the display jumps.
    const int note = (int) std::lround (cents / 100.0);
    if (smoothValid && std::abs (cents - smoothCents) > 60.0)
    {
        if (note == pendingNote) ++pendingCount;
        else { pendingNote = note; pendingCount = 1; }
        if (pendingCount < 2) return;
        recentCount = 0;
        smoothValid = false;
    }
    pendingCount = 0;
    pendingNote = -1000;

    // median of the last 5, then a one-pole in cents
    for (size_t i = recent.size() - 1; i > 0; --i) recent[i] = recent[i - 1];
    recent[0] = cents;
    recentCount = std::min (recentCount + 1, (int) recent.size());
    std::array<double, 5> tmp {};
    std::copy (recent.begin(), recent.begin() + recentCount, tmp.begin());
    std::sort (tmp.begin(), tmp.begin() + recentCount);
    const auto med = tmp[(size_t) recentCount / 2];

    smoothCents = smoothValid ? smoothCents + 0.35 * (med - smoothCents) : med;
    smoothValid = true;

    const int n = (int) std::lround (smoothCents / 100.0);
    dNote.store (69 + n, std::memory_order_relaxed);
    dCents.store (smoothCents - 100.0 * n, std::memory_order_relaxed);
    dHz.store (ref * std::pow (2.0, smoothCents / 1200.0), std::memory_order_relaxed);
    dHas.store (true, std::memory_order_release);
}

Tuner::Display Tuner::getDisplay() const noexcept
{
    Display d;
    d.hasPitch = dHas.load (std::memory_order_acquire);
    d.midiNote = dNote.load (std::memory_order_relaxed);
    d.cents = dCents.load (std::memory_order_relaxed);
    d.hz = dHz.load (std::memory_order_relaxed);
    d.rawHz = dRaw.load (std::memory_order_relaxed);
    return d;
}

const char* Tuner::noteName (int midiNote) noexcept
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[((midiNote % 12) + 12) % 12];
}
} // namespace jb::dsp
