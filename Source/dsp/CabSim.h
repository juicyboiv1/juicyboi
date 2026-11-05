#pragma once

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
        (after a short fade-out), so the preamp can feed an analog power amp. */
class CabSim
{
public:
    enum Slot { irA = 0, irB, numSlots };

    static juce::String slotName (int s)
    {
        static const char* n[] = { "IR A", "IR B" };
        return n[juce::jlimit (0, numSlots - 1, s)];
    }

    void prepare (double sampleRate, int maxBlock, int numChannels);
    void reset();

    /** Message thread. Returns false if the file couldn't be read. */
    bool loadImpulseResponse (int slot, const juce::File& file);
    void clearSlot (int slot);
    bool hasIR (int slot) const { return loaded[(size_t) slot].load(); }

    void setParameters (bool enabled, float blend01, float levelGain);

    /** In-place on a base-rate block. */
    void process (juce::dsp::AudioBlock<float> block);

    int getLatencySamples() const; // 0 with default (zero-latency) Convolution

private:
    std::array<juce::dsp::Convolution, numSlots> convs;
    std::array<std::atomic<bool>, numSlots> loaded {};
    juce::AudioBuffer<float> bufA, bufB, dry;
    juce::SmoothedValue<float> enableMix, blend, level;
    bool prepared = false;
};
} // namespace jb::dsp
