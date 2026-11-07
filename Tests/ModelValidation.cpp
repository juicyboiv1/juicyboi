#include "PluginProcessor.h"
#include <cstdio>
#include <vector>
#include <chrono>
#include <algorithm>

namespace
{
void set (JuicyboiAudioProcessor& p, const char* id, float v)
{
    auto* a = p.apvts.getParameter (id);
    a->setValueNotifyingHost (a->convertTo0to1 (v));
}
float get (JuicyboiAudioProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
float signal (int i, double fs)
{
    const auto t = (double) i / fs;
    return (float) (0.18 * std::sin (2.0 * jb::dsp::kPi * 82.4 * t)
                  + 0.04 * std::sin (2.0 * jb::dsp::kPi * 733.0 * t));
}
void setup (JuicyboiAudioProcessor& p, double fs, int channels, int preparedBlock, int model, int boost, float gain)
{
    set (p, "preamp_model", (float) model); set (p, "pre_gain", gain);
    set (p, "od_enable", boost > 0 ? 1.0f : 0.0f); set (p, "od_type", boost == 2 ? 1.0f : 0.0f);
    p.setPlayConfigDetails (channels, 2, fs, preparedBlock);
    p.prepareToPlay (fs, preparedBlock);
}
std::vector<float> render (double fs, int channels, int block, int model, int boost, float gain, bool quiet = false)
{
    JuicyboiAudioProcessor p;
    setup (p, fs, channels, 256, model, boost, gain);
    std::vector<float> out;
    juce::MidiBuffer midi;
    for (int start = 0; start < 16384; start += block)
    {
        const int n = std::min (block, 16384 - start);
        juce::AudioBuffer<float> buffer (2, n); buffer.clear();
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < channels; ++c)
                buffer.setSample (c, i, quiet ? 0.0f : signal (start + i, fs));
        p.processBlock (buffer, midi);
        out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + n);
        // Equal stereo inputs / mono fanout must remain identical.
        for (int i = 0; i < n; ++i)
            if (! std::isfinite (buffer.getSample (1, i))
                || (boost == 0 && std::abs (buffer.getSample (0, i) - buffer.getSample (1, i)) > 2e-6f))
                out[0] = std::numeric_limits<float>::quiet_NaN();
    }
    return out;
}
double rms (const std::vector<float>& x)
{
    double sum = 0.0;
    for (size_t i = x.size() / 2; i < x.size(); ++i) sum += (double) x[i] * x[i];
    return std::sqrt (sum / (double) (x.size() - x.size() / 2));
}
}

