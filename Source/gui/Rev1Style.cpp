#include "Rev1Style.h"

namespace jb::gui
{
using namespace juce;

Font uiFont (float size, bool bold)
{
    return Font (FontOptions (size, bold ? Font::bold : Font::plain));
}

void fillChrome (Graphics& g, const Path& p, Rectangle<float> a, bool vertical)
{
    ColourGradient cg (colours::chromeHi, a.getX(), a.getY(),
                       colours::chromeLo, vertical ? a.getX() : a.getRight(), vertical ? a.getBottom() : a.getY(), false);
    cg.addColour (0.45, colours::chromeMid);
    cg.addColour (0.55, Colour (0xffe2e2de));
    g.setGradientFill (cg);
    g.fillPath (p);
}

void drawChromeScrew (Graphics& g, Point<float> c, float r)
{
    Path p; p.addEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    fillChrome (g, p, { c.x - r, c.y - r, 2 * r, 2 * r });
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 0.8f);
    g.drawLine (c.x - r * 0.6f, c.y - r * 0.6f, c.x + r * 0.6f, c.y + r * 0.6f, juce::jmax (0.8f, r * 0.25f));
}

void drawLed (Graphics& g, Point<float> c, float r, Colour lit, float amount)
{
    amount = jlimit (0.0f, 1.0f, amount);
    if (amount > 0.0f)
    {
        ColourGradient glow (lit.withAlpha (0.55f * amount), c.x, c.y, lit.withAlpha (0.0f), c.x + r * 3.2f, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (c.x - r * 3.2f, c.y - r * 3.2f, r * 6.4f, r * 6.4f);
    }
    // bezel
    g.setColour (Colour (0xff0a0a0a));
    g.fillEllipse (c.x - r * 1.35f, c.y - r * 1.35f, r * 2.7f, r * 2.7f);
    const auto body = colours::ledOff.interpolatedWith (lit, amount);
    ColourGradient lens (body.brighter (0.6f * amount + 0.1f), c.x - r * 0.4f, c.y - r * 0.5f, body.darker (0.4f), c.x + r, c.y + r, true);
    g.setGradientFill (lens);
    g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    g.setColour (Colours::white.withAlpha (0.35f + 0.3f * amount));
    g.fillEllipse (c.x - r * 0.5f, c.y - r * 0.62f, r * 0.55f, r * 0.4f);
}

//==============================================================================
Rev1LookAndFeel::Rev1LookAndFeel()
{
    setColour (TooltipWindow::backgroundColourId, Colour (0xf0161616));
    setColour (TooltipWindow::textColourId, colours::creme);
    setColour (TooltipWindow::outlineColourId, colours::cremeDim.withAlpha (0.6f));
    setColour (BubbleComponent::backgroundColourId, Colour (0xf0161616));
    setColour (BubbleComponent::outlineColourId, colours::cremeDim);
    setColour (TextButton::buttonColourId, Colour (0xff222222));
    setColour (TextButton::textColourOffId, colours::creme);
}

static Point<float> polar (Point<float> c, float r, float angle)
{
    return { c.x + r * std::sin (angle), c.y - r * std::cos (angle) };
}

void Rev1LookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, Slider& s)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = bounds.getCentre();
    const auto R = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto angle = startAngle + pos * (endAngle - startAngle);
    const bool pedal = s.getProperties()["jbStyle"].toString() == "pedal";
    const auto tickColour = Colour::fromString (s.getProperties().getWithDefault ("jbTick", colours::creme.toString()).toString());

    // Scale dots 0..10 around the knob
    for (int i = 0; i <= 10; ++i)
    {
        const auto a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const auto p = polar (c, R - 2.2f, a);
        const auto dr = (i == 0 || i == 5 || i == 10) ? 1.7f : 1.15f;
        g.setColour (tickColour.withAlpha (0.85f));
        g.fillEllipse (p.x - dr, p.y - dr, 2 * dr, 2 * dr);
    }

    const auto r = R * 0.80f;
    // drop shadow
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - r * 1.04f, c.y - r * 0.98f, r * 2.08f, r * 2.16f);

    if (! pedal)
    {
        // Chrome skirt
        {
            Path skirt; skirt.addEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
            ColourGradient cg (colours::chromeHi, c.x - r * 0.7f, c.y - r * 0.8f, colours::chromeLo, c.x + r * 0.7f, c.y + r, false);
            cg.addColour (0.5, colours::chromeMid);
            cg.addColour (0.62, Colour (0xffdcdcd6));
            g.setGradientFill (cg);
            g.fillPath (skirt);
            g.setColour (Colours::black.withAlpha (0.18f));
            for (int i = 0; i < 48; ++i)
            {
                const auto a = (float) i / 48.0f * MathConstants<float>::twoPi;
                g.drawLine (Line<float> (polar (c, r * 0.82f, a), polar (c, r * 0.985f, a)), 0.7f);
            }
            g.setColour (Colours::black.withAlpha (0.6f));
            g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 0.8f);
            // engraved skirt pointer
            g.setColour (Colour (0xff1a1a1a));
            g.drawLine (Line<float> (polar (c, r * 0.68f, angle), polar (c, r * 0.97f, angle)), jmax (1.4f, r * 0.07f));
        }
        // Black cap
        const auto cr = r * 0.63f;
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - cr * 1.06f, c.y - cr * 1.0f, cr * 2.12f, cr * 2.16f);
        ColourGradient cap (Colour (0xff3e3e3e), c.x, c.y - cr, Colour (0xff060606), c.x, c.y + cr, false);
        g.setGradientFill (cap);
        g.fillEllipse (c.x - cr, c.y - cr, 2 * cr, 2 * cr);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.fillEllipse (c.x - cr * 0.7f, c.y - cr * 0.9f, cr * 1.4f, cr * 0.8f);
        g.setColour (colours::creme);
        Path ptr;
        ptr.startNewSubPath (polar (c, cr * 0.2f, angle));
        ptr.lineTo (polar (c, cr * 0.92f, angle));
        g.strokePath (ptr, PathStrokeType (jmax (2.0f, r * 0.085f), PathStrokeType::curved, PathStrokeType::rounded));
    }
    else
    {
        // Ribbed black pedal knob
        ColourGradient body (Colour (0xff353535), c.x, c.y - r, Colour (0xff050505), c.x, c.y + r, false);
        g.setGradientFill (body);
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        for (int i = 0; i < 36; ++i)
        {
            const auto a = (float) i / 36.0f * MathConstants<float>::twoPi;
            g.setColour ((i & 1) ? Colours::white.withAlpha (0.10f) : Colours::black.withAlpha (0.5f));
            g.drawLine (Line<float> (polar (c, r * 0.80f, a), polar (c, r * 0.99f, a)), 1.0f);
        }
        const auto tr = r * 0.76f;
        ColourGradient top (Colour (0xff2c2c2c), c.x, c.y - tr, Colour (0xff0c0c0c), c.x, c.y + tr, false);
        g.setGradientFill (top);
        g.fillEllipse (c.x - tr, c.y - tr, 2 * tr, 2 * tr);
        g.setColour (Colours::white.withAlpha (0.08f));
        g.fillEllipse (c.x - tr * 0.7f, c.y - tr * 0.92f, tr * 1.4f, tr * 0.7f);
        g.setColour (Colours::white.withAlpha (0.92f));
        Path ptr;
        ptr.startNewSubPath (polar (c, r * 0.22f, angle));
        ptr.lineTo (polar (c, r * 0.95f, angle));
        g.strokePath (ptr, PathStrokeType (jmax (2.0f, r * 0.1f), PathStrokeType::curved, PathStrokeType::rounded));
    }

    if (! s.isEnabled())
    {
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    }
}

