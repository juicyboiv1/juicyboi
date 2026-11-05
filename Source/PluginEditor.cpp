#include "PluginEditor.h"

using namespace jb;

JuicyboiAudioProcessorEditor::JuicyboiAudioProcessorEditor (JuicyboiAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* g : { &inGroup, &odGroup, &preGroup, &outGroup, &cabGroup, &gateGroup })
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

    // Red v1 / v2 voicing switch (only meaningful on Red; dimmed on Orange)
    redVoice.addItemList ({ "Red v1", "Red v2" }, 1);
    redVoice.setTooltip ("A/B the Red channel: v1 = original tight Red, v2 = max-gain / fat-bottom Red");
    addAndMakeVisible (redVoice);
    redVoiceAttachment = std::make_unique<APVTS::ComboBoxAttachment> (processor.apvts, params::id::redVoice, redVoice);

    redVoiceLabel.setText ("Voice", juce::dontSendNotification);
    redVoiceLabel.setJustificationType (juce::Justification::centredRight);
    redVoiceLabel.setColour (juce::Label::textColourId, juce::Colour (0xffd4af37));
    addAndMakeVisible (redVoiceLabel);

    channel.onChange = [this] { updateRedVoiceEnablement(); };
    updateRedVoiceEnablement();

    // Cab section
    setupKnob (cabBlend, params::id::cabMicBlend, "Mic Blend");
    setupKnob (cabLevel, params::id::cabLevel,    "Cab Level");
    addAndMakeVisible (cabEnable);
    cabEnableAttachment = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, params::id::cabEnable, cabEnable);

    for (auto* cb : { &sm57Axis, &md421Axis })
    {
        cb->addItemList ({ "On-axis", "Off-axis" }, 1);
        addAndMakeVisible (*cb);
    }
    sm57AxisAttachment  = std::make_unique<APVTS::ComboBoxAttachment> (processor.apvts, params::id::cabSm57Axis,  sm57Axis);
    md421AxisAttachment = std::make_unique<APVTS::ComboBoxAttachment> (processor.apvts, params::id::cabMd421Axis, md421Axis);

    sm57Label .setText ("SM57",  juce::dontSendNotification);
    md421Label.setText ("MD421", juce::dontSendNotification);
    for (auto* l : { &sm57Label, &md421Label }) addAndMakeVisible (*l);

    cabHint.setText ("Cab off = hard bypass (for analog power amp). No IRs loaded = passthrough.",
                     juce::dontSendNotification);
    cabHint.setFont (juce::FontOptions (11.0f));
    cabHint.setColour (juce::Label::textColourId, juce::Colour (0xff7a9a5c));
    addAndMakeVisible (cabHint);

    for (int sl = 0; sl < dsp::CabSim::numSlots; ++sl)
    {
        irZones[(size_t) sl] = std::make_unique<IRDropZone> (processor, sl);
        addAndMakeVisible (*irZones[(size_t) sl]);
    }

    // Gate section (key = dry input, VCA after cab)
    setupKnob (gateThreshold, params::id::gateThreshold, "Threshold");
    setupKnob (gateHold,      params::id::gateHold,      "Hold");
    setupKnob (gateRelease,   params::id::gateRelease,   "Release");
    addAndMakeVisible (gateEnable);
    gateEnableAttachment = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, params::id::gateEnable, gateEnable);
    addAndMakeVisible (gateLed);

    setResizable (true, true);
    setResizeLimits (1040, 500, 1800, 900);
    setSize (1180, 540);
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
    g.drawText ("juicyboi v2", header.reduced (16, 0), juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff7a9a5c));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("heavy riff preamp  |  v0.1 scaffold", header.reduced (16, 0), juce::Justification::centredRight);

    g.setColour (juce::Colour (0xffd4af37).withAlpha (0.35f));
    g.fillRect (0, header.getBottom() - 1, getWidth(), 1);
}

juce::Rectangle<int> JuicyboiAudioProcessorEditor::layoutCab (juce::Rectangle<int> bounds)
{
    cabGroup.setBounds (bounds);
    auto r = bounds.reduced (10, 20);
    cabHint.setBounds (r.removeFromBottom (16));

    auto controls = r.removeFromLeft (190);
    cabEnable.setBounds (controls.removeFromTop (28).withWidth (90));
    controls.removeFromTop (6);
    auto row1 = controls.removeFromTop (26);
    sm57Label.setBounds (row1.removeFromLeft (56));
    sm57Axis.setBounds (row1.reduced (2));
    controls.removeFromTop (4);
    auto row2 = controls.removeFromTop (26);
    md421Label.setBounds (row2.removeFromLeft (56));
    md421Axis.setBounds (row2.reduced (2));

    auto knobs = r.removeFromLeft (200);
    auto place = [] (Knob& k, juce::Rectangle<int> kr)
    {
        k.label.setBounds (kr.removeFromTop (20));
        k.slider.setBounds (kr.reduced (4));
    };
    place (cabBlend, knobs.removeFromLeft (100));
    place (cabLevel, knobs);

    r.removeFromLeft (8);
    auto top = r.removeFromTop (r.getHeight() / 2);
    const auto w = r.getWidth() / 2;
    irZones[0]->setBounds (top.removeFromLeft (w).reduced (3));
    irZones[1]->setBounds (top.reduced (3));
    irZones[2]->setBounds (r.removeFromLeft (w).reduced (3));
    irZones[3]->setBounds (r.reduced (3));
    return bounds;
}

