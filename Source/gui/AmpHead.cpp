#include "AmpHead.h"
#include "../PluginProcessor.h"
#include "../Parameters.h"
#include "../LevelTaper.h"

namespace jb::gui
{
using namespace juce;
namespace pid = jb::params::id;

namespace
{
    constexpr float fadeSeconds = 0.2f;
    const Rectangle<float> grilleArea { 34.0f, 30.0f, 1032.0f, 236.0f };
    const Rectangle<float> panelArea  { 34.0f, 286.0f, 1032.0f, 120.0f };
    constexpr int panelCentreY = 346;
    constexpr int knobCentreY = 340;            // knobs sit a little high to leave room for captions
    constexpr int labelTop = 381, labelHeight = 13; // below the tick dots (knob bottom 374), above the pinstripe (399)
    constexpr int knobSize = 68;
    constexpr int knobLeft = 488, knobRight = 1052;

    String knobTip (const String& name, double v, const String& extra = {})
    {
        auto t = name + " " + String (v, 1);
        if (extra.isNotEmpty()) t << "  -  " << extra;
        return t;
    }
}

AmpHead::AmpHead (JuicyboiAudioProcessor& p, Component* popupParent)
    : proc (p),
      input (p.apvts, pid::inputTrim, ParamSlider::Style::ampKnob, [] (double v)
             { return "Input " + String (v, 1) + " = " + String (taper::inputKnobToDb (v), 1) + " dB (5 = unity, 4.8 dB per number)"; }),
      gain (p.apvts, pid::gain, ParamSlider::Style::ampKnob, [] (double v) { return knobTip ("Gain", v); }),
      bass (p.apvts, pid::bass, ParamSlider::Style::ampKnob, [this] (double v)
            { return knobTip (targetModel() == 1 ? "Low (Bass)" : "Bass", v); }),
      mid (p.apvts, pid::mid, ParamSlider::Style::ampKnob, [] (double v) { return knobTip ("Mid", v); }),
      treble (p.apvts, pid::treble, ParamSlider::Style::ampKnob, [this] (double v)
              { return knobTip (targetModel() == 1 ? "High (Treble)" : "Treble", v); }),
      presence (p.apvts, pid::presence, ParamSlider::Style::ampKnob, [] (double v)
                { return knobTip ("Presence", v, "Rectifier preamp presence"); }),
      leadPresence (p.apvts, pid::leadPresence, ParamSlider::Style::ampKnob, [] (double v)
                    { return knobTip ("5150 Presence", v, "5150-only feedback-response approximation, after EQ/before Cab. "
                                                          "0 = unchanged reference tone; no nonlinear power amp."); }),
      leadResonance (p.apvts, pid::leadResonance, ParamSlider::Style::ampKnob, [] (double v)
                     { return knobTip ("5150 Resonance", v, "5150-only NFB Resonance (VR8 1M + C12 .0068u), after EQ/before Cab. "
                                                            "0 = unchanged reference tone; more = more bass. Resistive load, no speaker/power-amp model."); }),
      master (p.apvts, pid::master, ParamSlider::Style::ampKnob, [] (double v)
              { return "Master " + String (v, 1) + " = " + (v > 0.0 ? String (taper::masterKnobToDb (v), 1) + " dB" : String ("mute"))
                       + " (7.5 = unity, 10 = +12 dB, 0 = silent)"; }),
      standby (p.apvts, pid::bypass, "STBY", "ON", false),
      model (p.apvts, pid::preampModel, "5150", "RECT", true),
      channel (p.apvts, pid::channel, "ORNG", "RED", true),
      voice (p.apvts, pid::redVoice, "V2", "V1", true)
{
    setOpaque (false);
    for (auto* k : { &input, &gain, &bass, &mid, &treble, &presence, &leadPresence, &leadResonance, &master })
    {
        k->setPopupDisplayEnabled (true, false, popupParent);
        addAndMakeVisible (*k);
    }
    standby.extraTip = "Standby = plugin bypass (latency-aligned dry signal).";
    model.extraTip = "Swaps faceplates: Rectifier (Red/Orange) or 5150-inspired Lead. Same plugin, same session.";
    channel.extraTip = "Rectifier channel: Red (Ch1) or Orange (Ch2).";
    voice.extraTip = "Red channel voicing: V1 = original tight Red, V2 = max-gain / fat-bottom Red. Red channel only.";
    for (auto* t : { &standby, &model, &channel, &voice })
        addAndMakeVisible (*t);
    addAndMakeVisible (pilot);
    addChildComponent (leadMark);
    for (auto& l : knobLabels)
    {
        l.setFont (uiFont (10.5f));
        l.setColour (Label::textColourId, colours::creme);
        l.setJustificationType (Justification::centred);
        l.setBorderSize ({});
        l.setInterceptsMouseClicks (false, false);
        addChildComponent (l);
    }

    channel.onChange = [this] { updateFaceplate(); };
    mix = (float) targetModel();
    updateFaceplate();
    startTimerHz (15);
}

AmpHead::~AmpHead() { stopTimer(); }

int AmpHead::targetModel() const
{
    return proc.apvts.getRawParameterValue (pid::preampModel)->load() > 0.5f ? 1 : 0;
}

void AmpHead::syncModelNow()
{
    mix = (float) targetModel();
    updateFaceplate();
    repaint();
}

void AmpHead::timerCallback()
{
    const auto now = Time::getMillisecondCounterHiRes() * 0.001;
    const auto dt = (float) jlimit (0.0, 0.1, now - lastTime);
    lastTime = now;

    const auto lit = proc.apvts.getRawParameterValue (pid::bypass)->load() < 0.5f;
    if (lit != pilot.lit) { pilot.lit = lit; pilot.repaint(); }

    const auto target = (float) targetModel();
    if (std::abs (mix - target) > 1.0e-6f)
    {
        const auto step = dt / fadeSeconds;
        mix = target > mix ? jmin (target, mix + step) : jmax (target, mix - step);
        updateFaceplate();
        repaint (panelArea.getUnion (grilleArea).expanded (4.0f).toNearestInt());
        if (getTimerInterval() > 20) startTimerHz (60);
    }
    else if (getTimerInterval() < 50)
    {
        startTimerHz (15);
    }
}

void AmpHead::updateFaceplate()
{
    const int m = mix < 0.5f ? 0 : 1;
    if (m != shownModel)
    {
        shownModel = m;
        layoutKnobs (m);
    }
    const auto a = jlimit (0.0f, 1.0f, std::abs (1.0f - 2.0f * mix)); // dips to 0 mid-fade, controls hop

    for (auto* c : std::initializer_list<Component*> { &input, &gain, &bass, &mid, &treble, &master })
        c->setAlpha (a);

    const bool rect = m == 0;
    presence.setVisible (rect);
    channel.setVisible (rect);
    voice.setVisible (rect);
    leadPresence.setVisible (! rect);
    leadResonance.setVisible (! rect);
    leadMark.setVisible (! rect);

    const bool isRed = ! channel.isOne();
    voice.setEnabled (isRed);
    presence.setAlpha (a);
    channel.setAlpha (a);
    voice.setAlpha (a * (isRed ? 1.0f : 0.45f));
    leadPresence.setAlpha (a);
    leadResonance.setAlpha (a);
    leadMark.setAlpha (a);
    syncKnobLabels();
}

std::array<ParamSlider*, 9> AmpHead::labelledKnobs() noexcept
{
    return { &input, &gain, &bass, &mid, &treble, &presence, &leadPresence, &leadResonance, &master };
}

void AmpHead::syncKnobLabels()
{
    const auto knobs = labelledKnobs();
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        knobLabels[i].setVisible (knobs[i]->isVisible());
        knobLabels[i].setAlpha (knobs[i]->getAlpha());
    }
}

