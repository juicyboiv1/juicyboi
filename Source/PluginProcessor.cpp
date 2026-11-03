#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace jb;

JuicyboiAudioProcessor::JuicyboiAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "juicyboi", params::createLayout())
{
    pInputTrim = apvts.getRawParameterValue (params::id::inputTrim);
    pOdEnable  = apvts.getRawParameterValue (params::id::odEnable);
    pOdDrive   = apvts.getRawParameterValue (params::id::odDrive);
    pOdTone    = apvts.getRawParameterValue (params::id::odTone);
    pOdLevel   = apvts.getRawParameterValue (params::id::odLevel);
    pChannel   = apvts.getRawParameterValue (params::id::channel);
    pGain      = apvts.getRawParameterValue (params::id::gain);
    pBass      = apvts.getRawParameterValue (params::id::bass);
    pMid       = apvts.getRawParameterValue (params::id::mid);
    pTreble    = apvts.getRawParameterValue (params::id::treble);
    pPresence  = apvts.getRawParameterValue (params::id::presence);
    pMaster    = apvts.getRawParameterValue (params::id::master);
    pBypass    = apvts.getRawParameterValue (params::id::bypass);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (params::id::bypass));

    voicingA = dsp::voicings::riffA();
    voicingB = dsp::voicings::riffB();
}

//==============================================================================
void JuicyboiAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxChunk = juce::jmax (1, samplesPerBlock);
    const auto numCh = dsp::kMaxChannels;

    oversampler.prepare (numCh, maxChunk);
    const auto osRate = sampleRate * oversampler.getFactor();

    od.prepare (osRate);
    riffA.prepare (osRate, voicingA);
    riffB.prepare (osRate, voicingB);
    tone.prepare (sampleRate);

    const auto latency = oversampler.getLatencySamples();
    setLatencySamples (latency);

    dryDelay.prepare ({ sampleRate, (juce::uint32) maxChunk, (juce::uint32) numCh });
    dryDelay.setMaximumDelayInSamples (latency + 1);
    dryDelay.setDelay ((float) latency);
    dryDelay.reset();
    dryBuffer.setSize (numCh, maxChunk, false, true, false);

    inputGain .reset (sampleRate, 0.02);
    masterGain.reset (sampleRate, 0.02);
    bypassMix .reset (sampleRate, 0.03);
    odMix     .reset (osRate, 0.015);
    channelMix.reset (osRate, 0.02);

    // Snap smoothers to current values so there is no fade-in on load
    inputGain .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInputTrim->load()));
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pMaster->load(), -60.0f));
    bypassMix .setCurrentAndTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
    odMix     .setCurrentAndTargetValue (pOdEnable->load() > 0.5f ? 1.0f : 0.0f);
    channelMix.setCurrentAndTargetValue (pChannel->load() > 0.5f ? 1.0f : 0.0f);

    lastChannel = -1;
    updateParameters();
}

void JuicyboiAudioProcessor::releaseResources()
{
    oversampler.reset();
}

bool JuicyboiAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    // mono->mono, mono->stereo (typical guitar DI), stereo->stereo
    if (in == juce::AudioChannelSet::mono())
        return true;

    return in == out;
}

//==============================================================================
void JuicyboiAudioProcessor::updateParameters()
{
    inputGain .setTargetValue (juce::Decibels::decibelsToGain (pInputTrim->load()));
    masterGain.setTargetValue (juce::Decibels::decibelsToGain (pMaster->load(), -60.0f));
    bypassMix .setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
    odMix     .setTargetValue (pOdEnable->load() > 0.5f ? 1.0f : 0.0f);

    const auto ch = pChannel->load() > 0.5f ? 1 : 0;
    channelMix.setTargetValue ((float) ch);

    // TODO: smooth OD level / tone-stack coefficient changes (currently per block)
    od.setParameters (pOdDrive->load() / 10.0f, pOdTone->load() / 10.0f,
                      juce::Decibels::decibelsToGain (pOdLevel->load()));

    const auto gain01 = pGain->load() / 10.0f;
    riffA.setGain (gain01);
    riffB.setGain (gain01);

    if (ch != lastChannel)
    {
        lastChannel = ch;
        tone.setVoicing (ch == 0 ? voicingA : voicingB);
    }

    tone.setKnobs (pBass->load(), pMid->load(), pTreble->load(), pPresence->load());
}

void JuicyboiAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numIn  = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    for (auto ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const auto numProc = juce::jmin (numIn, dsp::kMaxChannels, buffer.getNumChannels());
    if (numProc <= 0 || numSamples == 0)
        return;

    updateParameters();

    // Hosts may exceed the prepared block size: process in safe chunks.
    for (int start = 0; start < numSamples; start += maxChunk)
        processChunk (buffer, start, juce::jmin (maxChunk, numSamples - start), numProc);

    finaliseOutputs (buffer);
}

void JuicyboiAudioProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int len, int numProc)
{
    // 0) Latency-aligned dry copy for the bypass crossfade
    for (int ch = 0; ch < numProc; ++ch)
    {
        const auto* src = buffer.getReadPointer (ch, start);
        auto* dst = dryBuffer.getWritePointer (ch);
        for (int i = 0; i < len; ++i)
        {
            dryDelay.pushSample (ch, src[i]);
            dst[i] = dryDelay.popSample (ch);
        }
    }

    // 1) Input trim
    for (int i = 0; i < len; ++i)
    {
        const auto g = inputGain.getNextValue();
        for (int ch = 0; ch < numProc; ++ch)
            buffer.getWritePointer (ch, start)[i] *= g;
    }

    // 2) Nonlinear section at 4x: OD-style boost -> dual-channel preamp
    auto block = juce::dsp::AudioBlock<float> (buffer)
                     .getSubsetChannelBlock (0, (size_t) numProc)
                     .getSubBlock ((size_t) start, (size_t) len);

    oversampler.process (block, [this, numProc] (juce::dsp::AudioBlock<float>& up)
    {
        const auto n = (int) up.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            const auto odAmt = odMix.getNextValue();
            const auto chAmt = channelMix.getNextValue();

            for (int ch = 0; ch < numProc; ++ch)
            {
                auto* d = up.getChannelPointer ((size_t) ch);
                const auto x = d[i];

                // OD always runs (state stays warm) and is blended in, so
                // toggling it never clicks.
                const auto boosted = x + odAmt * (od.process (x, ch) - x);

                // Both channels run; switching is a short crossfade.
                const auto a = riffA.process (boosted, ch);
                const auto b = riffB.process (boosted, ch);
                d[i] = a + chAmt * (b - a);
            }
        }
    });

    // 3) Tone stack + presence, 4) master, 5) bypass crossfade
    for (int i = 0; i < len; ++i)
    {
        const auto m  = masterGain.getNextValue();
        const auto bp = bypassMix.getNextValue();

        for (int ch = 0; ch < numProc; ++ch)
        {
            auto* d = buffer.getWritePointer (ch, start);
            const auto wet = tone.process (d[i], ch) * m;
            const auto dry = dryBuffer.getReadPointer (ch)[i];
            d[i] = wet + bp * (dry - wet);
        }
    }
}

void JuicyboiAudioProcessor::finaliseOutputs (juce::AudioBuffer<float>& buffer)
{
    // Mono guitar in -> copy processed mono to remaining outputs
    if (getTotalNumInputChannels() == 1)
        for (int ch = 1; ch < getTotalNumOutputChannels(); ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, buffer.getNumSamples());
}

void JuicyboiAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Host-side bypass: pass dry audio delayed by our reported latency.
    juce::ScopedNoDenormals noDenormals;
    const auto numIn  = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();
    const auto numProc = juce::jmin (numIn, dsp::kMaxChannels, buffer.getNumChannels());

    for (auto ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    for (int ch = 0; ch < numProc; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            dryDelay.pushSample (ch, d[i]);
            d[i] = dryDelay.popSample (ch);
        }
    }

    finaliseOutputs (buffer);
}

//==============================================================================
void JuicyboiAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void JuicyboiAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* JuicyboiAudioProcessor::createEditor()
{
    return new JuicyboiAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JuicyboiAudioProcessor();
}
