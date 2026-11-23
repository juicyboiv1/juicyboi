#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "LevelTaper.h"

using namespace jb;


JuicyboiAudioProcessor::JuicyboiAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "juicyboi", params::createLayout())
{
    // Fresh state is already in the 0–10 Input/Master format (copyState() carries it).
    apvts.state.setProperty (stateVersionKey, taper::stateVersion, nullptr);

    pInputTrim = apvts.getRawParameterValue (params::id::inputTrim);
    pOdEnable  = apvts.getRawParameterValue (params::id::odEnable);
    pOdType    = apvts.getRawParameterValue (params::id::odType);
    pOdDrive   = apvts.getRawParameterValue (params::id::odDrive);
    pOdTone    = apvts.getRawParameterValue (params::id::odTone);
    pOdLevel   = apvts.getRawParameterValue (params::id::odLevel);
    pLeadPresence = apvts.getRawParameterValue (params::id::leadPresence);
    pLeadResonance = apvts.getRawParameterValue (params::id::leadResonance);
    pPreampModel = apvts.getRawParameterValue (params::id::preampModel);
    pChannel   = apvts.getRawParameterValue (params::id::channel);
    pRedVoice  = apvts.getRawParameterValue (params::id::redVoice);
    pGain      = apvts.getRawParameterValue (params::id::gain);
    pBass      = apvts.getRawParameterValue (params::id::bass);
    pMid       = apvts.getRawParameterValue (params::id::mid);
    pTreble    = apvts.getRawParameterValue (params::id::treble);
    pPresence  = apvts.getRawParameterValue (params::id::presence);
    pMaster    = apvts.getRawParameterValue (params::id::master);
    pBypass    = apvts.getRawParameterValue (params::id::bypass);
    pEqEnable  = apvts.getRawParameterValue (params::id::eqEnable);
    for (size_t b = 0; b < pEqBands.size(); ++b)
        pEqBands[b] = apvts.getRawParameterValue (params::id::eqBands[b]);
    pEqLevel   = apvts.getRawParameterValue (params::id::eqLevel);
    pCabEnable  = apvts.getRawParameterValue (params::id::cabEnable);
    pCabBlend   = apvts.getRawParameterValue (params::id::cabMicBlend);
    pCabLevel   = apvts.getRawParameterValue (params::id::cabLevel);
    pGateEnable  = apvts.getRawParameterValue (params::id::gateEnable);
    pGateThresh  = apvts.getRawParameterValue (params::id::gateThreshold);
    pGateHold    = apvts.getRawParameterValue (params::id::gateHold);
    pGateRelease = apvts.getRawParameterValue (params::id::gateRelease);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (params::id::bypass));

    for (int s = 0; s < params::numAmpSlots; ++s)
        p5150[(size_t) s] = apvts.getRawParameterValue (params::ampId (1, (params::AmpSlot) s));
    p5Channel    = apvts.getRawParameterValue (params::id::m5150Channel);
    p5Bright     = apvts.getRawParameterValue (params::id::m5150Bright);
    p5Crunch     = apvts.getRawParameterValue (params::id::m5150Crunch);
    p5RhythmPre  = apvts.getRawParameterValue (params::id::m5150RhythmPre);
    p5LeadPre    = apvts.getRawParameterValue (params::id::m5150LeadPre);
    p5Low        = apvts.getRawParameterValue (params::id::m5150Low);
    p5Mid        = apvts.getRawParameterValue (params::id::m5150Mid);
    p5High       = apvts.getRawParameterValue (params::id::m5150High);
    p5RhythmPost = apvts.getRawParameterValue (params::id::m5150RhythmPost);
    p5LeadPost   = apvts.getRawParameterValue (params::id::m5150LeadPost);
    pPreampOut   = apvts.getRawParameterValue (params::id::preampOut);

    voicingA   = dsp::voicings::riffA();   // Red v2
    voicingAv1 = dsp::voicings::riffAv1(); // Red v1
    voicingB = dsp::voicings::riffB();
}

