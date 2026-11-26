#include "TunerPedal.h"
#include <map>
#include "../PluginProcessor.h"
#include "../Parameters.h"

namespace jb::gui
{
using namespace juce;

namespace
{
    const Colour tunerLit    { 0xffff9a2e };   // amber: off pitch
    const Colour tunerInTune { 0xff3dff8a };   // bright green: in tune
    const Colour tunerUnlit  { 0xff1a1d22 };
    const Colour glass       { 0xff07090c };
    const Colour noteWhite   { 0xffeaf2ff };

    /** A seven-segment style glyph for A..G (plus the dash), drawn from
        rounded bars so it reads like an LED display without any font. */
    void drawSegmentNote (Graphics& g, char note, Rectangle<float> r, Colour lit)
    {
        // segments: a top, b upper-right, c lower-right, d bottom, e lower-left, f upper-left, g middle
        static const std::map<char, const char*> segs {
            { 'A', "abcefg" }, { 'B', "cdefg" }, { 'C', "adef" }, { 'D', "bcdeg" },
            { 'E', "adefg" }, { 'F', "aefg" }, { 'G', "acdef" }, { '-', "g" } };
        const auto it = segs.find (note);
        const String on (it != segs.end() ? it->second : "");
        const auto t = r.getWidth() * 0.16f;          // bar thickness
        const auto w = r.getWidth(), h = r.getHeight();
        const auto x = r.getX(), y = r.getY();
        const auto gap = t * 0.18f;
        struct Bar { char id; Rectangle<float> rc; };
        const Bar bars[] {
            { 'a', { x + t * 0.6f + gap, y, w - 1.2f * t - 2 * gap, t } },
            { 'g', { x + t * 0.6f + gap, y + 0.5f * (h - t), w - 1.2f * t - 2 * gap, t } },
            { 'd', { x + t * 0.6f + gap, y + h - t, w - 1.2f * t - 2 * gap, t } },
            { 'f', { x, y + t * 0.6f + gap, t, 0.5f * h - 1.1f * t - 2 * gap } },
            { 'e', { x, y + 0.5f * h + 0.5f * t + gap, t, 0.5f * h - 1.1f * t - 2 * gap } },
            { 'b', { x + w - t, y + t * 0.6f + gap, t, 0.5f * h - 1.1f * t - 2 * gap } },
            { 'c', { x + w - t, y + 0.5f * h + 0.5f * t + gap, t, 0.5f * h - 1.1f * t - 2 * gap } },
        };
        for (auto& b : bars)
        {
            const bool isOn = on.containsChar (b.id);
            if (isOn)
            {
                g.setColour (lit.withAlpha (0.25f));
                g.fillRoundedRectangle (b.rc.expanded (2.5f), t * 0.6f);
            }
            g.setColour (isOn ? lit : tunerUnlit.brighter (0.15f));
            g.fillRoundedRectangle (b.rc, t * 0.45f);
        }
    }

    void drawTriangle (Graphics& g, Rectangle<float> r, bool pointsRight, Colour c)
    {
        Path p;
        if (pointsRight) p.addTriangle (r.getX(), r.getY(), r.getRight(), r.getCentreY(), r.getX(), r.getBottom());
        else             p.addTriangle (r.getRight(), r.getY(), r.getX(), r.getCentreY(), r.getRight(), r.getBottom());
        g.setColour (c);
        g.fillPath (p);
    }
}

TunerPedal::TunerPedal (JuicyboiAudioProcessor& p, Component* popupParent)
    : proc (p),
      footswitch (p.apvts, jb::params::id::tunerOn, "Tuner (on = output muted)"),
      refKnob (p.apvts, jb::params::id::tunerRef, ParamSlider::Style::pedalKnob,
               [] (double v) { return "Tuner reference A4 = " + String (roundToInt (v)) + " Hz (436-445, double-click = 440)"; })
{
    setComponentID ("tunerPedal");
    addAndMakeVisible (footswitch);
    refKnob.setPopupDisplayEnabled (true, false, popupParent);
    refKnob.setDoubleClickReturnValue (true, 440.0);
    addAndMakeVisible (refKnob);
    startTimerHz (30);
}

TunerPedal::~TunerPedal() { stopTimer(); }

int TunerPedal::segmentForCents (double cents) noexcept
{
    const auto c = jlimit (-50.0, 50.0, cents);
    return jlimit (0, numSegments - 1, (int) std::lround (c / 5.0) + numSegments / 2);
}

void TunerPedal::resized()
{
    const auto b = getLocalBounds();
    displayArea = Rectangle<float> (12.0f, 16.0f, (float) b.getWidth() - 24.0f, 148.0f);
    refKnob.setBounds (Rectangle<int> (30, 30).withCentre ({ b.getWidth() - 26, 190 }));
    footswitch.setBounds (Rectangle<int> (52, 52).withCentre ({ b.getWidth() / 2, 238 }));
}

void TunerPedal::timerCallback()
{
    const auto on = proc.apvts.getRawParameterValue (jb::params::id::tunerOn)->load() > 0.5f;
    const auto d = previewDisplay ? *previewDisplay : proc.getTuner().getDisplay();
    if (on != shownOn || d.hasPitch != shown.hasPitch || d.midiNote != shown.midiNote
        || std::abs (d.cents - shown.cents) > 0.2)
    {
        shown = d;
        shownOn = on;
        repaint (displayArea.expanded (4.0f).toNearestInt());
    }
}

void TunerPedal::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    const float radius = 12.0f;

