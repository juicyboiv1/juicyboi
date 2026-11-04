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
    pCabEnable  = apvts.getRawParameterValue (params::id::cabEnable);
    pCab57Axis  = apvts.getRawParameterValue (params::id::cabSm57Axis);
    pCab421Axis = apvts.getRawParameterValue (params::id::cabMd421Axis);
    pCabBlend   = apvts.getRawParameterValue (params::id::cabMicBlend);
    pCabLevel   = apvts.getRawParameterValue (params::id::cabLevel);
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
    cab.prepare (sampleRate, maxChunk, numCh);

    // Cab uses zero-latency convolution; its latency (if any) is added anyway.
    baseLatency = oversampler.getLatencySamples();
    const auto latency = baseLatency + cab.getLatencySamples();
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

    cab.setParameters (pCabEnable->load() > 0.5f, pCab57Axis->load() > 0.5f, pCab421Axis->load() > 0.5f,
                       pCabBlend->load(), juce::Decibels::decibelsToGain (pCabLevel->load()));
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

    // 3) Tone stack + presence
    for (int ch = 0; ch < numProc; ++ch)
    {
        auto* d = buffer.getWritePointer (ch, start);
        for (int i = 0; i < len; ++i)
            d[i] = tone.process (d[i], ch);
    }

    // 4) Cab IR sim (no-op when Cab is off)
    cab.process (juce::dsp::AudioBlock<float> (buffer)
                     .getSubsetChannelBlock (0, (size_t) numProc)
                     .getSubBlock ((size_t) start, (size_t) len));

    // 5) master, 6) bypass crossfade
    for (int i = 0; i < len; ++i)
    {
        const auto m  = masterGain.getNextValue();
        const auto bp = bypassMix.getNextValue();

        for (int ch = 0; ch < numProc; ++ch)
        {
            auto* d = buffer.getWritePointer (ch, start);
            const auto wet = d[i] * m;
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
static const char* irKeys[] = { "ir_sm57_on", "ir_sm57_off", "ir_md421_on", "ir_md421_off" };

bool JuicyboiAudioProcessor::loadCabIR (int slot, const juce::File& file)
{
    if (slot < 0 || slot >= dsp::CabSim::numSlots || ! cab.loadImpulseResponse (slot, file))
        return false;
    cabFiles[(size_t) slot] = file;
    return true;
}

void JuicyboiAudioProcessor::clearCabIR (int slot)
{
    if (slot < 0 || slot >= dsp::CabSim::numSlots) return;
    cab.clearSlot (slot);
    cabFiles[(size_t) slot] = juce::File();
}

juce::File JuicyboiAudioProcessor::getCabIRFile (int slot) const
{
    return (slot >= 0 && slot < dsp::CabSim::numSlots) ? cabFiles[(size_t) slot] : juce::File();
}

void JuicyboiAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    for (int s = 0; s < dsp::CabSim::numSlots; ++s)
        state.setProperty (irKeys[s], cabFiles[(size_t) s].getFullPathName(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void JuicyboiAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);

            for (int s = 0; s < dsp::CabSim::numSlots; ++s)
            {
                const auto path = tree.getProperty (irKeys[s]).toString();
                if (path.isNotEmpty() && juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile())
                    loadCabIR (s, juce::File (path));
                else
                    clearCabIR (s);
            }
        }
}

juce::AudioProcessorEditor* JuicyboiAudioProcessor::createEditor()
{
    return new JuicyboiAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JuicyboiAudioProcessor();
}