//==============================================================================
void JuicyboiAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxChunk = juce::jmax (1, samplesPerBlock);
    const auto numCh = dsp::kMaxChannels;

    oversampler.prepare (numCh, maxChunk);
    leadOversampler.prepare (numCh, maxChunk);
    const auto osRate = sampleRate * oversampler.getFactor();

    lead5.configure (voicing5150, false);
    rhythm5.configure (voicing5150, true);
    lead5.setPreGain (p5LeadPre->load());
    rhythm5.setPreGain (p5RhythmPre->load());
    rhythm5.setSwitches (p5Bright->load() > 0.5f, p5Crunch->load() > 0.5f);
    lead5.prepare (osRate);
    rhythm5.prepare (osRate);
    leadTone.setKnobs (p5Low->load(), p5Mid->load(), p5High->load());
    leadTone.prepare (osRate); // Rev2.2: TMB + recovery run inside the 4x region
    power5.prepare (osRate, voicing5150);
    speaker5.prepare (sampleRate, voicing5150);
    leadPresence.setPosition (pLeadPresence->load());
    leadPresence.prepare (sampleRate);
    leadResonance.setPosition (pLeadResonance->load());
    leadResonance.prepare (sampleRate, leadPresence.resistance());
    leadBuffer.setSize (numCh, maxChunk, false, true, false);
    preampBuffer.setSize (numCh, maxChunk, false, true, false);
    preampBuffer5.setSize (numCh, maxChunk, false, true, false);

    od.prepare (osRate);
    sd1.prepare (osRate);
    od5.prepare (osRate);
    sd15.prepare (osRate);
    riffA.prepare (osRate, voicingA);
    riffAv1.prepare (osRate, voicingAv1);
    riffB.prepare (osRate, voicingB);
    tone.prepare (osRate);     // Rev2.2: TMB + recovery run inside the 4x region
    {
        dsp::RecoveryStage::Settings rs;
        rs.drive = voicingA.recoveryDrive;
        rs.bias  = voicingA.recoveryBias;
        rs.outGain = voicingA.recoveryOutGain;
        rectRecovery.prepare (osRate, rs);
        rectSpeaker.prepare (osRate, voicingA.speakerLoad); // Rev2.3, Red v2 only
        recoveryRed = voicingA.recoveryDrive;
        recoveryOrange = voicingB.recoveryDrive;
    }
    cab.prepare (sampleRate, maxChunk, numCh);
    cab5.prepare (sampleRate, maxChunk, numCh);
    gate.prepare (sampleRate);
    gate5.prepare (sampleRate);
    cabIdle[0] = cabIdle[1] = false;
    keyBuffer.setSize (1, maxChunk, false, true, false);
    keyBuffer5.setSize (1, maxChunk, false, true, false);

    // Cab uses zero-latency convolution; its latency (if any) is added anyway.
    baseLatency = oversampler.getLatencySamples();
    const auto latency = baseLatency + juce::jmax (cab.getLatencySamples(), cab5.getLatencySamples());
    setLatencySamples (latency);

    dryDelay.prepare ({ sampleRate, (juce::uint32) maxChunk, (juce::uint32) numCh });
    dryDelay.setMaximumDelayInSamples (latency + 1);
    dryDelay.setDelay ((float) latency);
    dryDelay.reset();
    dryBuffer.setSize (numCh, maxChunk, false, true, false);

    modelMix.reset (sampleRate, 0.030);
    modelMix.setCurrentAndTargetValue (pPreampModel->load() > 0.5f ? 1.0f : 0.0f);
    inputGain .reset (sampleRate, 0.02);
    masterGain.reset (sampleRate, 0.02);
    bypassMix .reset (sampleRate, 0.03);
    odMix     .reset (osRate, 0.015);
    odTypeMix .reset (osRate, 0.02);
    channelMix.reset (osRate, 0.02);
    redVoiceMix.reset (osRate, 0.03);
    odMix5    .reset (osRate, 0.015);
    odTypeMix5.reset (osRate, 0.02);
    chMix5    .reset (osRate, 0.02);
    postLead5 .reset (osRate, 0.02);
    postRhythm5.reset (osRate, 0.02);
    preampOutMix  .reset (sampleRate, preampOutFadeSeconds);
    preampOutMixOs.reset (osRate, preampOutFadeSeconds);

    // Snap smoothers to current values so there is no fade-in on load
    inputGain .setCurrentAndTargetValue (taper::inputKnobToGain (pInputTrim->load()));
    masterGain.setCurrentAndTargetValue (taper::masterKnobToGain (pMaster->load()));
    bypassMix .setCurrentAndTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
    odMix     .setCurrentAndTargetValue (pOdEnable->load() > 0.5f ? 1.0f : 0.0f);
    odTypeMix .setCurrentAndTargetValue (pOdType->load() > 0.5f ? 1.0f : 0.0f); // 1 = SD-1
    channelMix.setCurrentAndTargetValue (pChannel->load() > 0.5f ? 1.0f : 0.0f);
    redVoiceMix.setCurrentAndTargetValue (pRedVoice->load() > 0.5f ? 1.0f : 0.0f);
    {
        using S = params::AmpSlot;
        odMix5    .setCurrentAndTargetValue (p5 (S::odEnable) > 0.5f ? 1.0f : 0.0f);
        odTypeMix5.setCurrentAndTargetValue (p5 (S::odType) > 0.5f ? 1.0f : 0.0f);
        chMix5    .setCurrentAndTargetValue (p5Channel->load() > 0.5f ? 1.0f : 0.0f);
        postLead5 .setCurrentAndTargetValue (dsp::Power5150::postGain (p5LeadPost->load()));
        postRhythm5.setCurrentAndTargetValue (dsp::Power5150::postGain (p5RhythmPost->load()));
    }
    {
        const auto po = pPreampOut->load() > 0.5f ? 1.0f : 0.0f;
        preampOutMix  .setCurrentAndTargetValue (po);
        preampOutMixOs.setCurrentAndTargetValue (po);
    }

    lastChannel = -1;
    updateParameters();
    tone.snap();     // passive TMBs: start on the session's knobs/channel, no fade-in
    leadTone.snap();
    od.snap();       // Boost clippers: start on the session's Drive/Tone/Level
    sd1.snap();
    od5.snap();
    sd15.snap();
    lead5.snap();    // 5150 Pre Gain networks / Rhythm switches: no fade-in on load
    rhythm5.snap();
    leadGraphicEq.prepare (sampleRate, maxChunk);
    graphicEq.prepare (sampleRate, maxChunk); // after params: snaps EQ to current settings (no fade-in on load)
}