void Rev1LookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                        Slider::SliderStyle style, Slider& s)
{
    if (style != Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }
    const auto cx = (float) x + (float) w * 0.5f;
    const auto top = (float) y, bottom = (float) (y + h);
    // tick marks (+15 .. -15 dB in 5 dB steps)
    for (int i = 0; i <= 6; ++i)
    {
        const auto ty = top + (bottom - top) * (float) i / 6.0f;
        const auto len = (i == 3) ? 7.0f : 4.0f;
        g.setColour (colours::ink.withAlpha (i == 3 ? 0.8f : 0.45f));
        g.fillRect (cx - 6.0f - len, ty - 0.6f, len, 1.2f);
        g.fillRect (cx + 6.0f, ty - 0.6f, len, 1.2f);
    }
    // slot
    g.setColour (Colour (0xff111111));
    g.fillRoundedRectangle (cx - 2.5f, top - 4.0f, 5.0f, bottom - top + 8.0f, 2.5f);
    g.setColour (Colours::white.withAlpha (0.35f));
    g.fillRect (cx + 2.5f, top - 3.0f, 0.8f, bottom - top + 6.0f);

    // cap
    const auto capW = jmin ((float) w - 2.0f, 20.0f), capH = 12.0f;
    auto cap = Rectangle<float> (capW, capH).withCentre ({ cx, pos });
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (cap.translated (0.0f, 1.5f), 2.5f);
    ColourGradient cg (Colour (0xff3a3a3a), cap.getX(), cap.getY(), Colour (0xff070707), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (cg);
    g.fillRoundedRectangle (cap, 2.5f);
    g.setColour (colours::creme);
    g.fillRect (cap.getX() + 2.0f, cap.getCentreY() - 0.8f, cap.getWidth() - 4.0f, 1.6f);
}

Rectangle<int> Rev1LookAndFeel::getTooltipBounds (const String& tip, Point<int> pos, Rectangle<int> parentArea)
{
    const auto f = uiFont (13.0f, false);
    const int maxW = 360;
    AttributedString s; s.append (tip, f, colours::creme);
    TextLayout tl; tl.createLayout (s, (float) maxW);
    const auto w = (int) std::ceil (tl.getWidth()) + 16, h = (int) std::ceil (tl.getHeight()) + 12;
    return Rectangle<int> (pos.x > parentArea.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                           pos.y > parentArea.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

void Rev1LookAndFeel::drawTooltip (Graphics& g, const String& text, int w, int h)
{
    const auto b = Rectangle<float> ((float) w, (float) h);
    g.setColour (findColour (TooltipWindow::backgroundColourId));
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (findColour (TooltipWindow::outlineColourId));
    g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
    AttributedString s; s.append (text, uiFont (13.0f, false), colours::creme);
    TextLayout tl; tl.createLayout (s, (float) w - 16.0f);
    tl.draw (g, b.reduced (8.0f, 6.0f));
}

void Rev1LookAndFeel::drawBubble (Graphics& g, BubbleComponent&, const Point<float>&, const Rectangle<float>& body)
{
    g.setColour (Colour (0xf0161616));
    g.fillRoundedRectangle (body, 4.0f);
    g.setColour (colours::cremeDim);
    g.drawRoundedRectangle (body.reduced (0.5f), 4.0f, 1.0f);
}

//==============================================================================
ParamSlider::ParamSlider (AudioProcessorValueTreeState& s, const char* id, Style style, TipFn tip)
    : param (*s.getParameter (id)), tipFn (std::move (tip))
{
    setComponentID (id);
    setName (param.getName (64));
    setTextBoxStyle (NoTextBox, false, 0, 0);
    setColour (Slider::textBoxTextColourId, colours::creme);
    if (style == Style::fader)
    {
        setSliderStyle (LinearVertical);
    }
    else
    {
        setSliderStyle (RotaryHorizontalVerticalDrag);
        getProperties().set ("jbStyle", style == Style::pedalKnob ? "pedal" : "amp");
    }
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, id, *this);
    setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));
    setMouseCursor (MouseCursor::PointingHandCursor);
}