    // gloss-black enclosure
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (b.translated (2.0f, 4.0f), radius);
    ColourGradient body (Colour (0xff2b2d31), b.getX(), b.getY(), Colour (0xff0b0c0e), b.getX(), b.getBottom(), false);
    body.addColour (0.08, Colour (0xff3a3c41));
    body.addColour (0.62, Colour (0xff121316));
    body.addColour (0.64, Colour (0xff1d1e22));      // seam between display and switch halves
    g.setGradientFill (body);
    g.fillRoundedRectangle (b, radius);
    // glossy highlight down the left edge
    ColourGradient sheen (Colours::white.withAlpha (0.16f), b.getX() + 4.0f, 0, Colours::transparentWhite, b.getX() + 26.0f, 0, false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (b.withWidth (26.0f).reduced (0.0f, 6.0f), radius);
    ColourGradient bevel (Colours::white.withAlpha (0.28f), b.getX(), b.getY(), Colours::black.withAlpha (0.6f), b.getX(), b.getBottom(), false);
    g.setGradientFill (bevel);
    g.drawRoundedRectangle (b.reduced (0.75f), radius, 1.4f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawLine (b.getX() + 6.0f, 172.0f, b.getRight() - 6.0f, 172.0f, 1.2f);

    paintDisplay (g, displayArea);

    // nameplate (embossed, low contrast like a moulded logo)
    {
        const auto plate = Rectangle<float> (10.0f, 176.0f, b.getWidth() - 50.0f, 30.0f);
        g.setFont (uiFont (10.0f, false));
        g.setColour (Colours::white.withAlpha (0.45f));
        g.drawText ("juicyboi", plate.withHeight (12.0f), Justification::centredLeft, false);
        g.setFont (uiFont (17.0f));
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawText ("TUNE", plate.withTrimmedTop (11.0f).translated (0.0f, 1.0f), Justification::centredLeft, false);
        g.setColour (Colour (0xffc8ccd4));
        g.drawText ("TUNE", plate.withTrimmedTop (11.0f), Justification::centredLeft, false);
        g.setFont (uiFont (8.0f, false));
        g.setColour (Colours::white.withAlpha (0.5f));
        g.drawText ("CAL", Rectangle<float> (b.getRight() - 42.0f, 202.0f, 34.0f, 10.0f), Justification::centred, false);
    }
    for (auto x : { b.getX() + 9.0f, b.getRight() - 9.0f })
        for (auto y : { b.getY() + 8.0f, b.getBottom() - 8.0f })
            drawChromeScrew (g, { x, y }, 2.2f);

    // mute LED next to the switch (red while the output is muted)
    drawLed (g, { b.getRight() - 20.0f, 238.0f }, 5.0f, colours::ledRed, shownOn ? 1.0f : 0.0f);
}

void TunerPedal::paintDisplay (Graphics& g, Rectangle<float> r)
{
    // recessed glass window
    g.setColour (Colours::black);
    g.fillRoundedRectangle (r.expanded (2.0f), 6.0f);
    ColourGradient gl (glass.brighter (0.25f), r.getX(), r.getY(), glass, r.getX(), r.getBottom(), false);
    g.setGradientFill (gl);
    g.fillRoundedRectangle (r, 5.0f);

    const auto& d = shown;
    const bool live = shownOn && d.hasPitch;
    const bool inTune = live && std::abs (d.cents) <= inTuneCents;
    const auto accent = inTune ? tunerInTune : tunerLit;
    const int litSeg = live ? segmentForCents (d.cents) : -1;
    const int centre = numSegments / 2;

    // fan of vertical segments: taller towards the centre
    const auto meter = r.reduced (8.0f, 0.0f).withTrimmedTop (10.0f).withHeight (40.0f);
    const auto pitch = meter.getWidth() / (float) numSegments;
    for (int i = 0; i < numSegments; ++i)
    {
        const auto dist = std::abs (i - centre) / (float) centre;      // 0 centre .. 1 edge
        const auto hgt = meter.getHeight() * (1.0f - 0.45f * dist);
        auto seg = Rectangle<float> (meter.getX() + i * pitch + pitch * 0.22f, meter.getBottom() - hgt,
                                     pitch * 0.56f, hgt);
        Colour c = tunerUnlit;
        if (live)
        {
            if (i == litSeg)                     c = accent;
            else if (inTune && i == centre)      c = tunerInTune;
            else if (std::abs (i - litSeg) == 1) c = accent.withAlpha (0.35f).overlaidWith (tunerUnlit.withAlpha (0.4f));
        }
        if (c == accent || (inTune && i == centre))
        {
            g.setColour (c.withAlpha (0.3f));
            g.fillRoundedRectangle (seg.expanded (2.0f, 2.0f), 2.0f);
        }
        g.setColour (c);
        g.fillRoundedRectangle (seg, 1.2f);
    }
    // centre needle bar below the fan (the in-tune marker)
    {
        const auto needle = Rectangle<float> (3.0f, 26.0f).withCentre ({ meter.getCentreX(), meter.getBottom() + 18.0f });
        g.setColour (inTune ? tunerInTune : tunerUnlit.brighter (0.25f));
        g.fillRoundedRectangle (needle, 1.0f);
    }
    // flat / sharp arrows pointing at the needle
    {
        const auto y = meter.getBottom() + 8.0f;
        const auto flatOn  = live && ! inTune && d.cents < 0.0;
        const auto sharpOn = live && ! inTune && d.cents > 0.0;
        drawTriangle (g, { r.getX() + 12.0f, y, 22.0f, 16.0f }, true,
                      inTune ? tunerInTune : (flatOn ? tunerLit : tunerUnlit.brighter (0.2f)));
        drawTriangle (g, { r.getRight() - 34.0f, y, 22.0f, 16.0f }, false,
                      inTune ? tunerInTune : (sharpOn ? tunerLit : tunerUnlit.brighter (0.2f)));
    }
    // big note name (segment glyph) + sharp sign + octave
    {
        const auto glyph = Rectangle<float> (44.0f, 64.0f).withCentre ({ r.getCentreX() - 6.0f, r.getBottom() - 46.0f });
        const String name = live ? String (jb::dsp::Tuner::noteName (d.midiNote)) : String ("-");
        drawSegmentNote (g, live ? (char) name[0] : (shownOn ? '-' : ' '), glyph, inTune ? tunerInTune : noteWhite);
        const bool sharp = live && name.length() > 1;
        g.setFont (uiFont (20.0f));
        g.setColour (sharp ? (inTune ? tunerInTune : noteWhite) : tunerUnlit.brighter (0.2f));
        g.drawText ("#", Rectangle<float> (glyph.getRight() + 3.0f, glyph.getY() - 2.0f, 18.0f, 22.0f), Justification::centred, false);
        if (live)
        {
            g.setFont (uiFont (10.0f));
            g.setColour (noteWhite.withAlpha (0.7f));
            g.drawText (String (jb::dsp::Tuner::octave (d.midiNote)),
                        Rectangle<float> (glyph.getRight() + 4.0f, glyph.getBottom() - 14.0f, 16.0f, 14.0f), Justification::centred, false);
        }
    }
    // reference / status line
    {
        g.setFont (uiFont (8.5f, false));
        g.setColour (Colours::white.withAlpha (shownOn ? 0.55f : 0.3f));
        const auto ref = roundToInt (proc.apvts.getRawParameterValue (jb::params::id::tunerRef)->load());
        g.drawText (shownOn ? "A=" + String (ref) + "  MUTE" : "A=" + String (ref) + "  BYPASS",
                    r.withTrimmedTop (r.getHeight() - 13.0f), Justification::centred, false);
    }
    // glass reflection
    ColourGradient refl (Colours::white.withAlpha (0.07f), r.getX(), r.getY(), Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.45f, false);
    g.setGradientFill (refl);
    g.fillRoundedRectangle (r, 5.0f);
}
} // namespace jb::gui
