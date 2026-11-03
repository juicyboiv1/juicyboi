#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

/** Minimal V1 editor: rotary knobs + toggles attached to the APVTS.
    Functional, not pretty — a custom LookAndFeel comes later. */
class JuicyboiAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit JuicyboiAudioProcessorEditor (JuicyboiAudioProcessor&);
    ~JuicyboiAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const char* paramId, const juce::String& text);

    JuicyboiAudioProcessor& processor;

    Knob input, odDrive, odTone, odLevel, gain, bass, mid, treble, presence, master;

    juce::ToggleButton odEnable { "Boost" }, bypass { "Bypass" };
    std::unique_ptr<APVTS::ButtonAttachment> odEnableAttachment, bypassAttachment;

    juce::ComboBox channel;
    juce::Label channelLabel;
    std::unique_ptr<APVTS::ComboBoxAttachment> channelAttachment;

    juce::GroupComponent inGroup { {}, "Input" }, odGroup { {}, "Boost" },
                         preGroup { {}, "Preamp" }, outGroup { {}, "Output" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuicyboiAudioProcessorEditor)
};
