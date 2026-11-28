#include "PitchPedal.h"

namespace jb::gui
{
using namespace juce;

ModeButton::ModeButton (AudioProcessorValueTreeState& s, const char* id, String lbl, String hlp, Colour lit)
    : param (*s.getParameter (id)), label (std::move (lbl)), help (std::move (hlp)), litColour (lit),
      attachment (param, [this] (float v) { value = v; repaint(); if (onChange) onChange(); }, nullptr)
{
    setComponentID (id);
    setName (param.getName (64));
    setMouseCursor (MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

String ModeButton::getTooltip()
{
    auto t = param.getName (64) + (isOn() ? ": ON" : ": OFF") + "  (click to toggle)";
    if (help.isNotEmpty())
        t << "\n" << help;
    return t;
}

void ModeButton::mouseDown (const MouseEvent&)
{
    if (isEnabled())
        flip();
}

void ModeButton::flip()
{
    attachment.setValueAsCompleteGesture (isOn() ? 0.0f : 1.0f);
}

void ModeButton::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    const auto on = isOn();
    // recessed black cap with a chrome rim
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (b.translated (0.0f, 1.0f), 3.5f);
    ColourGradient cap (Colour (on ? 0xff2a2a2a : 0xff1a1a1a), 0, b.getY(), Colour (0xff070707), 0, b.getBottom(), false);
    g.setGradientFill (cap);
    g.fillRoundedRectangle (b, 3.5f);
    g.setColour (colours::chromeMid.withAlpha (0.55f));
    g.drawRoundedRectangle (b, 3.5f, 0.8f);

    const auto ledR = 2.6f;
    drawLed (g, { b.getX() + 7.0f, b.getCentreY() }, ledR, litColour, on ? 1.0f : 0.0f);

    g.setColour (on ? colours::creme : colours::creme.withAlpha (0.6f));
    g.setFont (uiFont (8.5f));
    g.drawText (label, b.withTrimmedLeft (12.0f).withTrimmedRight (1.0f), Justification::centred, false);
}
} // namespace jb::gui
