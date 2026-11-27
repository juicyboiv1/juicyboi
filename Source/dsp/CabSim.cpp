#include "CabSim.h"

namespace jb::dsp
{
void CabSim::prepare (double newSampleRate, int maxBlock, int numChannels)
{
    sampleRate = newSampleRate;
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, (juce::uint32) numChannels };
    for (auto& c : convs)
        c.prepare (spec);

    bufA.setSize (numChannels, maxBlock, false, true, false);
    bufB.setSize (numChannels, maxBlock, false, true, false);
    dry .setSize (numChannels, maxBlock, false, true, false);

    enableMix.reset (sampleRate, 0.02);
    blend    .reset (sampleRate, 0.03);
    level    .reset (sampleRate, 0.02);
    hpfLog   .reset (sampleRate, 0.03);
    lpfLog   .reset (sampleRate, 0.03);
    hpfMix   .reset (sampleRate, 0.015);
    lpfMix   .reset (sampleRate, 0.015);
    hpf.reset();
    lpf.reset();
    lastHpfHz = lastLpfHz = -1.0f;
    prepared = true;
}

void CabSim::reset()
{
    for (auto& c : convs)
        c.reset();
    hpf.reset();
    lpf.reset();
    lastHpfHz = lastLpfHz = -1.0f;
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

void CabSim::setParameters (bool enabled, float blend01, float levelGain, float hpfHz, float lpfHz)
{
    enableMix.setTargetValue (enabled ? 1.0f : 0.0f);
    blend.setTargetValue (juce::jlimit (0.0f, 1.0f, blend01));
    level.setTargetValue (levelGain);

    const auto h = juce::jlimit (hpfMinHz, hpfMaxHz, hpfHz);
    const auto l = juce::jlimit (lpfMinHz, lpfMaxHz, lpfHz);
    hpfLog.setTargetValue (std::log (h));
    lpfLog.setTargetValue (std::log (l));
    hpfMix.setTargetValue (hpfEngaged (h) ? 1.0f : 0.0f);
    lpfMix.setTargetValue (lpfEngaged (l) ? 1.0f : 0.0f);
}

int CabSim::getLatencySamples() const
{
    int lat = 0;
    for (auto& c : convs)
        lat = juce::jmax (lat, c.getLatency());
    return lat;
}

bool CabSim::filtersBypassed() const noexcept
{
    return ! hpfMix.isSmoothing() && hpfMix.getTargetValue() < 0.5f
        && ! lpfMix.isSmoothing() && lpfMix.getTargetValue() < 0.5f;
}

void CabSim::snapSmoothers() noexcept
{
    enableMix.setCurrentAndTargetValue (enableMix.getTargetValue());
    blend    .setCurrentAndTargetValue (blend.getTargetValue());
    level    .setCurrentAndTargetValue (level.getTargetValue());
    hpfLog   .setCurrentAndTargetValue (hpfLog.getTargetValue());
    lpfLog   .setCurrentAndTargetValue (lpfLog.getTargetValue());
    hpfMix   .setCurrentAndTargetValue (hpfMix.getTargetValue());
    lpfMix   .setCurrentAndTargetValue (lpfMix.getTargetValue());
}

void CabSim::updateCoeffs (float hHz, float lHz) noexcept
{
    if (differs (hHz, lastHpfHz))
    {
        hpf.setCoeffs (BiquadCoeffs::highPass (sampleRate, (double) hHz, (double) butterworthQ));
        lastHpfHz = hHz;
    }
    if (differs (lHz, lastLpfHz))
    {
        lpf.setCoeffs (BiquadCoeffs::lowPass (sampleRate, (double) lHz, (double) butterworthQ));
        lastLpfHz = lHz;
    }
}

void CabSim::process (juce::dsp::AudioBlock<float> block)
{
    const auto len = juce::jmin ((int) block.getNumSamples(), dry.getNumSamples());
    const auto nch = juce::jmin ((int) block.getNumChannels(), kMaxChannels);

    // Hard bypass: skip convolution and the cab filters entirely once faded out.
    if (! prepared || (! enableMix.isSmoothing() && enableMix.getTargetValue() < 0.5f))
    {
        snapSmoothers();
        hpf.reset();
        lpf.reset();
        lastHpfHz = lastLpfHz = -1.0f;
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
        const auto hHz = std::exp (hpfLog.getNextValue());
        const auto lHz = std::exp (lpfLog.getNextValue());
        const auto hMix = hpfMix.getNextValue();
        const auto lMix = lpfMix.getNextValue();

        if (hMix > 0.0f || lMix > 0.0f)
            updateCoeffs (hHz, lHz);

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

            // Post-IR HPF then LPF on the wet path only. Mix = 0 is a true
            // bypass of that filter (bit-identical to leaving wet alone).
            if (hMix > 0.0f)
            {
                const auto f = hpf.process (wet, ch);
                wet += hMix * (f - wet);
            }
            if (lMix > 0.0f)
            {
                const auto f = lpf.process (wet, ch);
                wet += lMix * (f - wet);
            }

            block.getChannelPointer ((size_t) ch)[i] = x + en * (wet - x);
        }
    }
}
} // namespace jb::dsp
