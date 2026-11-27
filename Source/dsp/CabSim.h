#pragma once

#include "Filters.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace jb::dsp
{
/** Blendable two-IR cabinet stage (IR A / IR B).

    Two IR slots, each its own zero-latency juce::dsp::Convolution.
    Blend 0 = 100% IR A, 1 = 100% IR B (linear crossfade of the wet signals).

    Empty-slot behaviour:
      - one slot empty -> 100% of the loaded IR
      - neither IR loaded               -> unity passthrough (no mute surprise)
      - cab disabled                    -> hard bypass, convolution not run at all
        (after a short fade-out), so the preamp can feed an analog power amp.

    Rev3.2.2: a 12 dB/oct Butterworth HPF and LPF sit on the wet path only,
    after the IR A/B mix and Cab Level, before the enable dry/wet crossfade
    (the usual guitar-IR-loader convention). They shape the cabinet, not the
    dry preamp during the on/off fade. Cab off skips them entirely. Defaults
    (HPF 20 Hz, LPF 20 kHz) are true bypass, so older sessions are unchanged.
    With the cab on and no IR loaded the filters still apply to the
    passthrough wet signal. */
class CabSim
{
public:
    enum Slot { irA = 0, irB, numSlots };

    static constexpr float hpfMinHz = 20.0f;
    static constexpr float hpfMaxHz = 300.0f;
    static constexpr float lpfMinHz = 2000.0f;
    static constexpr float lpfMaxHz = 20000.0f;
    /** 2nd-order Butterworth Q. */
    static constexpr float butterworthQ = 0.7071067811865476f;

    static juce::String slotName (int s)
    {
        static const char* n[] = { "IR A", "IR B" };
        return n[juce::jlimit (0, numSlots - 1, s)];
    }

    /** Endpoint bypass: a knob parked at (or extremely close to) the range
        end is treated as off, so default sessions stay bit-identical. */
    static bool hpfEngaged (float hz) noexcept { return hz > hpfMinHz * 1.02f; }
    static bool lpfEngaged (float hz) noexcept { return hz < lpfMaxHz / 1.02f; }

    void prepare (double sampleRate, int maxBlock, int numChannels);
    void reset();

    /** Message thread. Returns false if the file couldn't be read. */
    bool loadImpulseResponse (int slot, const juce::File& file);
    void clearSlot (int slot);
    bool hasIR (int slot) const { return loaded[(size_t) slot].load(); }

    void setParameters (bool enabled, float blend01, float levelGain,
                        float hpfHz = hpfMinHz, float lpfHz = lpfMaxHz);

    /** In-place on a base-rate block. */
    void process (juce::dsp::AudioBlock<float> block);

    int getLatencySamples() const; // 0 with default (zero-latency) Convolution

    /** Both filters settled in endpoint bypass (tests / CPU). */
    bool filtersBypassed() const noexcept;

private:
    void snapSmoothers() noexcept;
    void updateCoeffs (float hHz, float lHz) noexcept;

    std::array<juce::dsp::Convolution, numSlots> convs;
    std::array<std::atomic<bool>, numSlots> loaded {};
    juce::AudioBuffer<float> bufA, bufB, dry;
    juce::SmoothedValue<float> enableMix, blend, level;
    // Cutoffs are smoothed in log-Hz to keep knob moves even; mixes fade each
    // filter in and out of true bypass so engaging one does not click.
    juce::SmoothedValue<float> hpfLog, lpfLog, hpfMix, lpfMix;
    Biquad hpf, lpf;
    double sampleRate = 44100.0;
    float lastHpfHz = -1.0f, lastLpfHz = -1.0f;
    bool prepared = false;
};
} // namespace jb::dsp
