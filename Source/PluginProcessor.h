#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "dsp/Od808Stage.h"
#include "dsp/PreampCascade.h"
#include "dsp/ToneStack.h"
#include "dsp/OversampledNonlinear.h"
#include "dsp/CabSim.h"

/** juicyboi — heavy-riff preamp.
    Signal chain:
      Input trim -> [oversampled: OD-style boost (blend on/off) ->
      dual-channel cascaded preamp (Red Ch1 / Orange Ch2 crossfade)] ->
      tone stack + presence -> cab IR sim (optional, hard bypass) -> master -> (bypass crossfade vs delayed dry). */
class JuicyboiAudioProcessor final : public juce::AudioProcessor
{
public:
    JuicyboiAudioProcessor();
    ~JuicyboiAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "juicyboi"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

    //== Cab IR slots (message thread) =========================================
    bool loadCabIR (int slot, const juce::File& file);
    void clearCabIR (int slot);
    juce::File getCabIRFile (int slot) const;
    bool hasCabIR (int slot) const { return cab.hasIR (slot); }

private:
    void updateParameters();
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int len, int numProcChannels);
    void finaliseOutputs (juce::AudioBuffer<float>& buffer);

    // Cached parameter handles
    std::atomic<float>* pInputTrim = nullptr;
    std::atomic<float>* pOdEnable  = nullptr;
    std::atomic<float>* pOdDrive   = nullptr;
    std::atomic<float>* pOdTone    = nullptr;
    std::atomic<float>* pOdLevel   = nullptr;
    std::atomic<float>* pChannel   = nullptr;
    std::atomic<float>* pGain      = nullptr;
    std::atomic<float>* pBass      = nullptr;
    std::atomic<float>* pMid       = nullptr;
    std::atomic<float>* pTreble    = nullptr;
    std::atomic<float>* pPresence  = nullptr;
    std::atomic<float>* pMaster    = nullptr;
    std::atomic<float>* pBypass    = nullptr;
    std::atomic<float>* pCabEnable = nullptr;
    std::atomic<float>* pCab57Axis = nullptr;
    std::atomic<float>* pCab421Axis = nullptr;
    std::atomic<float>* pCabBlend  = nullptr;
    std::atomic<float>* pCabLevel  = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;
    int baseLatency = 0;

    // DSP
    jb::dsp::OversampledNonlinear oversampler;
    jb::dsp::Od808Stage od;
    jb::dsp::PreampCascade riffA, riffB;
    jb::dsp::ToneStack tone;
    jb::dsp::CabSim cab;
    std::array<juce::File, jb::dsp::CabSim::numSlots> cabFiles;
    jb::dsp::ChannelVoicing voicingA, voicingB;

    juce::SmoothedValue<float> inputGain, masterGain, bypassMix; // base rate
    juce::SmoothedValue<float> odMix, channelMix;                // oversampled rate

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 8192 };
    juce::AudioBuffer<float> dryBuffer;

    int maxChunk = 512;
    int lastChannel = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuicyboiAudioProcessor)
};
