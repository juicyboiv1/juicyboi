#include "PluginEditor.h"

using namespace juce;

JuicyboiAudioProcessorEditor::JuicyboiAudioProcessorEditor (JuicyboiAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), head (p, &root), drawer (p, &root)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (root);
    root.addAndMakeVisible (head);
    root.addAndMakeVisible (drawer);
    head.setBounds (0, 0, jb::gui::AmpHead::designWidth, jb::gui::AmpHead::designHeight);
    drawer.onToggle = [this] { setDrawerOpen (! drawerOpen); };

    drawerOpen = processor.editorDrawerOpen;
    drawer.setOpen (drawerOpen);
    drawer.setBounds (0, drawerTop, designWidth, drawerOpen ? jb::gui::Pedalboard::openHeight : jb::gui::Pedalboard::frontHeight);

    // read the remembered width first: applying the limits resizes (and re-saves the width)
    const auto w = jlimit (roundToInt (designWidth * minScale), roundToInt (designWidth * maxScale), processor.editorWidth);
    setResizable (true, true);
    applyConstraints();
    setSize (w, roundToInt ((double) w * getDesignHeight() / designWidth));
}

JuicyboiAudioProcessorEditor::~JuicyboiAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void JuicyboiAudioProcessorEditor::applyConstraints()
{
    const auto h = (double) getDesignHeight();
    setResizeLimits (roundToInt (designWidth * minScale), roundToInt (h * minScale),
                     roundToInt (designWidth * maxScale), roundToInt (h * maxScale));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) designWidth / h);
}

void JuicyboiAudioProcessorEditor::setDrawerOpen (bool shouldBeOpen)
{
    if (drawerOpen == shouldBeOpen)
        return;
    drawerOpen = shouldBeOpen;
    processor.editorDrawerOpen = shouldBeOpen;
    drawer.setOpen (shouldBeOpen);
    drawer.setBounds (0, drawerTop, designWidth, shouldBeOpen ? jb::gui::Pedalboard::openHeight : jb::gui::Pedalboard::frontHeight);
    const auto w = getWidth();
    applyConstraints();
    setSize (w, roundToInt ((double) w * getDesignHeight() / designWidth));
}

void JuicyboiAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff0b0b0b));
}

void JuicyboiAudioProcessorEditor::resized()
{
    const auto scale = (float) getWidth() / (float) designWidth;
    root.setBounds (0, 0, designWidth, getDesignHeight());
    root.setTransform (AffineTransform::scale (scale));
    if (getWidth() > 0)
        processor.editorWidth = getWidth();
}

void JuicyboiAudioProcessorEditor::Root::paint (Graphics& g)
{
    ColourGradient bg (Colour (0xff1b1b1b), 0, 0, Colour (0xff070707), 0, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();
    // rubber feet between head and drawer
    for (auto x : { 64.0f, 976.0f })
    {
        const auto r = Rectangle<float> (x, (float) jb::gui::AmpHead::designHeight - 6.0f, 60.0f, 12.0f);
        ColourGradient fg (Colour (0xff2a2a2a), r.getX(), r.getY(), Colour (0xff050505), r.getX(), r.getBottom(), false);
        g.setGradientFill (fg);
        g.fillRoundedRectangle (r, 4.0f);
    }
}
