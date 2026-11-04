#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace jb::dsp
{
/** Key/sidechain noise gate (original DSP).

    Follows the classic "key + VCA" gate topology used by high-gain guitar
    gates: the *detector* listens to the clean guitar (post input trim, before
    any gain stage), so its envelope tracks picking dynamics instead of the
    hiss floor of the distorted signal. The *VCA* sits late in the wet path
    (after tone + cab, before Master), so whatever noise the preamp/cab adds is
    muted along with the note tail.

    Detector: DC-blocked mono key -> |x| -> peak follower (fast attack, short
    release) -> threshold with hysteresis (closes a few dB below where it opens).
    Gain law state machine:  Open -> Hold -> Closing -> Closed
      Open    : gain -> 1 with a short fixed attack (no click on re-open)
      Hold    : key fell below close threshold; gain stays 1 for `hold` ms
      Closing : gain falls 60 dB over `release` ms (exponential = linear in dB)
      Closed  : gain = 0 (floor below -80 dB snaps to 0)
    Disabled: true bypass (after a short 10 ms fade) — no multiply, no state change. */
class NoiseGate
{
public:
    void prepare (double sampleRate);
    void reset();

    void setParameters (bool enabled, float thresholdDb, float holdMs, float releaseMs);

    /** Base-rate, in place. `key` is mono, `len` samples, time-aligned with
        (or slightly ahead of) `chans`. */
    void process (const float* key, float* const* chans, int numChannels, int len);

    /** 1 = fully open, 0 = closed. Thread-safe; read by the editor LED. */
    float getOpenAmount() const noexcept { return openAmount.load (std::memory_order_relaxed); }
    bool  isActive() const noexcept      { return active.load (std::memory_order_relaxed); }

    static constexpr float attackMs        = 0.5f;  // VCA open ramp (fixed)
    static constexpr float detReleaseMs    = 8.0f;  // envelope follower release
    static constexpr float hysteresisDb    = 4.0f;  // close threshold below open
    static constexpr float closedFloorGain = 1.0e-4f; // -80 dB -> snap to 0

private:
    enum class State { Open, Hold, Closing, Closed };

    double sr = 48000.0;
    State state = State::Closed;
    float env = 0.0f, gain = 0.0f;
    float dcX1 = 0.0f, dcY1 = 0.0f, dcR = 0.995f;
    float envAtt = 0.0f, envRel = 0.0f, gainAtt = 0.0f, gainRel = 0.0f;
    float openThr = 0.001f, closeThr = 0.0006f;
    int holdSamples = 0, holdCounter = 0;
    juce::SmoothedValue<float> enableMix;
    std::atomic<float> openAmount { 1.0f };
    std::atomic<bool> active { false };
};
} // namespace jb::dsp
