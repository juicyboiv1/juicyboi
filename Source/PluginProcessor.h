#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "dsp/Od808Stage.h"
#include "dsp/Sd1Stage.h"
#include "dsp/RecoveryStage.h"
#include "dsp/SpeakerLoad.h"
#include "dsp/PreampCascade.h"
#include "dsp/Lead5150.h"
#include "dsp/Amp5150.h"
#include "dsp/LeadPresence.h"
#include "dsp/LeadResonance.h"
#include "dsp/ToneStack.h"
#include "dsp/GraphicEq.h"
#include "dsp/OversampledNonlinear.h"
#include "dsp/CabSim.h"
#include "dsp/NoiseGate.h"
#include "dsp/Tuner.h"
#include "dsp/PitchShifter.h"

/** juicyboi — heavy-riff preamp. Two amps, each with its OWN full settings set
    (Rev2.4, docs/per-amp-state.md); `preamp_model` picks the one you hear:
      Tuner (Rev3.2, shared; listens to the input, mutes the output while on)
      Shared input calibration (input_trim) then, per amp:
      PITCH (Rev3.3, docs/pitch-shift.md; base rate, true bypass when off) ->
      Rectifier: [4x: Boost -> Red v1|v2 / Orange cascade -> TMB ->
                 recovery -> (Red v2) 4x12 load -> GEQ-7] -> Cab (IR + HPF/LPF) -> Gate -> Master
      5150:      [4x: Boost -> Rhythm|Lead cascade (Pre Gain networks)
                 -> shared TMB -> Post Gain -> power section -> GEQ-7] ->
                 Presence/Resonance + 4x12 load -> Cab (IR + HPF/LPF) -> Gate   (no Master)
    Preamp Out (global `preamp_out`) skips cab IR, Red v2 / 5150 speaker-load
    modelling, and 5150 Presence/Resonance (power-amp NFB). Preamp stages,
    tone stacks, boosts, post EQ, gate, Rectifier recovery, 5150 Post Gain /
    Power5150 and the Rectifier Master stay. A 25 ms crossfade makes the
    switch click-free. The two amp outputs crossfade (30 ms) after their
    gates, then the global bypass crossfade against the latency-aligned dry
    input. */
class JuicyboiAudioProcessor final : public juce::AudioProcessor
{
public:
    JuicyboiAudioProcessor();
    ~JuicyboiAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
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

    /** Root state property: 4 = per-amp settings (Rev2.4; input_trim is the
        shared input calibration); 3 = Input/Master/Boost Level/Cab Level are
        0–10 knobs; 2 = only Input/Master; absent = 1 = all four in dB. */
    static constexpr const char* stateVersionKey = "jb_state_version";

    //== Cab IR slots (message thread) =========================================
    /** Per amp (0 = Rectifier, 1 = 5150): each amp has its own cab and IR slots.
        The two-argument forms address the Rectifier (pre-Rev2.4 API). */
    bool loadCabIR (int amp, int slot, const juce::File& file);
    void clearCabIR (int amp, int slot);
    juce::File getCabIRFile (int amp, int slot) const;
    bool hasCabIR (int amp, int slot) const { return cabFor (amp).hasIR (slot); }
    bool loadCabIR (int slot, const juce::File& file) { return loadCabIR (0, slot, file); }
    void clearCabIR (int slot)                         { clearCabIR (0, slot); }
    juce::File getCabIRFile (int slot) const           { return getCabIRFile (0, slot); }
    bool hasCabIR (int slot) const                     { return hasCabIR (0, slot); }

    //== Gate status (any thread) ===============================================
    /** 1 = open, 0 = closed (1 when the gate is bypassed). */
    float getGateOpenAmount (int amp = 0) const noexcept { return (amp == 0 ? gate : gate5).getOpenAmount(); }
    bool  isGateActive (int amp = 0) const noexcept      { return (amp == 0 ? gate : gate5).isActive(); }

    /** Graphic EQ is processing (on, or fading out). */
    bool  isEqActive (int amp = 0) const noexcept        { return (amp == 0 ? graphicEq : leadGraphicEq).isActive(); }

    /** Rev3.2 tuner pedal (shared by both amps). The editor reads its display;
        a background thread runs the pitch detection while it is on. */
    jb::dsp::Tuner& getTuner() noexcept { return tuner; }
    /** Tests: stop the background analysis thread (then call getTuner().analyse() directly). */
    void stopTunerThread();