void AmpHead::layoutKnobs (int m)
{
    std::vector<Component*> row;
    if (m == 0) row = { &input, &gain, &bass, &mid, &treble, &presence, &master };
    else        row = { &input, &gain, &bass, &mid, &treble, &leadPresence, &leadResonance, &master };
    const auto pitch = (float) (knobRight - knobLeft) / (float) row.size();
    for (size_t i = 0; i < row.size(); ++i)
    {
        const auto cx = (float) knobLeft + pitch * ((float) i + 0.5f);
        row[i]->setBounds (Rectangle<int> (knobSize, knobSize).withCentre ({ roundToInt (cx), knobCentreY }));
    }

    // captions below the knobs; abbreviate only if a word would not fit its column
    const auto knobs = labelledKnobs();
    const char* rectNames[] = { "INPUT", "GAIN", "BASS", "MID", "TREBLE", "PRESENCE", "PRESENCE", "RESONANCE", "MASTER" };
    const char* leadNames[] = { "INPUT", "GAIN", "LOW", "MID", "HIGH", "PRESENCE", "PRESENCE", "RESONANCE", "MASTER" };
    const auto font = uiFont (10.5f);
    const auto colW = roundToInt (pitch) - 4;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        String text = m == 0 ? rectNames[i] : leadNames[i];
        if (GlyphArrangement::getStringWidth (font, text) > (float) colW - 2.0f)
            text = text == "RESONANCE" ? "RESO" : text.substring (0, 4);
        knobLabels[i].setText (text, dontSendNotification);
        knobLabels[i].setTooltip (knobs[i]->getName());
        knobLabels[i].setBounds (Rectangle<int> (colW, labelHeight)
                                     .withCentre ({ knobs[i]->getBounds().getCentreX(), labelTop + labelHeight / 2 }));
    }
}

