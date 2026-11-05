#pragma once

#include "Filters.h"
#include <array>
#include <complex>

namespace jb::dsp
{
/** 7-band graphic EQ + Level, placed post-ToneStack / pre-Cab.

    Original juicyboi DSP inspired by classic octave-spaced pedal graphic EQs
    (the "carve after the drive" tool): bands at 100 / 200 / 400 / 800 /
    1.6k / 3.2k / 6.4k Hz, ±15 dB each, plus a ±15 dB output Level.
    Bands 1–6 are RBJ peaking biquads (Q ≈ 1.3, roughly one octave wide, so
    adjacent bands overlap smoothly like a gyrator EQ); the 6.4 kHz band is a
    high shelf so it handles top-end fizz/air rather than a narrow bump.

    Enable is a short (10 ms) crossfade; once fully off the stage is a true
    bypass (no filtering, no multiply). Coefficients are recomputed only when
    a band value changes (per block), and band gains are slewed per block to
    keep slider moves zipper-free. */
class GraphicEq
{
public:
    static constexpr int numBands = 7;
    static constexpr std::array<double, numBands> bandHz { 100.0, 200.0, 400.0, 800.0, 1600.0, 3200.0, 6400.0 };
    static constexpr double peakQ = 1.3;
    static constexpr double shelfQ = 0.707;
    static constexpr float maxDb = 15.0f;

    void prepare (double sampleRate, int maxBlock);
    void reset();

    /** gainsDb: 7 band gains (dB), levelDb: output level (dB). */
    void setParameters (bool enabled, const std::array<float, numBands>& gainsDb, float levelDb) noexcept;

    /** In-place processing of numCh channel pointers, len samples. */
    void process (float* const* chans, int numCh, int len) noexcept;

    /** True when the EQ is currently doing any work (on, or fading). */
    bool isActive() const noexcept { return active; }

    /** Magnitude response (dB) of the current target settings at hz, excluding Level. */
    double getMagnitudeDb (double hz) const noexcept;

private:
    void updateCoeffs() noexcept;

    double fs = 44100.0;
    std::array<Biquad, numBands> bands;
    std::array<float, numBands> targetDb {}, currentDb {};
    std::array<BiquadCoeffs, numBands> coeffs {};
    bool coeffsDirty = true;

    float levelTarget = 1.0f, levelCurrent = 1.0f;
    float mixTarget = 0.0f, mixCurrent = 0.0f, mixStep = 0.0f;
    bool active = false;
};
} // namespace jb::dsp