    /** Rev3.3 PITCH pedal of amp 0 (Rectifier) or 1 (5150). Its latency is
        not reported to the host (docs/pitch-shift.md). */
    const jb::dsp::PitchPedal& getPitchPedal (int amp) const noexcept { return pitchPedals[(size_t) (amp == 0 ? 0 : 1)]; }

    /** The amp whose settings are shown/heard: 0 = Rectifier, 1 = 5150. */
    int activeAmp() const noexcept { return pPreampModel->load() > 0.5f ? 1 : 0; }

    /** 5150 tunables (docs/5150.md). Tests/tools may replace them before prepareToPlay. */
    jb::dsp::Voicing5150 voicing5150 = jb::dsp::defaultVoicing5150();

    //== Editor-only view state (not saved in the session; no effect on audio) ==
    bool editorDrawerOpen = true;
    int  editorWidth = 1100;

    /** Rev2.4 v4 migration: old Master (0–10) -> 5150 Post knob with the same
        small-signal level change (Master 6.5 = Post 5). */
    static float leadPostForMaster (float masterKnob);

    /** Preamp Out path gain (linear). 1 = the preamp, post-EQ level with no
        pad, which is already a sensible FX-return line level at default knobs
        (documented in docs/per-amp-state.md and PreampOutTest). */
    static constexpr float preampOutGain = 1.0f;
    /** Click-free Full Amp <-> Preamp Out fade. */
    static constexpr float preampOutFadeSeconds = 0.025f;