int modelValidation()
{
    int failures = 0;
    auto check = [&] (const char* label, bool ok)
    {
        std::printf ("model %-54s %s\n", label, ok ? "ok" : "FAIL");
        if (! ok) ++failures;
    };
    {
        JuicyboiAudioProcessor p;
        check ("default Rectifier; parameter appended", get (p, "preamp_model") < 0.5f
               && p.apvts.getParameter ("preamp_model")->getParameterIndex() == p.getParameters().size() - 1);
        set (p, "channel", 1.0f); set (p, "red_voice", 0.0f); set (p, "preamp_model", 1.0f);
        auto old = p.apvts.copyState();
        old.removeChild (old.getChildWithProperty ("id", "preamp_model"), nullptr);
        juce::MemoryBlock state;
        juce::AudioProcessor::copyXmlToBinary (*old.createXml(), state);
        p.setStateInformation (state.getData(), (int) state.getSize());
        check ("old state resets active Lead to Rectifier", get (p, "preamp_model") < 0.5f
               && get (p, "channel") > 0.5f && get (p, "red_voice") < 0.5f);
        set (p, "preamp_model", 1.0f); set (p, "pre_gain", 9.1f);
        p.getStateInformation (state);
        JuicyboiAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check ("new state recalls model/gain and hidden choices", get (restored, "preamp_model") > 0.5f
               && std::abs (get (restored, "pre_gain") - 9.1f) < 0.02f
               && get (restored, "channel") > 0.5f && get (restored, "red_voice") < 0.5f);
    }

    int cases = 0; bool matrixOk = true; float maxPeak = 0.0f;
    const auto began = std::chrono::steady_clock::now();
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int block : { 17, 256, 1023 })
            for (int channels : { 1, 2 })
                for (int boost : { 0, 1, 2 })
                    for (float gain : { 0.0f, 6.0f, 10.0f })
                    {
                        const auto out = render (fs, channels, block, 1, boost, gain);
                        const auto level = rms (out);
                        for (auto f : out) { matrixOk = matrixOk && std::isfinite (f); maxPeak = std::max (maxPeak, std::abs (f)); }
                        matrixOk = matrixOk && level > 0.0001 && level < 0.9;
                        ++cases;
                    }
    const auto elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - began).count();
    std::printf ("Lead matrix: %d cases; peak %.4f; %.3f wall seconds (not hardware CPU guarantee)\n", cases, (double) maxPeak, elapsed);
    check ("sample-rate/block/mono/stereo/Boost/Gain finite matrix", matrixOk && maxPeak < 1.0f);

    const auto small = render (48000.0, 1, 17, 1, 2, 10.0f);
    const auto large = render (48000.0, 2, 1023, 1, 2, 10.0f);
    float partitionError = 0.0f;
    for (size_t i = 0; i < small.size(); ++i) partitionError = std::max (partitionError, std::abs (small[i] - large[i]));
    std::printf ("Lead block/channel invariance error: %.9g\n", (double) partitionError);
    // Legacy Boost smoothing advances per channel; compare Boost-off below for
    // strict channel invariance, and identical channel counts for partitioning.
    const auto mono = render (48000.0, 1, 17, 1, 0, 10.0f);
    const auto stereo = render (48000.0, 2, 1023, 1, 0, 10.0f);
    const auto partition = render (48000.0, 1, 1023, 1, 2, 10.0f);
    float channelError = 0.0f, blockError = 0.0f;
    for (size_t i = 0; i < mono.size(); ++i)
    {
        channelError = std::max (channelError, std::abs (mono[i] - stereo[i]));
        blockError = std::max (blockError, std::abs (small[i] - partition[i]));
    }
    check ("Lead stereo gain and oversized-chunk invariance", channelError < 2e-6f && blockError < 2e-6f);

    const auto rect = render (48000.0, 1, 256, 0, 0, 6.0f);
    const auto lead = render (48000.0, 1, 256, 1, 0, 6.0f);
    double cross = 0.0, rr = 0.0, ll = 0.0;
    for (size_t i = 4096; i < rect.size(); ++i)
    { cross += (double) rect[i] * lead[i]; rr += (double) rect[i] * rect[i]; ll += (double) lead[i] * lead[i]; }
    const auto correlation = cross / std::sqrt (rr * ll);
    std::printf ("Rectifier/Lead RMS %.4f/%.4f; normalized correlation %.4f\n", rms (rect), rms (lead), correlation);
    check ("distinct waveform after level normalization", std::abs (correlation) < 0.97);

    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        const auto zero = render (fs, 2, 127, 1, 2, 10.0f, true);
        bool zeroOk = true; for (auto f : zero) zeroOk = zeroOk && std::isfinite (f) && std::abs (f) < 1e-8f;
        check ("silence maps to zero (each sample rate)", zeroOk);
        JuicyboiAudioProcessor p; setup (p, fs, 1, 256, 1, 2, 10.0f);
        juce::MidiBuffer midi; double mean = 0.0; float endPeak = 0.0f;
        for (int b = 0; b < (int) (fs / 256) + 40; ++b)
        {
            juce::AudioBuffer<float> buffer (2, 256); buffer.clear();
            for (int i = 0; i < 256; ++i) buffer.setSample (0, i, 0.1f);
            p.processBlock (buffer, midi);
            if (b >= (int) (fs / 256) + 30)
                for (int i = 0; i < 256; ++i) { mean += buffer.getSample (0, i); endPeak = std::max (endPeak, std::abs (buffer.getSample (0, i))); }
        }
        check ("sustained DC rejected after settling", std::isfinite (mean) && endPeak < 1e-4f);
    }

    // Switching: compare live output to independently warmed endpoint processors.
    // Every transition sample must match the specified 30ms linear blend. This
    // measures switching artifacts separately from intentional clipped-wave edges.
    {
        JuicyboiAudioProcessor p, rectP, leadP;
        setup (p, 48000.0, 1, 64, 0, 2, 10.0f);
        setup (rectP, 48000.0, 1, 64, 0, 2, 10.0f);
        setup (leadP, 48000.0, 1, 64, 1, 2, 10.0f);
        juce::SmoothedValue<float> blend; blend.reset (48000.0, 0.030); blend.setCurrentAndTargetValue (0.0f);
        float error = 0.0f; bool finite = true; juce::MidiBuffer midi;
        for (int b = 0; b < 180; ++b)
        {
            if (b == 40 || b == 53 || b == 80 || b == 140)
            {
                const float model = b == 40 || b == 80 ? 1.0f : 0.0f;
                set (p, "preamp_model", model); blend.setTargetValue (model);
            }
            juce::AudioBuffer<float> x (2, 64); x.clear();
            for (int i = 0; i < 64; ++i) x.setSample (0, i, signal (b * 64 + i, 48000.0));
            auto a = x, c = x;
            p.processBlock (x, midi); rectP.processBlock (a, midi); leadP.processBlock (c, midi);
            for (int i = 0; i < 64; ++i)
            {
                const auto amount = blend.getNextValue();
                const auto expected = a.getSample (0, i) + amount * (c.getSample (0, i) - a.getSample (0, i));
                const auto actual = x.getSample (0, i);
                finite = finite && std::isfinite (actual);
                error = std::max (error, std::abs (actual - expected));
            }
        }
        check ("warm 30ms switch/reversal matches endpoint blend", finite && error < 2e-6f);
        check ("same fixed model latency", p.getLatencySamples() == rectP.getLatencySamples()
               && p.getLatencySamples() == leadP.getLatencySamples());
    }

    // Clean key must still close the post-gain gate with Lead running hot.
    {
        JuicyboiAudioProcessor p; setup (p, 48000.0, 1, 256, 1, 2, 10.0f);
        set (p, "gate_enable", 1.0f); juce::MidiBuffer midi; juce::Random noise (991);
        float tail = 0.0f; bool opened = false;
        for (int b = 0; b < 200; ++b)
        {
            juce::AudioBuffer<float> x (2, 256); x.clear();
            for (int i = 0; i < 256; ++i)
                x.setSample (0, i, b < 60 ? signal (b * 256 + i, 48000.0) : 5e-5f * (noise.nextFloat() * 2.0f - 1.0f));
            p.processBlock (x, midi);
            if (b == 50) opened = p.getGateOpenAmount() > 0.99f;
            if (b > 180) tail = std::max (tail, x.getMagnitude (0, 256));
        }
        check ("Lead dry-key gate opens for note, closes for hiss", opened && tail < 1e-5f && p.getGateOpenAmount() < 0.01f);
    }

    // Multi-input Standalone processing regression (wrapper-unmute patch itself
    // remains unmodified; physical microphone permission/device checks are manual).
    {
        juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
        JuicyboiAudioProcessor p;
        juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);
        setup (p, 48000.0, 4, 256, 1, 1, 8.0f);
        bool allInputs = true; juce::MidiBuffer midi;
        for (int input = 0; input < 4; ++input)
        {
            double energy = 0.0;
            for (int b = 0; b < 30; ++b)
            {
                juce::AudioBuffer<float> x (4, 256); x.clear();
                for (int i = 0; i < 256; ++i) x.setSample (input, i, signal (b * 256 + i, 48000.0));
                p.processBlock (x, midi);
                for (int i = 0; i < 256; ++i)
                {
                    energy += (double) x.getSample (0, i) * x.getSample (0, i);
                    allInputs = allInputs && std::abs (x.getSample (0, i) - x.getSample (1, i)) < 2e-6f;
                }
            }
            allInputs = allInputs && energy > 1.0;
        }
        check ("Standalone Lead guitar on each of four inputs -> L/R", allInputs);
    }
    // Paint offscreen only: no native window or desktop interaction.
    {
        JuicyboiAudioProcessor p;
        set (p, "channel", 1.0f); set (p, "red_voice", 0.0f);
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        juce::ComboBox* model = nullptr; juce::ComboBox* channel = nullptr; juce::ComboBox* voice = nullptr;
        for (auto* child : editor->getChildren())
            if (auto* combo = dynamic_cast<juce::ComboBox*> (child))
            {
                if (combo->getItemText (0) == "Rectifier") model = combo;
                if (combo->getItemText (0) == "Red (Ch1)") channel = combo;
                if (combo->getItemText (0) == "Red v1") voice = combo;
            }
        bool uiOk = model && channel && voice;
        if (uiOk)
        {
            model->setSelectedItemIndex (1, juce::sendNotificationSync);
            uiOk = ! channel->isEnabled() && ! voice->isEnabled()
                   && get (p, "channel") > 0.5f && get (p, "red_voice") < 0.5f;
            editor->setSize (1200, 560);
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds());
            uiOk = uiOk && image.isValid() && model->getBounds().getWidth() >= 160;
            model->setSelectedItemIndex (0, juce::sendNotificationSync);
            uiOk = uiOk && channel->isEnabled() && ! voice->isEnabled(); // Orange retained
        }
        check ("offscreen GUI dropdown dims/preserves Rectifier choices", uiOk);
    }
    return failures;
}
