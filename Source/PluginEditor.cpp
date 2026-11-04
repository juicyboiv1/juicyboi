#include "PluginEditor.h"

using namespace jb;

JuicyboiAudioProcessorEditor::JuicyboiAudioProcessorEditor (JuicyboiAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

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

    channel.addItemList ({ "Red (Ch1)", "Orange (Ch2)" }, 1);
    addAndMakeVisible (channel);
    channelAttachment = std::make_unique<APVTS::ComboBoxAttachment> (processor.apvts, params::id::channel, channel);

    channelLabel.setText ("Channel", juce::dontSendNotification);
    channelLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (channelLabel);

    setResizable (true, true);
    setResizeLimits (900, 340, 1600, 640);
    setSize (1040, 360);
}

JuicyboiAudioProcessorEditor::~JuicyboiAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void JuicyboiAudioProcessorEditor::setupKnob (Knob& k, const char* paramId, const juce::String& text)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 20);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (0xffd4af37));
    k.slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2a3d20));
    k.slider.setColour (juce::Slider::thumbColourId,               juce::Colour (0xfff5e6a3));
    addAndMakeVisible (k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, juce::Colour (0xffe8e0c8));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<APVTS::SliderAttachment> (processor.apvts, paramId, k.slider);
}

void JuicyboiAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff12160f));

    auto header = getLocalBounds().removeFromTop (44);
    juce::ColourGradient headerWash (juce::Colour (0xff1e2a16), 0.0f, 0.0f,
                                     juce::Colour (0xff12160f), 0.0f, (float) header.getHeight(), false);
    g.setGradientFill (headerWash);
    g.fillRect (header);

    g.setColour (juce::Colour (0xffd4af37));
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("juicyboi v1", header.reduced (16, 0), juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff7a9a5c));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("heavy riff preamp  |  v0.1 scaffold", header.reduced (16, 0), juce::Justification::centredRight);

    g.setColour (juce::Colour (0xffd4af37).withAlpha (0.35f));
    g.fillRect (0, header.getBottom() - 1, getWidth(), 1);
}

void JuicyboiAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (40);

    const auto unit = area.getWidth() / 12; // 12 columns: 1 in, 3 boost, 6 pre, 2 out

    auto placeKnob = [] (Knob& k, juce::Rectangle<int> r)
    {
        k.label.setBounds (r.removeFromTop (20));
        k.slider.setBounds (r.reduced (4));
    };

    auto layoutGroup = [] (juce::GroupComponent& g, juce::Rectangle<int> r)
    {
        g.setBounds (r);
        return r.reduced (10, 20);
    };

    // Input
    {
        auto r = layoutGroup (inGroup, area.removeFromLeft (unit));
        placeKnob (input, r);
    }

    // Boost (OD-style)
    {
        auto r = layoutGroup (odGroup, area.removeFromLeft (unit * 3));
        odEnable.setBounds (r.removeFromBottom (26).withSizeKeepingCentre (100, 26));
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
        auto chRow = r.removeFromBottom (28);
        channelLabel.setBounds (chRow.removeFromLeft (72));
        channel.setBounds (chRow.removeFromLeft (130).reduced (2));

        const auto w = r.getWidth() / 5;
        placeKnob (gain,     r.removeFromLeft (w));
        placeKnob (bass,     r.removeFromLeft (w));
        placeKnob (mid,      r.removeFromLeft (w));
        placeKnob (treble,   r.removeFromLeft (w));
        placeKnob (presence, r);
    }

    {
        auto r = layoutGroup (outGroup, outArea);
        bypass.setBounds (r.removeFromBottom (26).withSizeKeepingCentre (100, 26));
        placeKnob (master, r);
    }
}