void AmpHead::resized()
{
    auto place = [] (Component& c, int cx, int w, int h) { c.setBounds (Rectangle<int> (w, h).withCentre ({ cx, panelCentreY })); };
    place (standby, 252, 46, 104);
    place (model,   306, 46, 104);
    place (channel, 362, 46, 104);
    place (voice,   416, 46, 104);
    place (pilot,   462, 44, 44);
    leadMark.setBounds (Rectangle<int> (118, 100).withCentre ({ 389, panelCentreY }));
    layoutKnobs (shownModel < 0 ? 0 : shownModel);
}

//==============================================================================
void AmpHead::Pilot::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    // chrome jewel bezel
    Path bezel; bezel.addEllipse (c.x - 11.0f, c.y - 11.0f, 22.0f, 22.0f);
    fillChrome (g, bezel, bezel.getBounds());
    drawLed (g, c, 6.5f, colours::ledGreen, lit ? 0.85f : 0.0f);
}

void AmpHead::LeadMark::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setFont (uiFont (10.5f));
    const auto lamps = b.withSizeKeepingCentre (b.getWidth(), 50.0f);
    const Point<float> rhy (lamps.getX() + 30.0f, lamps.getCentreY() - 4.0f), lead (lamps.getRight() - 30.0f, lamps.getCentreY() - 4.0f);
    drawLed (g, rhy, 6.0f, colours::ledRed, 0.0f);
    drawLed (g, lead, 6.0f, colours::ledRed, 1.0f);
    g.setColour (colours::creme.withAlpha (0.8f));
    g.drawText ("RHYTHM", Rectangle<float> (60.0f, 12.0f).withCentre ({ rhy.x, rhy.y + 20.0f }), Justification::centred, false);
    g.setColour (colours::creme);
    g.drawText ("LEAD", Rectangle<float> (60.0f, 12.0f).withCentre ({ lead.x, lead.y + 20.0f }), Justification::centred, false);
    g.setColour (colours::creme.withAlpha (0.6f));
    g.drawLine (rhy.x + 12.0f, rhy.y, lead.x - 12.0f, lead.y, 1.0f);
}

//==============================================================================
const Image& AmpHead::getCache (int m, float scale)
{
    if (cache[m].isNull() || std::abs (cacheScale[m] - scale) > 0.001f)
    {
        const auto w = roundToInt ((float) designWidth * scale), h = roundToInt ((float) designHeight * scale);
        cache[m] = Image (Image::ARGB, jmax (1, w), jmax (1, h), true);
        Graphics ig (cache[m]);
        ig.addTransform (AffineTransform::scale ((float) w / (float) designWidth, (float) h / (float) designHeight));
        paintStatic (ig, m);
        cacheScale[m] = scale;
    }
    return cache[m];
}