void JuicyboiAudioProcessor::releaseResources()
{
    oversampler.reset();
    leadOversampler.reset();
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

    // Standalone: the audio device hands us however many interface inputs are
    // ticked in Audio/MIDI Settings (e.g. Scarlett 4i4 inputs 1..4). Accept any
    // count; processBlock folds them down to one mono guitar signal, so the
    // guitar is heard whichever input it is plugged into.
    if (wrapperType == wrapperType_Standalone)
        return ! in.isDisabled();

    return in == out;
}

//==============================================================================
void JuicyboiAudioProcessor::updateParameters()
{
    modelMix.setTargetValue (pPreampModel->load() > 0.5f ? 1.0f : 0.0f);
    inputGain .setTargetValue (taper::inputKnobToGain (pInputTrim->load()));
    masterGain.setTargetValue (taper::masterKnobToGain (pMaster->load()));
    bypassMix .setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
    odMix     .setTargetValue (pOdEnable->load() > 0.5f ? 1.0f : 0.0f);
    odTypeMix .setTargetValue (pOdType->load() > 0.5f ? 1.0f : 0.0f); // 1 = SD-1

    const auto ch = pChannel->load() > 0.5f ? 1 : 0;
    channelMix.setTargetValue ((float) ch);
    redVoiceMix.setTargetValue (pRedVoice->load() > 0.5f ? 1.0f : 0.0f); // 1 = v2

    // Boost Drive/Tone/Level are targets; the stages smooth them per sample.
    const auto odDrive01 = pOdDrive->load() / 10.0f;
    const auto odTone01  = pOdTone->load() / 10.0f;
    const auto odLvl     = taper::boostLevelKnobToGain (pOdLevel->load());
    od.setParameters (odDrive01, odTone01, odLvl);
    sd1.setParameters (odDrive01, odTone01, odLvl);

    const auto gain01 = pGain->load() / 10.0f;
    riffA.setGain (gain01);
    riffAv1.setGain (gain01);
    riffB.setGain (gain01);

    if (ch != lastChannel)
    {
        lastChannel = ch;
        tone.setVoicing (ch == 0 ? voicingA : voicingB);
    }

    tone.setKnobs (pBass->load(), pMid->load(), pTreble->load(), pPresence->load());

    {
        std::array<float, dsp::GraphicEq::numBands> eqDb {};
        for (size_t b = 0; b < eqDb.size(); ++b)
            eqDb[b] = pEqBands[b]->load();
        graphicEq.setParameters (pEqEnable->load() > 0.5f, eqDb, pEqLevel->load());
    }

    cab.setParameters (pCabEnable->load() > 0.5f, pCabBlend->load(), taper::cabLevelKnobToGain (pCabLevel->load()));

    gate.setParameters (pGateEnable->load() > 0.5f, pGateThresh->load(), pGateHold->load(), pGateRelease->load());

    // Rev2.4: the 5150's own set (docs/per-amp-state.md)
    {
        using S = params::AmpSlot;
        odMix5    .setTargetValue (p5 (S::odEnable) > 0.5f ? 1.0f : 0.0f);
        odTypeMix5.setTargetValue (p5 (S::odType) > 0.5f ? 1.0f : 0.0f);
        const auto d5 = p5 (S::odDrive) / 10.0f, t5 = p5 (S::odTone) / 10.0f;
        const auto l5 = taper::boostLevelKnobToGain (p5 (S::odLevel));
        od5.setParameters (d5, t5, l5);
        sd15.setParameters (d5, t5, l5);

        lead5.setPreGain (p5LeadPre->load());
        rhythm5.setPreGain (p5RhythmPre->load());
        rhythm5.setSwitches (p5Bright->load() > 0.5f, p5Crunch->load() > 0.5f);
        chMix5.setTargetValue (p5Channel->load() > 0.5f ? 1.0f : 0.0f);
        postLead5.setTargetValue (dsp::Power5150::postGain (p5LeadPost->load()));
        postRhythm5.setTargetValue (dsp::Power5150::postGain (p5RhythmPost->load()));
        leadTone.setKnobs (p5Low->load(), p5Mid->load(), p5High->load());
        leadPresence.setPosition (pLeadPresence->load());
        leadResonance.setPosition (pLeadResonance->load());

        std::array<float, dsp::GraphicEq::numBands> eqDb {};
        for (size_t b = 0; b < eqDb.size(); ++b)
            eqDb[b] = p5 ((S) ((int) S::eq100 + (int) b));
        leadGraphicEq.setParameters (p5 (S::eqEnable) > 0.5f, eqDb, p5 (S::eqLevel));
        cab5.setParameters (p5 (S::cabEnable) > 0.5f, p5 (S::cabMicBlend), taper::cabLevelKnobToGain (p5 (S::cabLevel)));
        gate5.setParameters (p5 (S::gateEnable) > 0.5f, p5 (S::gateThreshold), p5 (S::gateHold), p5 (S::gateRelease));
    }

    const auto po = pPreampOut->load() > 0.5f ? 1.0f : 0.0f;
    preampOutMix  .setTargetValue (po);
    preampOutMixOs.setTargetValue (po);
}

void JuicyboiAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numIn  = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    const auto mono = foldStandaloneInputsToMono (buffer);

    for (auto ch = (mono ? 1 : numIn); ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const auto numProc = mono ? 1 : juce::jmin (numIn, dsp::kMaxChannels, buffer.getNumChannels());
    if (numProc <= 0 || numSamples == 0)
        return;

    updateParameters();

    // Hosts may exceed the prepared block size: process in safe chunks.
    for (int start = 0; start < numSamples; start += maxChunk)
        processChunk (buffer, start, juce::jmin (maxChunk, numSamples - start), numProc);

    finaliseOutputs (buffer, mono);
}

bool JuicyboiAudioProcessor::foldStandaloneInputsToMono (juce::AudioBuffer<float>& buffer) const
{
    const auto numIn = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());

    if (numIn <= 1)
        return numIn == 1;

    if (wrapperType != wrapperType_Standalone)
        return false; // plugin hosts: keep true stereo-in -> stereo-out

    // Standalone: sum every active interface input into channel 0, so a mono
    // guitar on Scarlett input 1 (or 2/3/4) always reaches the preamp even when
    // several input checkboxes are ticked or the device opens as stereo.
    const auto n = buffer.getNumSamples();
    for (int ch = 1; ch < numIn; ++ch)
        buffer.addFrom (0, 0, buffer, ch, 0, n);

    return true;
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

    // 1) Shared input calibration (input_trim). The Rectifier's in-place
    //    multiply is unchanged from Rev2.4; the 5150 then hears the same
    //    trimmed guitar (m5150_input_trim is unused / hidden).
    for (int i = 0; i < len; ++i)
    {
        const auto g = inputGain.getNextValue();
        for (int ch = 0; ch < numProc; ++ch)
            buffer.getWritePointer (ch, start)[i] *= g;
    }
    for (int ch = 0; ch < numProc; ++ch)
        leadBuffer.copyFrom (ch, 0, buffer, ch, start, len);

    // 1b) Gate keys: clean post-trim guitar (mono sum), captured before any
    //     gain stage so the detector follows picking, not preamp hiss.
    {
        auto* k = keyBuffer.getWritePointer (0);
        auto* k5 = keyBuffer5.getWritePointer (0);
        const auto norm = 1.0f / (float) numProc;
        for (int i = 0; i < len; ++i)
        {
            float sum = 0.0f, sum5 = 0.0f;
            for (int ch = 0; ch < numProc; ++ch)
            {
                sum += buffer.getReadPointer (ch, start)[i];
                sum5 += leadBuffer.getReadPointer (ch)[i];
            }
            k[i] = sum * norm;
            k5[i] = sum5 * norm;
        }
    }

    // 2) Nonlinear section at 4x: per-amp boost -> preamp -> TMB -> recovery
    auto block = juce::dsp::AudioBlock<float> (buffer)
                     .getSubsetChannelBlock (0, (size_t) numProc)
                     .getSubBlock ((size_t) start, (size_t) len);

    auto leadBlock = juce::dsp::AudioBlock<float> (leadBuffer)
                         .getSubsetChannelBlock (0, (size_t) numProc).getSubBlock (0, (size_t) len);
    oversampler.processParallel (block, leadBlock, leadOversampler,
        [this, numProc] (juce::dsp::AudioBlock<float>& up, juce::dsp::AudioBlock<float>& leadUp)
    {
        const auto n = (int) up.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            od.beginFrame();
            sd1.beginFrame();
            const auto odAmt   = odMix.getNextValue();
            const auto typeAmt = odTypeMix.getNextValue(); // 0 = OD-808, 1 = SD-1
            const auto chAmt   = channelMix.getNextValue();
            const auto rvAmt   = redVoiceMix.getNextValue(); // 0 = Red v1, 1 = Red v2
            const auto recDrive = recoveryRed + chAmt * (recoveryOrange - recoveryRed);
            const auto spkAmt  = rvAmt * (1.0f - chAmt); // Red v2 share of the Rect output (Rev2.3)
            const auto poOs    = preampOutMixOs.getNextValue(); // 0 = Full Amp, 1 = skip SpeakerLoad

            // Boost stages needed this frame (both while a crossfade is running)
            const bool boostAudible = odAmt > 0.0f || odMix.isSmoothing();
            const bool runOd  = boostAudible && (typeAmt < 1.0f || odTypeMix.isSmoothing());
            const bool runSd1 = boostAudible && (typeAmt > 0.0f || odTypeMix.isSmoothing());
            if (runOd && ! odRunning)   od.reset();
            if (runSd1 && ! sd1Running) sd1.reset();
            odRunning = runOd;
            sd1Running = runSd1;
            tone.beginFrame();

            // 5150 set: its own boost (same logic), channel relay and Post pots
            od5.beginFrame();
            sd15.beginFrame();
            lead5.beginFrame();
            rhythm5.beginFrame();
            leadTone.beginFrame();
            const auto odAmt5  = odMix5.getNextValue();
            const auto typeAmt5 = odTypeMix5.getNextValue();
            const auto leadAmt = chMix5.getNextValue();       // 0 = Rhythm, 1 = Lead
            const auto gLead   = postLead5.getNextValue();
            const auto gRhythm = postRhythm5.getNextValue();
            const bool boostAudible5 = odAmt5 > 0.0f || odMix5.isSmoothing();
            const bool runOd5  = boostAudible5 && (typeAmt5 < 1.0f || odTypeMix5.isSmoothing());
            const bool runSd15 = boostAudible5 && (typeAmt5 > 0.0f || odTypeMix5.isSmoothing());
            if (runOd5 && ! od5Running)   od5.reset();
            if (runSd15 && ! sd15Running) sd15.reset();
            od5Running = runOd5;
            sd15Running = runSd15;

            for (int ch = 0; ch < numProc; ++ch)
            {
                auto* d = up.getChannelPointer ((size_t) ch);
                const auto x = d[i];

                // Type and on/off are short equal-gain crossfades so switching
                // never clicks. Rev2.2: the solved clippers only run while they
                // are audible (a stage restarts from rest when it is needed).
                const auto odOut  = runOd  ? od.process (x, ch)  : x;
                const auto sd1Out = runSd1 ? sd1.process (x, ch) : x;
                const auto wet    = odOut + typeAmt * (sd1Out - odOut);
                const auto boosted = x + odAmt * (wet - x);

                // All cascades run (state stays warm); Red v1/v2 and
                // Red/Orange switching are short equal-gain crossfades.
                const auto a2 = riffA.process (boosted, ch);
                const auto a1 = riffAv1.process (boosted, ch);
                const auto a  = a1 + rvAmt * (a2 - a1);
                const auto b  = riffB.process (boosted, ch);

                // Passive TMB, then the post-stack recovery gain (both at 4x)
                const auto rect = tone.process (a + chAmt * (b - a), ch);
                const auto rec  = rectRecovery.process (rect, recDrive, ch);
                // Rev2.3: Red v2 power-amp / 4x12 low-end interaction. Linear,
                // always running (warm state); weight 0 (Red v1, Orange) passes
                // `rec` through untouched, so those voices stay bit-identical.
                const auto spk  = rectSpeaker.process (rec, ch);
                // Full Amp (poOs == 0): exact previous blend, so Red v2 fixtures stay
                // bit-identical. Preamp Out fades the 4x12 load out (Red v1 / Orange
                // already pass `rec` through, so they are unaffected).
                d[i] = poOs <= 0.0f
                           ? (spkAmt > 0.0f ? rec + spkAmt * (spk - rec) : rec)
                           : rec + (spkAmt * (1.0f - poOs)) * (spk - rec);

                // 5150: both channels run (warm); the relay is a short crossfade
                // after each channel's Post pot (the TMB between is linear and
                // shared, so Post-before-TMB is the same signal as on the amp).
                auto* l = leadUp.getChannelPointer ((size_t) ch);
                const auto x5 = l[i];
                const auto od5Out  = runOd5  ? od5.process (x5, ch)  : x5;
                const auto sd15Out = runSd15 ? sd15.process (x5, ch) : x5;
                const auto boosted5 = x5 + odAmt5 * ((od5Out + typeAmt5 * (sd15Out - od5Out)) - x5);
                const auto rhy = rhythm5.process (boosted5, ch) * gRhythm;
                const auto ld  = lead5.process (boosted5, ch) * gLead;
                l[i] = power5.process (leadTone.process (rhy + leadAmt * (ld - rhy), ch), ch);
            }
        }
    });

    // 3) Tone stacks + recovery run inside the oversampled region (step 2).

    // 3b) Post EQ, one per amp with its own settings (Rev2.4).
    {
        float* rect[dsp::kMaxChannels] {}, *alt[dsp::kMaxChannels] {};
        for (int ch = 0; ch < numProc; ++ch)
        {
            rect[ch] = buffer.getWritePointer (ch, start);
            alt[ch] = leadBuffer.getWritePointer (ch);
        }
        graphicEq.process (rect, numProc, len);
        leadGraphicEq.process (alt, numProc, len);
    }

    // Preamp Out copies the post-EQ (pre-cab, and for the 5150 pre-Presence /
    // Resonance / speaker-load) signal so the fade can mix it back in. When
    // the mode is settled Full Amp this is skipped and the path below is
    // byte-for-byte the previous build.
    const bool preampOutActive = preampOutMix.isSmoothing() || preampOutMix.getCurrentValue() > 0.0f;
    if (preampOutActive)
    {
        for (int ch = 0; ch < numProc; ++ch)
        {
            preampBuffer.copyFrom (ch, 0, buffer, ch, start, len);
            preampBuffer5.copyFrom (ch, 0, leadBuffer, ch, 0, len);
        }
    }

    // 3c) 5150 Presence + Resonance (one NFB loop) and the 4x12 load, whose
    //     output impedance follows the same loop (Resonance acts at the cab peak).
    speaker5.setLoop (leadResonance.resistance(), leadPresence.resistance());
    for (int i = 0; i < len; ++i)
    {
        leadPresence.beginFrame();
        leadResonance.beginFrame (leadPresence.resistance());
        if ((i & 31) == 0) speaker5.setLoop (leadResonance.resistance(), leadPresence.resistance());
        speaker5.beginFrame();
        for (int ch = 0; ch < numProc; ++ch)
        {
            auto& v = leadBuffer.getWritePointer (ch)[i];
            v = speaker5.process (leadResonance.process (leadPresence.process (v, ch), ch), ch);
        }
    }

    // 4) Cab IR sims, one per amp (no-op when that Cab is off). An amp that is
    //    fully faded out, or fully in Preamp Out, skips its convolution; it
    //    restarts clean when needed.
    {
        const auto m = modelMix.getCurrentValue();
        const auto poNow = preampOutMix.getCurrentValue();
        const bool cabWanted = poNow < 1.0f || preampOutMix.isSmoothing();
        const bool audible[2] { cabWanted && (m < 1.0f || modelMix.isSmoothing()),
                                cabWanted && (m > 0.0f || modelMix.isSmoothing()) };
        for (int a = 0; a < 2; ++a)
        {
            if (! audible[a]) { cabIdle[a] = true; continue; }
            auto& c = cabFor (a);
            if (cabIdle[a]) { c.reset(); cabIdle[a] = false; }
            auto& src = a == 0 ? buffer : leadBuffer;
            c.process (juce::dsp::AudioBlock<float> (src)
                           .getSubsetChannelBlock (0, (size_t) numProc)
                           .getSubBlock ((size_t) (a == 0 ? start : 0), (size_t) len));
        }
    }

    // 4b) Preamp Out crossfade: mix the post-EQ preamp signal (no cab, and for
    //     the 5150 no Presence/Resonance/speaker load) with the Full Amp path.
    //     Settled Full Amp never enters this block (bit-identical).
    if (preampOutActive)
    {
        for (int i = 0; i < len; ++i)
        {
            const auto po = preampOutMix.getNextValue();
            for (int ch = 0; ch < numProc; ++ch)
            {
                auto& r = buffer.getWritePointer (ch, start)[i];
                auto& l = leadBuffer.getWritePointer (ch)[i];
                const auto rPre = preampBuffer.getReadPointer (ch)[i] * preampOutGain;
                const auto lPre = preampBuffer5.getReadPointer (ch)[i] * preampOutGain;
                // Exact assign at the endpoints: r += 1*(pre-r) is not bit-identical
                // to r = pre (rounding), and would leak Presence/cab by 1 ULP.
                if (po >= 1.0f)      { r = rPre; l = lPre; }
                else if (po > 0.0f)  { r += po * (rPre - r); l += po * (lPre - l); }
            }
        }
    }

    // 4c) Gate VCAs after the cabs / Preamp Out mix (true bypass when off), each
    //     keyed from its own post-trim input. The key runs ~oversampler-latency
    //     ahead of the wet path: a tiny look-ahead so pick attacks are never clipped.
    {
        float* chans[dsp::kMaxChannels] {}, *chans5[dsp::kMaxChannels] {};
        for (int ch = 0; ch < numProc; ++ch)
        {
            chans[ch] = buffer.getWritePointer (ch, start);
            chans5[ch] = leadBuffer.getWritePointer (ch);
        }
        gate.process (keyBuffer.getReadPointer (0), chans, numProc, len);
        gate5.process (keyBuffer5.getReadPointer (0), chans5, numProc, len);
    }

    // 5) Rectifier master, model crossfade (the 5150 has no Master: its Post
    //    pots set its level), 6) bypass crossfade
    for (int i = 0; i < len; ++i)
    {
        const auto m  = masterGain.getNextValue();
        const auto bp = bypassMix.getNextValue();
        const auto mix = modelMix.getNextValue();

        for (int ch = 0; ch < numProc; ++ch)
        {
            auto* d = buffer.getWritePointer (ch, start);
            auto wet = d[i] * m;
            if (mix > 0.0f)
            {
                const auto alt = leadBuffer.getReadPointer (ch)[i];
                wet = mix >= 1.0f ? alt : wet + mix * (alt - wet);
            }
            const auto dry = dryBuffer.getReadPointer (ch)[i];
            d[i] = wet + bp * (dry - wet);
        }
    }
}

