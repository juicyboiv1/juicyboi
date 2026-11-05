#include "CabSim.h"

namespace jb::dsp
{
void CabSim::prepare (double sampleRate, int maxBlock, int numChannels)
{
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, (juce::uint32) numChannels };
    for (auto& c : convs)
        c.prepare (spec);

    bufA.setSize (numChannels, maxBlock, false, true, false);
    bufB.setSize (numChannels, maxBlock, false, true, false);
    dry .setSize (numChannels, maxBlock, false, true, false);

    enableMix.reset (sampleRate, 0.02);
    blend    .reset (sampleRate, 0.03);
    level    .reset (sampleRate, 0.02);
    prepared = true;
}

void CabSim::reset()
{
    for (auto& c : convs)
        c.reset();
}

bool CabSim::loadImpulseResponse (int slot, const juce::File& file)
{
    if (slot < 0 || slot >= numSlots || ! file.existsAsFile())
        return false;

    // Mono or stereo IR; Stereo::yes uses both IR channels if present,
    // a mono IR is applied to every channel. Trim silence, normalise.
    convs[(size_t) slot].loadImpulseResponse (file,
                                              juce::dsp::Convolution::Stereo::yes,
                                              juce::dsp::Convolution::Trim::yes,
                                              0,
                                              juce::dsp::Convolution::Normalise::yes);
    loaded[(size_t) slot] = true;
    return true;
}

void CabSim::clearSlot (int slot)
{
    if (slot >= 0 && slot < numSlots)
        loaded[(size_t) slot] = false;
}

void CabSim::setParameters (bool enabled, float blend01, float levelGain)
{
    enableMix.setTargetValue (enabled ? 1.0f : 0.0f);
    blend.setTargetValue (juce::jlimit (0.0f, 1.0f, blend01));
    level.setTargetValue (levelGain);
}

int CabSim::getLatencySamples() const
{
    int l = 0;
    for (auto& c : convs)
        l = juce::jmax (l, c.getLatency());
    return l;
}

void CabSim::process (juce::dsp::AudioBlock<float> block)
{
    const auto len = (int) block.getNumSamples();
    const auto nch = (int) block.getNumChannels();

    // Hard bypass: skip convolution entirely once faded out.
    if (! prepared || (! enableMix.isSmoothing() && enableMix.getTargetValue() < 0.5f))
    {
        enableMix.setCurrentAndTargetValue (enableMix.getTargetValue());
        return;
    }

    constexpr int slotA = irA, slotB = irB;
    const bool hasA = hasIR (slotA), hasB = hasIR (slotB);

    for (int ch = 0; ch < nch; ++ch)
        dry.copyFrom (ch, 0, block.getChannelPointer ((size_t) ch), len);

    auto run = [&] (int slot, juce::AudioBuffer<float>& buf)
    {
        for (int ch = 0; ch < nch; ++ch)
            buf.copyFrom (ch, 0, dry, ch, 0, len);
        auto b = juce::dsp::AudioBlock<float> (buf).getSubsetChannelBlock (0, (size_t) nch).getSubBlock (0, (size_t) len);
        convs[(size_t) slot].process (juce::dsp::ProcessContextReplacing<float> (b));
    };

    if (hasA) run (slotA, bufA);
    if (hasB) run (slotB, bufB);

    for (int i = 0; i < len; ++i)
    {
        const auto en = enableMix.getNextValue();
        const auto bl = blend.getNextValue();
        const auto lv = level.getNextValue();

        float wA = hasA ? 1.0f - bl : 0.0f;
        float wB = hasB ? bl : 0.0f;
        if (hasA && ! hasB) wA = 1.0f;   // renormalise toward the loaded IR
        if (hasB && ! hasA) wB = 1.0f;

        for (int ch = 0; ch < nch; ++ch)
        {
            const auto x = dry.getSample (ch, i);
            float wet = x; // neither IR loaded -> passthrough
            if (hasA || hasB)
                wet = ((hasA ? wA * bufA.getSample (ch, i) : 0.0f)
                     + (hasB ? wB * bufB.getSample (ch, i) : 0.0f)) * lv;
            block.getChannelPointer ((size_t) ch)[i] = x + en * (wet - x);
        }
    }
}
} // namespace jb::dsp