void JuicyboiAudioProcessorEditor::layoutGate (juce::Rectangle<int> bounds)
{
    gateGroup.setBounds (bounds);
    auto r = bounds.reduced (10, 20);

    auto top = r.removeFromTop (28);
    gateEnable.setBounds (top.removeFromLeft (80));
    gateLed.setBounds (top.removeFromLeft (22).withSizeKeepingCentre (16, 16));
    r.removeFromTop (4);

    auto place = [] (Knob& k, juce::Rectangle<int> kr)
    {
        k.label.setBounds (kr.removeFromTop (20));
        k.slider.setBounds (kr.reduced (4));
    };
    const auto w = r.getWidth() / 3;
    place (gateThreshold, r.removeFromLeft (w));
    place (gateHold,      r.removeFromLeft (w));
    place (gateRelease,   r);
}

void JuicyboiAudioProcessorEditor::GateLed::timerCallback()
{
    const auto nowOn = proc.isGateActive()
                       && proc.apvts.getRawParameterValue (jb::params::id::gateEnable)->load() > 0.5f;
    const auto amt = nowOn ? proc.getGateOpenAmount() : 0.0f;
    if (nowOn != on || std::abs (amt - shown) > 0.02f)
    {
        on = nowOn;
        shown = amt;
        repaint();
    }
}

void JuicyboiAudioProcessorEditor::GateLed::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.5f);
    const auto closed = juce::Colour (0xffe0542a); // orange-red
    const auto open   = juce::Colour (0xff4cd964); // green
    const auto c = on ? closed.interpolatedWith (open, juce::jlimit (0.0f, 1.0f, shown))
                      : juce::Colour (0xff2a3d20);

    if (on)
    {
        g.setColour (c.withAlpha (0.35f));
        g.fillEllipse (b.expanded (1.5f));
    }
    g.setColour (c);
    g.fillEllipse (b);
    g.setColour (juce::Colour (0xffd4af37).withAlpha (0.7f));
    g.drawEllipse (b, 1.0f);
}

//==============================================================================
JuicyboiAudioProcessorEditor::IRDropZone::IRDropZone (JuicyboiAudioProcessor& p, int s) : proc (p), slot (s)
{
    clearButton.setTooltip ("Clear IR");
    clearButton.onClick = [this] { proc.clearCabIR (slot); refresh(); };
    addAndMakeVisible (clearButton);
    refresh();
}

void JuicyboiAudioProcessorEditor::IRDropZone::refresh()
{
    clearButton.setVisible (proc.hasCabIR (slot));
    repaint();
}

void JuicyboiAudioProcessorEditor::IRDropZone::resized()
{
    clearButton.setBounds (getLocalBounds().removeFromRight (24).withSizeKeepingCentre (20, 20));
}

bool JuicyboiAudioProcessorEditor::IRDropZone::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac"))
            return true;
    return false;
}

void JuicyboiAudioProcessorEditor::IRDropZone::filesDropped (const juce::StringArray& files, int, int)
{
    hovering = false;
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac"))
        {
            proc.loadCabIR (slot, juce::File (f));
            break;
        }
    refresh();
}

void JuicyboiAudioProcessorEditor::IRDropZone::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const auto loaded = proc.hasCabIR (slot);
    g.setColour (hovering ? juce::Colour (0xff2f4a22) : juce::Colour (0xff1a2214));
    g.fillRoundedRectangle (b, 5.0f);

    juce::Path border;
    border.addRoundedRectangle (b, 5.0f);
    if (loaded)
    {
        g.setColour (juce::Colour (0xffd4af37));
        g.strokePath (border, juce::PathStrokeType (1.5f));
    }
    else
    {
        juce::Path dashed;
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, border, dashes, 2);
        g.setColour (juce::Colour (0xff5a7a44));
        g.fillPath (dashed);
    }

    auto t = getLocalBounds().reduced (8, 2).withTrimmedRight (22);
    g.setColour (juce::Colour (0xffd4af37));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (jb::dsp::CabSim::slotName (slot), t.removeFromTop (t.getHeight() / 2), juce::Justification::bottomLeft);

    g.setColour (loaded ? juce::Colour (0xffe8e0c8) : juce::Colour (0xff7a9a5c));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (loaded ? proc.getCabIRFile (slot).getFileName() : juce::String ("Drop IR..."),
                t, juce::Justification::topLeft, true);
}

void JuicyboiAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (40);

    // Cab row along the bottom
    {
        auto bottom = area.removeFromBottom (juce::jmax (170, area.getHeight() * 2 / 5));
        layoutGate (bottom.removeFromLeft (juce::jmax (270, bottom.getWidth() / 4)));
        bottom.removeFromLeft (6);
        auto r = layoutCab (bottom);
        juce::ignoreUnused (r);
    }

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
        redVoiceLabel.setBounds (chRow.removeFromLeft (56));
        redVoice.setBounds (chRow.removeFromLeft (96).reduced (2));

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

void JuicyboiAudioProcessorEditor::updateRedVoiceEnablement()
{
    const auto isRed = channel.getSelectedItemIndex() <= 0;
    redVoice.setEnabled (isRed);
    redVoice.setAlpha (isRed ? 1.0f : 0.45f);
    redVoiceLabel.setAlpha (isRed ? 1.0f : 0.45f);
}
