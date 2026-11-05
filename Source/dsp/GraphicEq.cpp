#include "GraphicEq.h"

namespace jb::dsp
{
void GraphicEq::prepare (double sampleRate, int)
{
    fs = sampleRate;
    mixStep = (float) (1.0 / (0.010 * sampleRate)); // 10 ms enable fade
    currentDb = targetDb;
    levelCurrent = levelTarget;
    mixCurrent = mixTarget;
    active = mixCurrent > 0.0f;
    coeffsDirty = true;
    updateCoeffs();
    reset();
}

void GraphicEq::reset()
{
    for (auto& b : bands)
        b.reset();
}

void GraphicEq::setParameters (bool enabled, const std::array<float, numBands>& gainsDb, float levelDb) noexcept
{
    for (size_t i = 0; i < (size_t) numBands; ++i)
    {
        const auto g = std::clamp (gainsDb[i], -maxDb, maxDb);
        if (differs (g, targetDb[i]))
            targetDb[i] = g;
    }

    levelTarget = std::pow (10.0f, std::clamp (levelDb, -maxDb, maxDb) / 20.0f);

    const auto newMix = enabled ? 1.0f : 0.0f;
    if (newMix > 0.5f && ! active)
    {
        // Waking from true bypass: start from clean filter state at the
        // current settings so the fade-in has no stale ringing.
        currentDb = targetDb;
        levelCurrent = levelTarget;
        coeffsDirty = true;
        updateCoeffs();
        reset();
        active = true;
    }
    mixTarget = newMix;
}

void GraphicEq::updateCoeffs() noexcept
{
    if (! coeffsDirty)
        return;
    coeffsDirty = false;

    for (size_t i = 0; i < (size_t) numBands; ++i)
    {
        coeffs[i] = (i + 1 == (size_t) numBands)
                        ? BiquadCoeffs::highShelf (fs, bandHz[i], shelfQ, currentDb[i])
                        : BiquadCoeffs::peak      (fs, bandHz[i], peakQ,  currentDb[i]);
        bands[i].setCoeffs (coeffs[i]);
    }
}

void GraphicEq::process (float* const* chans, int numCh, int len) noexcept
{
    if (! active || len <= 0)
        return; // true bypass

    // Per-block slew of band gains (max 1.5 dB per block) -> no zipper on drags
    for (size_t i = 0; i < (size_t) numBands; ++i)
    {
        const auto d = targetDb[i] - currentDb[i];
        if (std::abs (d) > 1.0e-4f)
        {
            currentDb[i] += std::clamp (d, -1.5f, 1.5f);
            coeffsDirty = true;
        }
    }
    updateCoeffs();

    const auto lvlStep = (levelTarget - levelCurrent) / (float) len;
    numCh = std::min (numCh, kMaxChannels);

    for (int n = 0; n < len; ++n)
    {
        levelCurrent += lvlStep;
        if (mixCurrent < mixTarget)      mixCurrent = std::min (mixTarget, mixCurrent + mixStep);
        else if (mixCurrent > mixTarget) mixCurrent = std::max (mixTarget, mixCurrent - mixStep);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const auto x = chans[ch][n];
            auto y = x;
            for (auto& b : bands)
                y = b.process (y, ch);
            y *= levelCurrent;
            chans[ch][n] = x + mixCurrent * (y - x);
        }
    }
    levelCurrent = levelTarget;

    if (mixTarget <= 0.0f && mixCurrent <= 0.0f)
        active = false; // fully faded out -> true bypass from next block
}

double GraphicEq::getMagnitudeDb (double hz) const noexcept
{
    const auto w = 2.0 * kPi * hz / fs;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
    double db = 0.0;
    for (size_t i = 0; i < (size_t) numBands; ++i)
    {
        const auto c = (i + 1 == (size_t) numBands)
                           ? BiquadCoeffs::highShelf (fs, bandHz[i], shelfQ, targetDb[i])
                           : BiquadCoeffs::peak      (fs, bandHz[i], peakQ,  targetDb[i]);
        const auto num = (double) c.b0 + (double) c.b1 * z1 + (double) c.b2 * z2;
        const auto den = 1.0 + (double) c.a1 * z1 + (double) c.a2 * z2;
        db += 20.0 * std::log10 (std::abs (num / den) + 1e-30);
    }
    return db;
}
} // namespace jb::dsp
