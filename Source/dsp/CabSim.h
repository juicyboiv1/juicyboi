#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace jb::dsp
{
/** Blendable dual-mic cabinet IR stage (SM57 = mic A, MD421 = mic B).

    Four IR slots (57 on/off-axis, 421 on/off-axis), each its own zero-latency
    juce::dsp::Convolution so axis switching is instant (no reload).

    Empty-slot behaviour:
      - selected IR missing for a mic -> that mic contributes 0 and the blend is
        renormalised to the mic that has an IR (100% of it)
      - neither selected IR loaded      -> unity passthrough (no mute surprise)
      - cab disabled                    -> hard bypass, convolution not run at all
        (after a short fade-out), so the preamp can feed an analog power amp. */
class CabSim
{
public:
    enum Slot { sm57On = 0, sm57Off, md421On, md421Off, numSlots };

    static juce::String slotName (int s)
    {
        static const char* n[] = { "SM57 On-axis", "SM57 Off-axis", "MD421 On-axis", "MD421 Off-axis" };
        return n[juce::jlimit (0, numSlots - 1, s)];
    }

    void prepare (double sampleRate, int maxBlock, int numChannels);
    void reset();

    /** Message thread. Returns false if the file couldn't be read. */
    bool loadImpulseResponse (int slot, const juce::File& file);
    void clearSlot (int slot);
    bool hasIR (int slot) const { return loaded[(size_t) slot].load(); }

    void setParameters (bool enabled, bool sm57OffAxis, bool md421OffAxis, float blend01, float levelGain);

    /** In-place on a base-rate block. */
    void process (juce::dsp::AudioBlock<float> block);

    int getLatencySamples() const; // 0 with default (zero-latency) Convolution

private:
    std::array<juce::dsp::Convolution, numSlots> convs;
    std::array<std::atomic<bool>, numSlots> loaded {};
    juce::AudioBuffer<float> bufA, bufB, dry;
    juce::SmoothedValue<float> enableMix, blend, level;
    bool a57Off = false, b421Off = false;
    bool prepared = false;
};
} // namespace jb::dsp