void AmpHead::paint (Graphics& g)
{
    const auto scale = jmax (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto area = getLocalBounds().toFloat();
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    if (mix < 1.0f)
        g.drawImage (getCache (0, scale), area);
    if (mix > 0.0f)
    {
        g.setOpacity (jmin (1.0f, mix));
        g.drawImage (getCache (1, scale), area);
        g.setOpacity (1.0f);
    }
}

static void paintBasketweave (Graphics& g, Rectangle<float> area, bool dark)
{
    const auto gap   = dark ? Colour (0xff050505) : colours::blondeGap;
    const auto base  = dark ? Colour (0xff2b2b2b) : colours::blonde;
    const auto hi    = dark ? Colour (0xff474747) : colours::blondeHi;
    const auto lo    = dark ? Colour (0xff141414) : colours::blondeLo;

    g.setColour (gap);
    g.fillRect (area);
    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (area.toNearestInt());

    const float cell = dark ? 10.0f : 15.0f;
    const int strands = 3;
    const float sw = cell / (float) strands;
    Random rng (dark ? 5150 : 1959);
    for (float y = area.getY(); y < area.getBottom(); y += cell)
        for (float x = area.getX(); x < area.getRight(); x += cell)
        {
            const int i = (int) ((x - area.getX()) / cell), j = (int) ((y - area.getY()) / cell);
            const bool horiz = ((i + j) & 1) == 0;
            for (int s = 0; s < strands; ++s)
            {
                const auto jitter = (rng.nextFloat() - 0.5f) * 0.08f;
                const auto shade = base.interpolatedWith (rng.nextBool() ? hi : lo, 0.12f + 0.1f * rng.nextFloat()).brighter (jitter);
                Rectangle<float> r = horiz ? Rectangle<float> (x + 0.6f, y + (float) s * sw + 0.45f, cell - 1.2f, sw - 0.9f)
                                           : Rectangle<float> (x + (float) s * sw + 0.45f, y + 0.6f, sw - 0.9f, cell - 1.2f);
                g.setColour (shade);
                g.fillRoundedRectangle (r, sw * 0.35f);
                g.setColour (hi.withAlpha (0.55f));
                if (horiz) g.fillRect (r.getX() + 0.8f, r.getY() + 0.25f, r.getWidth() - 1.6f, 0.7f);
                else       g.fillRect (r.getX() + 0.25f, r.getY() + 0.8f, 0.7f, r.getHeight() - 1.6f);
                g.setColour (lo.withAlpha (0.8f));
                if (horiz) g.fillRect (r.getX() + 0.8f, r.getBottom() - 0.8f, r.getWidth() - 1.6f, 0.7f);
                else       g.fillRect (r.getRight() - 0.8f, r.getY() + 0.8f, 0.7f, r.getHeight() - 1.6f);
            }
        }

    // depth: soft inner shadow around the grille opening, plus a gentle light falloff
    const auto d = 18.0f;
    auto edge = [&] (Rectangle<float> r, float x1, float y1, float x2, float y2)
    {
        ColourGradient cg (Colours::black.withAlpha (0.55f), x1, y1, Colours::black.withAlpha (0.0f), x2, y2, false);
        g.setGradientFill (cg);
        g.fillRect (r);
    };
    edge (area.withHeight (d), 0, area.getY(), 0, area.getY() + d);
    edge (area.withTrimmedTop (area.getHeight() - d), 0, area.getBottom(), 0, area.getBottom() - d);
    edge (area.withWidth (d), area.getX(), 0, area.getX() + d, 0);
    edge (area.withTrimmedLeft (area.getWidth() - d), area.getRight(), 0, area.getRight() - d, 0);
    ColourGradient sheen (Colours::white.withAlpha (dark ? 0.04f : 0.10f), area.getCentreX(), area.getY(),
                          Colours::black.withAlpha (dark ? 0.15f : 0.18f), area.getCentreX(), area.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillRect (area);
}

static void paintCornerCap (Graphics& g, float cx, float cy, bool flipX, bool flipY)
{
    const float s = 50.0f, t = 17.0f;
    Path cap;
    cap.addRoundedRectangle (0.0f, 0.0f, s, s, 11.0f);
    Path inner; inner.addRoundedRectangle (t, t, s, s, 5.0f);
    cap.setUsingNonZeroWinding (false);
    cap.addPath (inner);
    auto tf = AffineTransform::scale (flipX ? -1.0f : 1.0f, flipY ? -1.0f : 1.0f).translated (cx, cy);
    Graphics::ScopedSaveState ss (g);
    Path clipRect; clipRect.addRectangle (0.0f, 0.0f, s, s);
    g.reduceClipRegion (clipRect, tf);
    Path shadow (cap); shadow.applyTransform (tf.translated (1.5f, 2.0f));
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillPath (shadow);
    cap.applyTransform (tf);
    fillChrome (g, cap, cap.getBounds());
    g.setColour (Colours::black.withAlpha (0.5f));
    g.strokePath (cap, PathStrokeType (0.8f));
    const auto rivet = Point<float> (8.5f, 8.5f).transformedBy (tf);
    drawChromeScrew (g, Point<float> (t * 0.5f + 1.0f, s - 12.0f).transformedBy (tf), 3.2f);
    drawChromeScrew (g, Point<float> (s - 12.0f, t * 0.5f + 1.0f).transformedBy (tf), 3.2f);
    ignoreUnused (rivet);
}

void AmpHead::paintStatic (Graphics& g, int m)
{
    const bool lead = m == 1;
    const auto all = Rectangle<float> ((float) designWidth, (float) designHeight);

    // --- tolex body
    g.setColour (colours::tolex);
    g.fillRoundedRectangle (all, 12.0f);
    {
        Graphics::ScopedSaveState ss (g);
        Path body; body.addRoundedRectangle (all, 12.0f);
        g.reduceClipRegion (body);
        Random rng (4412);
        for (int i = 0; i < 9000; ++i)
        {
            const auto x = rng.nextFloat() * all.getWidth(), y = rng.nextFloat() * all.getHeight();
            g.setColour (rng.nextBool() ? Colours::white.withAlpha (0.045f) : Colours::black.withAlpha (0.35f));
            g.fillRect (x, y, 1.1f + rng.nextFloat(), 1.1f + rng.nextFloat());
        }
        ColourGradient light (Colours::white.withAlpha (0.07f), 0, 0, Colours::black.withAlpha (0.25f), 0, all.getHeight(), false);
        g.setGradientFill (light);
        g.fillRect (all);
    }
    g.setColour (Colours::black);
    g.drawRoundedRectangle (all.reduced (0.5f), 12.0f, 1.0f);

    // --- grille + piping
    paintBasketweave (g, grilleArea, lead);
    g.setColour (lead ? Colour (0xff3c3c3c) : colours::creme.withAlpha (0.9f));
    g.drawRoundedRectangle (grilleArea.expanded (3.0f), 5.0f, 2.4f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (grilleArea.expanded (5.0f), 6.0f, 1.0f);

    // --- the one juicyboi badge (grille only)
    {
        const auto badge = Rectangle<float> (236.0f, 66.0f).withCentre (grilleArea.getCentre().translated (0.0f, -6.0f));
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (badge.translated (2.0f, 4.0f), 12.0f);
        Path rim; rim.addRoundedRectangle (badge, 12.0f);
        fillChrome (g, rim, badge);
        const auto inner = badge.reduced (4.0f);
        ColourGradient plate (Colour (0xff1e1e1e), inner.getX(), inner.getY(), Colour (0xff050505), inner.getX(), inner.getBottom(), false);
        g.setGradientFill (plate);
        g.fillRoundedRectangle (inner, 9.0f);
        g.setColour (colours::creme.withAlpha (0.7f));
        g.drawRoundedRectangle (inner.reduced (3.0f), 7.0f, 1.0f);
        g.setFont (Font (FontOptions (Font::getDefaultSerifFontName(), 42.0f, Font::bold | Font::italic)));
        g.setColour (Colours::black);
        g.drawText ("juicyboi", inner.translated (1.0f, 2.0f), Justification::centred, false);
        g.setColour (colours::creme);
        g.drawText ("juicyboi", inner, Justification::centred, false);
    }

    // --- control panel
    {
        g.setColour (Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (panelArea.expanded (2.0f).translated (0.0f, 2.0f), 6.0f);
        ColourGradient pg (lead ? Colour (0xff141414) : Colour (0xff1a1a1a), 0, panelArea.getY(),
                           Colour (0xff070707), 0, panelArea.getBottom(), false);
        g.setGradientFill (pg);
        g.fillRoundedRectangle (panelArea, 5.0f);
        Random rng (lead ? 77 : 33);
        for (float y = panelArea.getY() + 2.0f; y < panelArea.getBottom() - 2.0f; y += 1.5f)
        {
            g.setColour (Colours::white.withAlpha (0.008f + 0.012f * rng.nextFloat()));
            g.fillRect (panelArea.getX() + 3.0f, y, panelArea.getWidth() - 6.0f, 0.6f);
        }
        Path edge; edge.addRoundedRectangle (panelArea, 5.0f);
        ColourGradient eg (colours::chromeHi.withAlpha (0.8f), 0, panelArea.getY(), colours::chromeLo, 0, panelArea.getBottom(), false);
        g.setGradientFill (eg);
        g.strokePath (edge, PathStrokeType (1.6f));
        if (! lead)
        {
            g.setColour (colours::creme.withAlpha (0.35f));
            g.drawRoundedRectangle (panelArea.reduced (7.0f), 3.0f, 0.8f);
        }
        else
        {
            g.setColour (Colour (0xff2e2e2e));
            g.drawRoundedRectangle (panelArea.reduced (7.0f), 3.0f, 0.8f);
        }
        // panel screws
        for (auto x : { panelArea.getX() + 16.0f, panelArea.getRight() - 16.0f })
            for (auto y : { panelArea.getY() + 16.0f, panelArea.getBottom() - 16.0f })
                drawChromeScrew (g, { x, y }, 3.0f);
    }

    // --- Revision plate (next to the controls)
    {
        const auto text = revisionPlateText (lead);
        const auto w = lead ? 150.0f : 104.0f;
        const auto plate = Rectangle<float> (60.0f, (float) panelCentreY - 17.0f, w, 34.0f);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (plate.translated (1.0f, 2.0f), 4.0f);
        Path pp; pp.addRoundedRectangle (plate, 4.0f);
        if (lead)
        {
            fillChrome (g, pp, plate);
        }
        else
        {
            ColourGradient cg (colours::creme, plate.getX(), plate.getY(), colours::eqCremeD, plate.getX(), plate.getBottom(), false);
            g.setGradientFill (cg);
            g.fillPath (pp);
        }
        g.setColour (Colours::black.withAlpha (0.5f));
        g.strokePath (pp, PathStrokeType (0.8f));
        drawChromeScrew (g, { plate.getX() + 9.0f, plate.getCentreY() }, 2.6f);
        drawChromeScrew (g, { plate.getRight() - 9.0f, plate.getCentreY() }, 2.6f);
        g.setColour (colours::ink);
        g.setFont (uiFont (17.0f));
        g.drawText (text, plate.reduced (16.0f, 0.0f), Justification::centred, false);
    }

    // --- corner caps
    paintCornerCap (g, 0.0f, 0.0f, false, false);
    paintCornerCap (g, all.getRight(), 0.0f, true, false);
    paintCornerCap (g, 0.0f, all.getBottom(), false, true);
    paintCornerCap (g, all.getRight(), all.getBottom(), true, true);
}
String AmpHead::revisionPlateText (bool leadFaceplate)
{
    return leadFaceplate ? String (CharPointer_UTF8 ("Rev2 \xc2\xb7 5150")) : String ("Rev2");
}
} // namespace jb::gui