void JuicyboiAudioProcessor::finaliseOutputs (juce::AudioBuffer<float>& buffer, bool monoIn)
{
    // Mono guitar in -> copy processed mono to remaining outputs
    if (monoIn)
        for (int ch = 1; ch < juce::jmin (getTotalNumOutputChannels(), buffer.getNumChannels()); ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, buffer.getNumSamples());
}

void JuicyboiAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Host-side bypass: pass dry audio delayed by our reported latency.
    juce::ScopedNoDenormals noDenormals;
    const auto numIn  = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();
    const auto mono = foldStandaloneInputsToMono (buffer);
    const auto numProc = mono ? 1 : juce::jmin (numIn, dsp::kMaxChannels, buffer.getNumChannels());

    for (auto ch = (mono ? 1 : numIn); ch < numOut; ++ch)
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

    finaliseOutputs (buffer, mono);
}

//==============================================================================
bool JuicyboiAudioProcessor::loadCabIR (int amp, int slot, const juce::File& file)
{
    if (amp < 0 || amp > 1 || slot < 0 || slot >= dsp::CabSim::numSlots || ! cabFor (amp).loadImpulseResponse (slot, file))
        return false;
    cabFiles[(size_t) amp][(size_t) slot] = file;
    return true;
}

void JuicyboiAudioProcessor::clearCabIR (int amp, int slot)
{
    if (amp < 0 || amp > 1 || slot < 0 || slot >= dsp::CabSim::numSlots) return;
    cabFor (amp).clearSlot (slot);
    cabFiles[(size_t) amp][(size_t) slot] = juce::File();
}