String ParamSlider::getTooltip()
{
    if (tipFn)
        return tipFn (getValue());
    auto t = param.getName (64) + ": " + param.getCurrentValueAsText();
    if (extraTip.isNotEmpty())
        t << "  " << extraTip;
    return t;
}

//==============================================================================
ToggleSwitch::ToggleSwitch (AudioProcessorValueTreeState& s, const char* id, String one, String zero,
                            bool oneTop, bool horiz, Colour mc)
    : param (*s.getParameter (id)), markOne (std::move (one)), markZero (std::move (zero)),
      oneAtTop (oneTop), horizontal (horiz), markColour (mc),
      attachment (param, [this] (float v) { value = v; repaint(); if (onChange) onChange(); }, nullptr)
{
    setComponentID (id);
    setName (param.getName (64));
    setMouseCursor (MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

String ToggleSwitch::getTooltip()
{
    auto t = param.getName (64) + ": " + param.getCurrentValueAsText() + "  (click to flip)";
    if (extraTip.isNotEmpty())
        t << "\n" << extraTip;
    return t;
}

void ToggleSwitch::mouseDown (const MouseEvent&)
{
    if (isEnabled())
        flip();
}

void ToggleSwitch::flip()
{
    attachment.setValueAsCompleteGesture (isOne() ? 0.0f : 1.0f);
}

void ToggleSwitch::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const auto markH = 12.0f;
    const auto f = uiFont (9.5f);
    g.setFont (f);

    Rectangle<float> body;
    bool towardFirst; // lever points to top (or left)
    if (! horizontal)
    {
        auto topMark = b.removeFromTop (markH);
        auto botMark = b.removeFromBottom (markH);
        const bool oneTop = oneAtTop;
        const auto on = isOne();
        g.setColour (markColour.withAlpha (on == oneTop ? 1.0f : 0.55f));
        g.drawText (oneTop ? markOne : markZero, topMark, Justification::centred, false);
        g.setColour (markColour.withAlpha (on != oneTop ? 1.0f : 0.55f));
        g.drawText (oneTop ? markZero : markOne, botMark, Justification::centred, false);
        body = b;
        towardFirst = (on == oneTop);
    }
    else
    {
        const auto mw = jmax (GlyphArrangement::getStringWidth (f, markOne), GlyphArrangement::getStringWidth (f, markZero)) + 6.0f;
        auto leftMark = b.removeFromLeft (mw);
        auto rightMark = b.removeFromRight (mw);
        const bool oneRight = oneAtTop;
        const auto on = isOne();
        g.setColour (markColour.withAlpha (on != oneRight ? 1.0f : 0.55f));
        g.drawText (oneRight ? markZero : markOne, leftMark, Justification::centred, false);
        g.setColour (markColour.withAlpha (on == oneRight ? 1.0f : 0.55f));
        g.drawText (oneRight ? markOne : markZero, rightMark, Justification::centred, false);
        body = b;
        towardFirst = (on != oneRight);
    }

    const auto c = body.getCentre();
    const auto s = jmin (body.getWidth(), body.getHeight());
    // hex nut + washer
    const auto nutR = s * 0.26f;
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - nutR * 1.35f, c.y - nutR * 1.25f, nutR * 2.7f, nutR * 2.7f);
    Path washer; washer.addEllipse (c.x - nutR * 1.25f, c.y - nutR * 1.25f, nutR * 2.5f, nutR * 2.5f);
    fillChrome (g, washer, washer.getBounds());
    Path hex; hex.addPolygon (c, 6, nutR, MathConstants<float>::pi / 6.0f);
    fillChrome (g, hex, hex.getBounds(), false);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.strokePath (hex, PathStrokeType (0.7f));

    // bat lever
    const auto len = (horizontal ? body.getWidth() : body.getHeight()) * 0.30f;
    const auto dir = towardFirst ? -1.0f : 1.0f;
    const Point<float> tip = horizontal ? Point<float> (c.x + dir * len, c.y - 1.0f) : Point<float> (c.x, c.y + dir * len);
    Path lever;
    const auto baseW = nutR * 0.38f, tipW = nutR * 0.6f;
    if (! horizontal)
    {
        lever.startNewSubPath (c.x - baseW, c.y);
        lever.lineTo (tip.x - tipW * 0.75f, tip.y);
        lever.quadraticTo (tip.x, tip.y + dir * tipW * 0.6f, tip.x + tipW * 0.75f, tip.y);
        lever.lineTo (c.x + baseW, c.y);
    }
    else
    {
        lever.startNewSubPath (c.x, c.y - baseW);
        lever.lineTo (tip.x, tip.y - tipW * 0.75f);
        lever.quadraticTo (tip.x + dir * tipW * 0.6f, tip.y, tip.x, tip.y + tipW * 0.75f);
        lever.lineTo (c.x, c.y + baseW);
    }
    lever.closeSubPath();
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillPath (lever, AffineTransform::translation (1.5f, 2.0f));
    ColourGradient cg (colours::chromeHi, c.x - tipW, c.y, colours::chromeLo, c.x + tipW, c.y, false);
    if (horizontal) cg = ColourGradient (colours::chromeHi, c.x, c.y - tipW, colours::chromeLo, c.x, c.y + tipW, false);
    cg.addColour (0.4, Colour (0xffe9e9e5));
    g.setGradientFill (cg);
    g.fillPath (lever);
    // ball tip
    const auto br = tipW * 0.9f;
    Path ball; ball.addEllipse (tip.x - br, tip.y - br, 2 * br, 2 * br);
    ColourGradient bg (Colours::white, tip.x - br * 0.4f, tip.y - br * 0.5f, colours::chromeLo, tip.x + br, tip.y + br, true);
    g.setGradientFill (bg);
    g.fillPath (ball);

    if (! isEnabled())
    {
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillEllipse (body.withSizeKeepingCentre (s, s));
    }
}

