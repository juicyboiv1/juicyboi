#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "dsp/Od808Stage.h"
#include "dsp/PreampCascade.h"
#include "dsp/ToneStack.h"
#include "dsp/GraphicEq.h"
#include "dsp/OversampledNonlinear.h"
#include "dsp/CabSim.h"
#include "dsp/NoiseGate.h"

/** juicyboi — heavy-riff preamp.
    Signal chain:
      Input trim -> [oversampled: OD-style boost (blend on/off) ->
      dual-channel cascaded preamp (Red Ch1 v1|v2 / Orange Ch2 crossfades)] ->
      tone stack + presence -> graphic EQ (optional, true bypass) -> cab IR sim (optional, hard bypass) -> gate VCA (optional, true bypass;
      keyed from the dry post-trim input) -> master -> (bypass crossfade vs delayed dry). */
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

    //== Gate status (any thread) ===============================================
    /** 1 = open, 0 = closed (1 when the gate is bypassed). */
    float getGateOpenAmount() const noexcept { return gate.getOpenAmount(); }
    bool  isGateActive() const noexcept      { return gate.isActive(); }

    /** Graphic EQ is processing (on, or fading out). */
    bool  isEqActive() const noexcept        { return graphicEq.isActive(); }

private:
    void updateParameters();
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int len, int numProcChannels);
    void finaliseOutputs (juce::AudioBuffer<float>& buffer, bool monoIn);
    bool foldStandaloneInputsToMono (juce::AudioBuffer<float>& buffer) const;

    // Cached parameter handles
    std::atomic<float>* pInputTrim = nullptr;
    std::atomic<float>* pOdEnable  = nullptr;
    std::atomic<float>* pOdDrive   = nullptr;
    std::atomic<float>* pOdTone    = nullptr;
    std::atomic<float>* pOdLevel   = nullptr;
    std::atomic<float>* pChannel   = nullptr;
    std::atomic<float>* pRedVoice  = nullptr;
    std::atomic<float>* pGain      = nullptr;
    std::atomic<float>* pBass      = nullptr;
    std::atomic<float>* pMid       = nullptr;
    std::atomic<float>* pTreble    = nullptr;
    std::atomic<float>* pPresence  = nullptr;
    std::atomic<float>* pMaster    = nullptr;
    std::atomic<float>* pBypass    = nullptr;
    std::atomic<float>* pEqEnable = nullptr;
    std::array<std::atomic<float>*, jb::dsp::GraphicEq::numBands> pEqBands {};
    std::atomic<float>* pEqLevel  = nullptr;
    std::atomic<float>* pCabEnable = nullptr;
    std::atomic<float>* pCabBlend  = nullptr;
    std::atomic<float>* pCabLevel  = nullptr;
    std::atomic<float>* pGateEnable  = nullptr;
    std::atomic<float>* pGateThresh  = nullptr;
    std::atomic<float>* pGateHold    = nullptr;
    std::atomic<float>* pGateRelease = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;
    int baseLatency = 0;

    // DSP
    jb::dsp::OversampledNonlinear oversampler;
    jb::dsp::Od808Stage od;
    jb::dsp::PreampCascade riffA, riffAv1, riffB; // Red v2, Red v1, Orange
    jb::dsp::ToneStack tone;
    jb::dsp::GraphicEq graphicEq;
    jb::dsp::CabSim cab;
    jb::dsp::NoiseGate gate;
    std::array<juce::File, jb::dsp::CabSim::numSlots> cabFiles;
    jb::dsp::ChannelVoicing voicingA, voicingAv1, voicingB;

    juce::SmoothedValue<float> inputGain, masterGain, bypassMix; // base rate
    juce::SmoothedValue<float> odMix, channelMix, redVoiceMix;   // oversampled rate

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 8192 };
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> keyBuffer; // mono gate key: post-trim, pre-gain

    int maxChunk = 512;
    int lastChannel = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuicyboiAudioProcessor)
};