juce::File JuicyboiAudioProcessor::getCabIRFile (int amp, int slot) const
{
    return (amp >= 0 && amp <= 1 && slot >= 0 && slot < dsp::CabSim::numSlots) ? cabFiles[(size_t) amp][(size_t) slot] : juce::File();
}

void JuicyboiAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (stateVersionKey, taper::stateVersion, nullptr);
    for (int a = 0; a < 2; ++a)
        for (int s = 0; s < dsp::CabSim::numSlots; ++s)
            state.setProperty (params::ampIrKey (a, s), cabFiles[(size_t) a][(size_t) s].getFullPathName(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

float JuicyboiAudioProcessor::leadPostForMaster (float masterKnob)
{
    // Same small-signal level change as the old Master move from its default
    // (Master 6.5 <-> Post 5): Post gain = audio taper, Master = LevelTaper.
    const auto ratio = taper::masterKnobToGain (masterKnob) / taper::masterKnobToGain (taper::masterDefaultKnob);
    if (ratio <= 0.0f) return 0.0f;
    const auto g = (double) ratio * dsp::audioTaper (5.0);
    return (float) juce::jlimit (0.0, 10.0, 10.0 * std::log10 (99.0 * g + 1.0) / 2.0);
}

void JuicyboiAudioProcessor::migrateToPerAmp (juce::ValueTree& tree)
{
    auto valueOf = [this, &tree] (const char* pid) -> float
    {
        const auto child = tree.getChildWithProperty ("id", pid);
        if (child.isValid() && child.hasProperty ("value"))
            return (float) child.getProperty ("value");
        auto* p = apvts.getParameter (pid);
        return p != nullptr ? p->convertFrom0to1 (p->getDefaultValue()) : 0.0f;
    };
    auto setValue = [&tree] (const char* pid, float v)
    {
        auto child = tree.getChildWithProperty ("id", pid);
        if (! child.isValid())
        {
            child = juce::ValueTree ("PARAM");
            child.setProperty ("id", pid, nullptr);
            tree.addChild (child, -1, nullptr);
        }
        child.setProperty ("value", v, nullptr);
    };

    // Every per-amp slot (boost, post EQ, cab, gate, and unused m5150_input_trim):
    // the old value goes into the 5150's set too; the legacy IDs remain the
    // Rectifier's set. input_trim is the shared calibration and already holds
    // the Rectifier / legacy input level, so old sessions keep that level.
    for (int s = 0; s < params::numAmpSlots; ++s)
        setValue (params::ampId (1, (params::AmpSlot) s), valueOf (params::ampId (0, (params::AmpSlot) s)));

    // The old 5150 was the Lead channel on the shared Gain/Bass/Mid/Treble.
    setValue (params::id::m5150Channel, 1.0f);
    setValue (params::id::m5150LeadPre, valueOf (params::id::gain));
    setValue (params::id::m5150Low,     valueOf (params::id::bass));
    setValue (params::id::m5150Mid,     valueOf (params::id::mid));
    setValue (params::id::m5150High,    valueOf (params::id::treble));
    // No Master on the 5150: its level moves to the Post pots.
    const auto post = leadPostForMaster (valueOf (params::id::master));
    setValue (params::id::m5150LeadPost, post);
    setValue (params::id::m5150RhythmPost, post);
    // New Rhythm controls start at their defaults (not selected by old sessions).
    for (auto* pid : { params::id::m5150RhythmPre, params::id::m5150Bright, params::id::m5150Crunch })
    {
        auto* p = apvts.getParameter (pid);
        setValue (pid, p->convertFrom0to1 (p->getDefaultValue()));
    }

    for (int s = 0; s < dsp::CabSim::numSlots; ++s)
        tree.setProperty (params::ampIrKey (1, s), tree.getProperty (params::ampIrKey (0, s), juce::String()), nullptr);
}

void JuicyboiAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            // Older states store some level knobs in dB; convert them to the 0–10
            // knobs so the session plays at exactly the same level.
            //   v1 (no property): Input, Master, Boost Level, Cab Level in dB
            //   v2:               Boost Level, Cab Level in dB
            const int loadedVersion = (int) tree.getProperty (stateVersionKey, 1);
            auto convert = [&tree] (const char* pid, auto&& dbToKnob)
            {
                auto child = tree.getChildWithProperty ("id", pid);
                if (child.isValid() && child.hasProperty ("value"))
                    child.setProperty ("value", (float) dbToKnob ((double) child.getProperty ("value")), nullptr);
            };
            if (loadedVersion < 2)
            {
                convert (params::id::inputTrim, taper::inputDbToKnob);
                convert (params::id::master,    taper::masterDbToKnob);
            }
            if (loadedVersion < 3)
            {
                convert (params::id::odLevel,  taper::boostLevelDbToKnob);
                convert (params::id::cabLevel, taper::cabLevelDbToKnob);
            }
            // v4 (Rev2.4): per-amp settings. Old values go into every amp's set.
            if (loadedVersion < 4)
                migrateToPerAmp (tree);

            // Any parameter the state does not mention gets its default, even
            // when loading over a processor that was changed (pre-model sessions
            // -> Rectifier; pre-Presence/Resonance sessions -> 0 = their sound).
            for (auto* p : getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                    if (! tree.getChildWithProperty ("id", rp->getParameterID()).isValid())
                    {
                        juce::ValueTree child ("PARAM");
                        child.setProperty ("id", rp->getParameterID(), nullptr);
                        child.setProperty ("value", rp->convertFrom0to1 (rp->getDefaultValue()), nullptr);
                        tree.addChild (child, -1, nullptr);
                    }

            tree.setProperty (stateVersionKey, taper::stateVersion, nullptr);
            apvts.replaceState (tree);

            for (int a = 0; a < 2; ++a)
                for (int s = 0; s < dsp::CabSim::numSlots; ++s)
                {
                    const auto path = tree.getProperty (params::ampIrKey (a, s)).toString();
                    if (path.isNotEmpty() && juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile())
                        loadCabIR (a, s, juce::File (path));
                    else
                        clearCabIR (a, s);
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
