#pragma once

#include "PitchDetector.h"
#include <array>
#include <atomic>
#include <mutex>
#include <vector>

namespace jb::dsp
{
/** Rev3.2 tuner pedal (docs/tuner.md): TU-3-style mute + chromatic display.

    Audio thread: push() feeds the (decimated) input into a lock-free ring and
    applyMute() fades the output to silence while the tuner is on (10 ms, no
    clicks). With the tuner off and the fade settled, applyMute() returns
    without touching the buffer, so the signal is bit-identical to Rev3.1.

    Analysis thread: analyse() takes the latest frame, runs PitchDetector and
    smooths the result for the display (median of 5 + one-pole in cents, note
    changes confirmed over two frames, 400 ms hold after the note dies). */
class Tuner
{
public:
    void prepare (double sampleRate, int maxBlock);

    // ---- audio thread ----
    void setEnabled (bool on) noexcept { enabled.store (on, std::memory_order_relaxed); muteTarget = on ? 0.0f : 1.0f; }
    bool isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }
    /** True while the analysis input is needed (tuner on or fading). */
    bool wantsInput() const noexcept { return isEnabled() || muteGain < 1.0f; }
    void push (const float* mono, int n) noexcept;
    void applyMute (float* const* chans, int numCh, int n) noexcept;
    float getMuteGain() const noexcept { return muteGain; }

    // ---- analysis (any non-audio thread) ----
    void setReference (double aHz) noexcept { refHz.store (aHz, std::memory_order_relaxed); }
    double getReference() const noexcept { return refHz.load (std::memory_order_relaxed); }
    /** Runs one detection on the newest frame; elapsedSeconds = time since the previous call. */
    void analyse (double elapsedSeconds = 0.02);
    void resetDisplay();

    struct Display
    {
        bool hasPitch = false;
        int midiNote = 0;       ///< nearest note to the smoothed pitch
        double cents = 0.0;     ///< smoothed deviation from that note (-50..+50)
        double hz = 0.0;        ///< smoothed frequency
        double rawHz = 0.0;     ///< last raw detection (0 = none)
    };
    Display getDisplay() const noexcept;

    static const char* noteName (int midiNote) noexcept;   ///< "C", "C#", ... "B"
    static int octave (int midiNote) noexcept { return midiNote / 12 - 1; }

    double getAnalysisRate() const noexcept { return analysisRate; }

private:
    // audio-side
    std::atomic<bool> enabled { false };
    float muteGain = 1.0f, muteTarget = 1.0f, muteStep = 0.0f;
    int decim = 1, decimPhase = 0;
    struct Biquad { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
                    double run (double x) noexcept { const auto y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
    std::array<Biquad, 2> aa {};
    std::vector<std::atomic<float>> ring;
    int ringMask = 0;
    std::atomic<int> writePos { 0 };

    // analysis-side
    std::mutex analysisLock;
    PitchDetector detector;
    std::vector<float> frame;
    double analysisRate = 48000.0;
    std::atomic<double> refHz { 440.0 };
    std::array<double, 5> recent {};
    int recentCount = 0;
    double smoothCents = 0.0;     // absolute pitch in cents re A4 = refHz
    bool smoothValid = false;
    int pendingNote = -1000, pendingCount = 0;
    double sinceValid = 1.0e9;

    std::atomic<bool> dHas { false };
    std::atomic<int> dNote { 69 };
    std::atomic<double> dCents { 0.0 }, dHz { 0.0 }, dRaw { 0.0 };
};
} // namespace jb::dsp