//==============================================================================
Footswitch::Footswitch (AudioProcessorValueTreeState& s, const char* id, String n)
    : param (*s.getParameter (id)), name (std::move (n)),
      attachment (param, [this] (float v) { value = v; repaint(); if (onChange) onChange(); }, nullptr)
{
    setComponentID (id);
    setName (param.getName (64));
    setMouseCursor (MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

String Footswitch::getTooltip()
{
    return name + (isOn() ? ": ON" : ": BYPASSED") + "  (click the footswitch to toggle)";
}

void Footswitch::mouseDown (const MouseEvent&)
{
    pressed = true;
    attachment.setValueAsCompleteGesture (isOn() ? 0.0f : 1.0f);
    repaint();
}

void Footswitch::mouseUp (const MouseEvent&)
{
    pressed = false;
    repaint();
}

void Footswitch::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const auto R = jmin (b.getWidth(), b.getHeight()) * 0.5f - 1.0f;
    // shadow + hex nut base
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - R, c.y - R + 2.0f, 2 * R, 2 * R);
    Path hex; hex.addPolygon (c, 6, R * 0.98f, 0.0f);
    fillChrome (g, hex, hex.getBounds());
    g.setColour (Colours::black.withAlpha (0.4f));
    g.strokePath (hex, PathStrokeType (0.8f));
    // threaded collar
    const auto cr = R * 0.72f;
    Path collar; collar.addEllipse (c.x - cr, c.y - cr, 2 * cr, 2 * cr);
    fillChrome (g, collar, collar.getBounds(), false);
    // domed plunger
    const auto pr = R * (pressed ? 0.50f : 0.54f);
    const auto py = c.y + (pressed ? 1.0f : -1.0f);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (c.x - pr, py - pr + 2.0f, 2 * pr, 2 * pr);
    ColourGradient dome (Colours::white, c.x - pr * 0.4f, py - pr * 0.5f, colours::chromeLo, c.x + pr, py + pr, true);
    dome.addColour (0.5, colours::chromeMid);
    g.setGradientFill (dome);
    g.fillEllipse (c.x - pr, py - pr, 2 * pr, 2 * pr);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawEllipse (c.x - pr, py - pr, 2 * pr, 2 * pr, 0.8f);
}
} // namespace jb::gui
