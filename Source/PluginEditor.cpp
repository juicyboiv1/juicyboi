#include "PluginEditor.h"

using namespace jb;

JuicyboiAudioProcessorEditor::JuicyboiAudioProcessorEditor (JuicyboiAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    for (auto* g : { &inGroup, &odGroup, &preGroup, &outGroup })
        addAndMakeVisible (*g);

    setupKnob (input,    params::id::inputTrim, "Input");
    setupKnob (odDrive,  params::id::odDrive,   "Drive");
    setupKnob (odTone,   params::id::odTone,    "Tone");
    setupKnob (odLevel,  params::id::odLevel,   "Level");
    setupKnob (gain,     params::id::gain,      "Gain");
    setupKnob (bass,     params::id::bass,      "Bass");
    setupKnob (mid,      params::id::mid,       "Mid");
    setupKnob (treble,   params::id::treble,    "Treble");
    setupKnob (presence, params::id::presence,  "Presence");
    setupKnob (master,   params::id::master,    "Master");

    addAndMakeVisible (odEnable);
    odEnableAttachment = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, params::id::odEnable, odEnable);

    addAndMakeVisible (bypass);
    bypassAttachment = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, params::id::bypass, bypass);

    channel.addItemList ({ "Riff A", "Riff B" }, 1);
    addAndMakeVisible (channel);
    channelAttachment = std::make_unique<APVTS::ComboBoxAttachment> (processor.apvts, params::id::channel, channel);

    channelLabel.setText ("Channel", juce::dontSendNotification);
    channelLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (channelLabel);

    setResizable (true, true);
    setResizeLimits (820, 300, 1600, 600);
    setSize (980, 320);
}

void JuicyboiAudioProcessorEditor::setupKnob (Knob& k, const char* paramId, const juce::String& text)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    addAndMakeVisible (k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<APVTS::SliderAttachment> (processor.apvts, paramId, k.slider);
}

void JuicyboiAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1b1f));

    auto header = getLocalBounds().removeFromTop (40);
    g.setColour (juce::Colour (0xffff8a3d));
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("juicyboi", header.reduced (14, 0), juce::Justification::centredLeft);

    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("heavy riff preamp  |  v0.1 scaffold", header.reduced (14, 0), juce::Justification::centredRight);
}

void JuicyboiAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (36);

    const auto unit = area.getWidth() / 12; // 12 columns: 1 in, 3 boost, 6 pre, 2 out

    auto placeKnob = [] (Knob& k, juce::Rectangle<int> r)
    {
        k.label.setBounds (r.removeFromTop (18));
        k.slider.setBounds (r.reduced (2));
    };

    auto layoutGroup = [] (juce::GroupComponent& g, juce::Rectangle<int> r)
    {
        g.setBounds (r);
        return r.reduced (8, 18);
    };

    // Input
    {
        auto r = layoutGroup (inGroup, area.removeFromLeft (unit));
        placeKnob (input, r);
    }

    // Boost (OD-style)
    {
        auto r = layoutGroup (odGroup, area.removeFromLeft (unit * 3));
        odEnable.setBounds (r.removeFromBottom (24).withSizeKeepingCentre (90, 24));
        const auto w = r.getWidth() / 3;
        placeKnob (odDrive, r.removeFromLeft (w));
        placeKnob (odTone,  r.removeFromLeft (w));
        placeKnob (odLevel, r);
    }

    // Output (laid out from the right so preamp gets the rest)
    auto outArea = area.removeFromRight (unit * 2);

    // Preamp
    {
        auto r = layoutGroup (preGroup, area);
        auto chRow = r.removeFromBottom (26);
        channelLabel.setBounds (chRow.removeFromLeft (70));
        channel.setBounds (chRow.removeFromLeft (120).reduced (2));

        const auto w = r.getWidth() / 5;
        placeKnob (gain,     r.removeFromLeft (w));
        placeKnob (bass,     r.removeFromLeft (w));
        placeKnob (mid,      r.removeFromLeft (w));
        placeKnob (treble,   r.removeFromLeft (w));
        placeKnob (presence, r);
    }

    {
        auto r = layoutGroup (outGroup, outArea);
        bypass.setBounds (r.removeFromBottom (24).withSizeKeepingCentre (90, 24));
        placeKnob (master, r);
    }
}