    /** Rev3 test hook: run every solved cascade every sample (Rev2.x "always
        warm" behaviour) so switching tests can compare against independently
        warmed endpoint processors. Off in the plugin: only audible cascades run. */
    bool keepAllCascadesWarm = false;

private:
    void migrateToPerAmp (juce::ValueTree& tree);
    void updateParameters();
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int len, int numProcChannels);
    void finaliseOutputs (juce::AudioBuffer<float>& buffer, bool monoIn);
    bool foldStandaloneInputsToMono (juce::AudioBuffer<float>& buffer) const;

    // Cached parameter handles
    std::atomic<float>* pInputTrim = nullptr;
    std::atomic<float>* pOdEnable  = nullptr;
    std::atomic<float>* pOdType    = nullptr;
    std::atomic<float>* pOdDrive   = nullptr;
    std::atomic<float>* pOdTone    = nullptr;
    std::atomic<float>* pOdLevel   = nullptr;
    std::atomic<float>* pPreampModel = nullptr;
    std::atomic<float>* pLeadPresence = nullptr;
    std::atomic<float>* pLeadResonance = nullptr;
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
    std::atomic<float>* pCabHpf    = nullptr; // Rev3.2.2, appended
    std::atomic<float>* pCabLpf    = nullptr;
    std::atomic<float>* p5CabHpf   = nullptr;
    std::atomic<float>* p5CabLpf   = nullptr;
    std::atomic<float>* pGateEnable  = nullptr;
    std::atomic<float>* pGateThresh  = nullptr;
    std::atomic<float>* pGateHold    = nullptr;
    std::atomic<float>* pGateRelease = nullptr;
    // Rev2.4: the 5150's own per-amp slots and faceplate
    std::array<std::atomic<float>*, jb::params::numAmpSlots> p5150 {};
    std::atomic<float>* p5Channel = nullptr;
    std::atomic<float>* p5Bright = nullptr;
    std::atomic<float>* p5Crunch = nullptr;
    std::atomic<float>* p5RhythmPre = nullptr;
    std::atomic<float>* p5LeadPre = nullptr;
    std::atomic<float>* p5Low = nullptr;
    std::atomic<float>* p5Mid = nullptr;
    std::atomic<float>* p5High = nullptr;
    std::atomic<float>* p5RhythmPost = nullptr;
    std::atomic<float>* p5LeadPost = nullptr;
    std::atomic<float>* pPreampOut = nullptr; // 0 = Full Amp, 1 = Preamp Out
    std::atomic<float>* pTunerOn = nullptr;   // Rev3.2
    std::atomic<float>* pTunerRef = nullptr;
    std::array<std::array<std::atomic<float>*, jb::params::numPitchSlots>, 2> pPitch {}; // Rev3.3
    float p5 (jb::params::AmpSlot s) const noexcept { return p5150[(size_t) s]->load(); }
    juce::AudioParameterBool* bypassParam = nullptr;
    int baseLatency = 0;

    // DSP
    jb::dsp::OversampledNonlinear oversampler, leadOversampler;
    jb::dsp::Cascade5150 lead5, rhythm5;      // 5150 Lead / Rhythm channels (Rev2.4)
    jb::dsp::Lead5150Tone leadTone;            // shared 5150 TMB (Low/Mid/High)
    jb::dsp::Power5150 power5;                 // Post Gain -> power section
    jb::dsp::SpeakerLoad5150 speaker5;         // 4x12 load, Resonance at the cab peak
    jb::dsp::Od808Stage od, od5;               // per-amp boosts
    jb::dsp::Sd1Stage   sd1, sd15;
    jb::dsp::PreampCascade riffA, riffAv1, riffB; // Red v2, Red v1, Orange
    // Rev3: which solved cascades ran last frame (gating; see processBlock)
    bool riffARunning = false, riffAv1Running = false, riffBRunning = false, lead5Running = false, rhythm5Running = false;
    jb::dsp::ToneStack tone;
    jb::dsp::RecoveryStage rectRecovery; // post-TMB recovery gain (Rev2.2)
    jb::dsp::SpeakerLoad rectSpeaker; // Red v2 power-amp / 4x12 low end (Rev2.3)
    float recoveryRed = 0.0f, recoveryOrange = 0.0f;
    bool odRunning = false, sd1Running = false; // Boost stages processed last frame
    bool od5Running = false, sd15Running = false;
    jb::dsp::GraphicEq graphicEq, leadGraphicEq; // Rectifier / 5150 GEQ-7 (Rev3.2: at 4x)
    bool rectEqRunning = false, leadEqRunning = false;
    jb::dsp::Tuner tuner;
    std::array<jb::dsp::PitchPedal, 2> pitchPedals; // Rev3.3, per amp (before each Boost)
    struct TunerThread final : juce::Thread
    {
        explicit TunerThread (jb::dsp::Tuner& t) : juce::Thread ("juicyboi tuner"), tuner (t) {}
        void run() override;
        jb::dsp::Tuner& tuner;
    };
    std::unique_ptr<TunerThread> tunerThread;
    jb::dsp::LeadPresence leadPresence;
    jb::dsp::LeadResonance leadResonance;
    jb::dsp::CabSim cab, cab5;
    jb::dsp::NoiseGate gate, gate5;
    bool cabIdle[2] { false, false }; // amp inaudible: its cab skipped (reset before reuse)
    std::array<std::array<juce::File, jb::dsp::CabSim::numSlots>, 2> cabFiles;
    jb::dsp::CabSim& cabFor (int amp) noexcept { return amp == 0 ? cab : cab5; }
    const jb::dsp::CabSim& cabFor (int amp) const noexcept { return amp == 0 ? cab : cab5; }
    jb::dsp::ChannelVoicing voicingA, voicingAv1, voicingB;

    juce::SmoothedValue<float> modelMix; // base-rate, after both amps' gates (Rev2.4)
    juce::SmoothedValue<float> inputGain, masterGain, bypassMix; // base rate (inputGain = shared cal)
    juce::SmoothedValue<float> odMix, odTypeMix, channelMix, redVoiceMix; // oversampled rate
    juce::SmoothedValue<float> odMix5, odTypeMix5, chMix5, postLead5, postRhythm5; // oversampled rate
    juce::SmoothedValue<float> preampOutMix;   // base rate, cab / 5150 load crossfade
    juce::SmoothedValue<float> preampOutMixOs; // 4x rate, Red v2 SpeakerLoad crossfade

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 8192 };
    juce::AudioBuffer<float> dryBuffer, leadBuffer;
    juce::AudioBuffer<float> keyBuffer, keyBuffer5; // mono gate keys: post shared-cal, pre-gain (per amp)
    juce::AudioBuffer<float> preampBuffer, preampBuffer5; // post-EQ copies for the Preamp Out mix

    int maxChunk = 512;
    int lastChannel = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuicyboiAudioProcessor)
};
