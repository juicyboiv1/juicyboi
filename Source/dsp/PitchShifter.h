#pragma once

#include "Filters.h"
#include "PitchDetector.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

namespace jb::dsp
{
/** Rev3.3 low-latency "mono" shifter (docs/pitch-shift.md).

    Two read taps sweep through a delay line at slope (1 - ratio) samples per
    sample and are cross-faded with sin^2 / cos^2 windows (a dual-delay
    crossfade, the classic Whammy-style grain shifter). The grain (window)
    length is pitch-synchronous: an integer number of periods of the note
    being played (period from PitchDetector, run here on a decimated copy),
    at least ~12 ms. Each tap only takes the new grain length when it wraps,
    i.e. while its gain is zero, so length changes never click. W is an EVEN
    number of periods (>= 10 ms; >= 15 ms beyond an octave up): every tap jump
    and the half-grain offset between the taps are whole periods, so the
    splices and the cross-fade line up with the waveform.

    Delay = minDelay .. minDelay + W, i.e. about W/2 on average: 5-12 ms
    depending on the note (shorter for higher notes). */
class GrainShifter
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;

    /** Process n samples in place; ratio[i] = frequency ratio per sample. */
    void process (float* const* chans, int numChannels, int n, const float* ratio) noexcept;

    /** Grain length the taps will latch at their next wrap (samples). */
    double getTargetGrain() const noexcept { return targetW; }
    /** Last detected period (samples), 0 = none. */
    double getPeriod() const noexcept { return period; }
    double getMinDelay() const noexcept { return minDelay; }
    /** Mean delay (samples) for a ratio: minDelay + W/2. */
    double meanDelayFor (double ratio) const noexcept { return minDelay + 0.5 * grainFor (period, ratio); }

    static constexpr double minGrainSeconds = 0.010;
    static constexpr double maxGrainSeconds = 0.040;
    static constexpr double defaultGrainSeconds = 0.015;

private:
    double grainFor (double periodSamples, double ratio) const noexcept;
    void analyse (const float* const* chans, int numChannels, int n, double ratioNow) noexcept;
    inline float tap (const std::vector<float>& ring, double delay) const noexcept;

    double fs = 48000.0;
    double minDelay = 12.0;
    std::array<std::vector<float>, kMaxChannels> rings;
    int ringMask = 0, writePos = 0;
    double written = 0.0, warmLimit = 0.0; // samples since reset (tap fade-in)

    double phase[2] { 0.0, 0.5 };
    double grain[2] { 720.0, 720.0 };
    double targetW = 720.0;

    // pitch-synchronous grain sizing: decimated mono copy -> PitchDetector
    PitchDetector detector;
    int decim = 4, decimPhase = 0, hop = 128, sinceDetect = 0;
    Biquad aa1, aa2;
    std::vector<float> hist, frame;
    int histPos = 0, histFill = 0;
    double period = 0.0;    // samples at fs, 0 = unknown
    double sinceValid = 1.0e9;
};

/** Rev3.3 "poly" shifter: a phase vocoder with Laroche-Dolson peak-locked
    pitch shifting (spectral peaks and their regions of influence are moved as
    rigid blocks, the phase of each region rotated by its peak's frequency
    change), which keeps chords and vertical phase coherence clean.
    FFT 2048 / hop 256 at 44.1-48 kHz (scaled with the rate, so the time
    resolution is constant). Latency = N (2048 samples = 42.7 ms at 48 kHz). */
class PhaseVocoderShifter
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void process (float* const* chans, int numChannels, int n, const float* ratio) noexcept;
    /** A sample is output once its last overlapping frame is in: N samples. */
    int getLatencySamples() const noexcept { return fftSize; }
    int getFftSize() const noexcept { return fftSize; }
    int getHopSize() const noexcept { return hopSize; }

private:
    void processFrame (int ch, float ratio) noexcept;

    int order = 11, fftSize = 2048, hopSize = 256, bins = 1025;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;
    float olaScale = 1.0f;
    int rover = 0;

    struct Chan
    {
        std::vector<float> inFifo, outFifo, outAccum, work;
        std::vector<std::complex<float>> prevSpec, synth;
        std::vector<float> rotByBin, newRot;
    };
    std::array<Chan, kMaxChannels> ch;
    std::vector<float> mag2;
    std::vector<int> peaks;
};

/** Rev3.3 PITCH pedal: shifter engines + the four mode toggles + true bypass.

    semis = (Whammy ? Sweep : 1) * Shift + (Transpose ? Detune/100 : 0)
            when Whammy or Transpose is on; otherwise no shift (dry).
    Harmony on: out = (1 - Mix) * dry + Mix * shifted; off: 100 % shifted.
    Poly on: phase vocoder; off: low-latency grain shifter (30 ms crossfade).
    |semis| < 0.005 (Whammy heel, Shift 0): the dry signal (no latency).
    Pedal off: 15 ms fade, then the buffer is not touched (true bypass). */
class PitchPedal
{
public:
    struct Settings
    {
        bool on = false;
        float shift = 12.0f;     ///< semitones, -24..+24
        float detuneCents = 0.0f;///< -50..+50
        float mix = 50.0f;       ///< Harmony blend, % shifted
        float sweep = 1.0f;      ///< Whammy heel (0) .. toe (1)
        bool whammy = false, transpose = true, harmony = false, poly = false;
    };

    void prepare (double sampleRate, int maxBlock, int numChannels);
    void setParameters (const Settings&) noexcept;
    /** Process in place. Untouched (bit-identical) while off and settled. */
    void process (float* const* chans, int numChannels, int n) noexcept;
    /** Drop engine history (the amp was silent and skipped); fades stay. */
    void resetEngines() noexcept { monoRunning = polyRunning = false; }

    bool isActive() const noexcept { return engage.getCurrentValue() > 0.0f || engage.isSmoothing(); }
    bool isMonoRunning() const noexcept { return monoRunning; }
    bool isPolyRunning() const noexcept { return polyRunning; }

    static float targetSemitones (const Settings&) noexcept;
    float getTargetSemitones() const noexcept { return semis.getTargetValue(); }

    int getPolyLatencySamples() const noexcept { return poly.getLatencySamples(); }
    const GrainShifter& getGrainShifter() const noexcept { return mono; }

    static constexpr double engageSeconds = 0.015, glideSeconds = 0.025, engineFadeSeconds = 0.030;

private:
    GrainShifter mono;
    PhaseVocoderShifter poly;
    juce::SmoothedValue<float> engage, semis, wet, polyMix, park;
    std::array<std::vector<float>, kMaxChannels> dry, polyOut;
    std::vector<float> ratio;
    bool monoRunning = false, polyRunning = false, shiftSource = true;
    int maxBlock = 512;
};
} // namespace jb::dsp
