#include "Pedalboard.h"
#include "../PluginProcessor.h"
#include "../Parameters.h"
#include "../LevelTaper.h"

namespace jb::gui
{
using namespace juce;
namespace pid = jb::params::id;

namespace
{
    constexpr float pedalTop = 36.0f, pedalH = 282.0f, switchY = 238.0f;
    // Rev3.2: Tuner first in the chain (left), then Boost, GEQ-7, Gate, Cab.
    const Rectangle<int> tunerArea { 34, (int) pedalTop, 128, (int) pedalH };
    const Rectangle<int> boostArea { 172, (int) pedalTop, 214, (int) pedalH };
    const Rectangle<int> eqArea    { 396, (int) pedalTop, 304, (int) pedalH };
    const Rectangle<int> gateArea  { 710, (int) pedalTop, 200, (int) pedalH };
    const Rectangle<int> cabArea   { 920, (int) pedalTop, 148, (int) pedalH };
    constexpr int eqPitch = 34, eqX0 = 12, eqLevelX = 264;

    void paintWood (Graphics& g, Rectangle<float> r, int seed, bool horizontal, float brightness = 0.0f)
    {
        ColourGradient base (colours::woodHi.brighter (brightness), r.getX(), r.getY(),
                             colours::wood.darker (0.15f - brightness), horizontal ? r.getX() : r.getRight(),
                             horizontal ? r.getBottom() : r.getY(), false);
        g.setGradientFill (base);
        g.fillRect (r);
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r.toNearestInt());
        Random rng (seed);
        const auto span = horizontal ? r.getHeight() : r.getWidth();
        const int lines = jmax (6, (int) (span / 2.2f));
        for (int i = 0; i < lines; ++i)
        {
            const auto off = rng.nextFloat() * span;
            const auto amp = 0.6f + rng.nextFloat() * 2.2f;
            const auto freq = 0.004f + rng.nextFloat() * 0.01f;
            const auto ph = rng.nextFloat() * 6.28f;
            Path p;
            const auto len = horizontal ? r.getWidth() : r.getHeight();
            for (float t = 0.0f; t <= len; t += 12.0f)
            {
                const auto w = off + amp * std::sin (t * freq + ph);
                const Point<float> pt = horizontal ? Point<float> (r.getX() + t, r.getY() + w) : Point<float> (r.getX() + w, r.getY() + t);
                if (p.isEmpty()) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            g.setColour ((rng.nextBool() ? colours::woodLo : colours::woodHi.brighter (0.2f)).withAlpha (0.25f + 0.35f * rng.nextFloat()));
            g.strokePath (p, PathStrokeType (0.6f + rng.nextFloat() * 0.9f));
        }
    }

    float rawParam (JuicyboiAudioProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
}

//==============================================================================
Pedal::Pedal (Kind k, Colour b, Colour bd, Colour t, String ttl)
    : kind (k), body (b), bodyDark (bd), text (t), title (std::move (ttl))
{
    setPaintingIsUnclipped (false);
}

void Pedal::pollLed()
{
    if (! ledAmount) return;
    const auto a = ledAmount();
    const auto c = ledColour ? ledColour() : colours::ledGreen;
    if (std::abs (a - shownLed) > 0.02f || c != shownColour)
    {
        shownLed = a; shownColour = c;
        // Whole pedal, not a small LED rectangle: how CoreGraphics merges/clips partial
        // dirty rectangles differs between macOS versions and machines, and an LED that
        // only repaints through its own tiny rectangle was the one thing that never
        // lit on an Apple-Silicon MacBook (docs/led-repaint.md). A pedal is small and
        // repaints only when its LED changes.
        repaint (ledRepaintArea());
    }
}

Rectangle<float> Pedal::ledFootprint() const noexcept
{
    const auto r = (kind == Kind::stomp ? 6.0f : 4.5f) * 3.2f;   // glow radius in drawLed
    return Rectangle<float> (2.0f * r, 2.0f * r).withCentre (ledCentre);
}

void Pedal::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    const auto radius = kind == Kind::stomp ? 14.0f : 8.0f;

    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (b.translated (2.0f, 4.0f), radius);
    ColourGradient cg (body, b.getX(), b.getY(), bodyDark, b.getX(), b.getBottom(), false);
    g.setGradientFill (cg);
    g.fillRoundedRectangle (b, radius);
    // bevel
    ColourGradient bevel (Colours::white.withAlpha (0.35f), b.getX(), b.getY(), Colours::black.withAlpha (0.5f), b.getX(), b.getBottom(), false);
    g.setGradientFill (bevel);
    g.drawRoundedRectangle (b.reduced (0.75f), radius, 1.5f);

    if (kind == Kind::stomp)
    {
        g.setColour (Colours::black.withAlpha (0.18f));
        g.drawRoundedRectangle (b.reduced (8.0f), radius - 6.0f, 1.0f);
        for (auto x : { b.getX() + 10.0f, b.getRight() - 10.0f })
            for (auto y : { b.getY() + 10.0f, b.getBottom() - 10.0f })
                drawChromeScrew (g, { x, y }, 2.6f);
    }
    else
    {
        g.setColour (colours::chromeMid.withAlpha (0.5f));
        g.drawRoundedRectangle (b.reduced (4.0f), radius - 3.0f, 0.8f);
    }

    g.setColour (text);
    for (auto& c : captions)
    {
        g.setFont (uiFont (c.size));
        g.drawText (c.text, c.area, Justification::centred, false);
    }
    if (title.isNotEmpty())
    {
        g.setFont (uiFont (titleSize));
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawText (title, titleArea.translated (0.0f, 1.5f), Justification::centred, false);
        g.setColour (text);
        g.drawText (title, titleArea, Justification::centred, false);
    }
    if (ledAmount)
        drawLed (g, ledCentre, kind == Kind::stomp ? 6.0f : 4.5f, ledColour ? ledColour() : colours::ledGreen, ledAmount());
}

//==============================================================================
IRDropZone::IRDropZone (JuicyboiAudioProcessor& p, int a, int s) : proc (p), amp (a), slot (s)
{
    setComponentID (jb::params::ampIrKey (a, s));
    clearButton.setTooltip ("Clear IR");
    clearButton.onClick = [this] { proc.clearCabIR (amp, slot); refresh(); };
    addAndMakeVisible (clearButton);
    setMouseCursor (MouseCursor::PointingHandCursor);
    refresh();
}

void IRDropZone::refresh()
{
    clearButton.setVisible (proc.hasCabIR (amp, slot));
    repaint();
}

void IRDropZone::resized()
{
    clearButton.setBounds (getLocalBounds().removeFromRight (22).withSizeKeepingCentre (18, 18));
}

String IRDropZone::getTooltip()
{
    const auto name = (amp == 1 ? String ("5150 ") : String()) + dsp::CabSim::slotName (slot);
    if (proc.hasCabIR (amp, slot))
        return name + ": " + proc.getCabIRFile (amp, slot).getFullPathName() + "\nDrop or click to replace; x clears.";
    return name + ": empty. Drop a .wav/.aif/.flac IR here, or click to browse.";
}

void IRDropZone::mouseUp (const MouseEvent& e)
{
    if (! isEnabled() || ! e.mouseWasClicked()) return;
    chooser = std::make_unique<FileChooser> ("Load " + dsp::CabSim::slotName (slot), File(), "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [safe = Component::SafePointer<IRDropZone> (this)] (const FileChooser& fc)
                          {
                              if (safe == nullptr) return;
                              const auto f = fc.getResult();
                              if (f.existsAsFile())
                                  safe->proc.loadCabIR (safe->amp, safe->slot, f);
                              safe->refresh();
                          });
}

bool IRDropZone::isInterestedInFileDrag (const StringArray& files)
{
    if (! isEnabled()) return false;
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac"))
            return true;
    return false;
}

void IRDropZone::filesDropped (const StringArray& files, int, int)
{
    hovering = false;
    if (! isEnabled()) { repaint(); return; }
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac"))
        {
            proc.loadCabIR (amp, slot, File (f));
            break;
        }
    refresh();
}

void IRDropZone::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const auto loaded = proc.hasCabIR (amp, slot);
    g.setColour (hovering ? Colour (0xff2f4a22) : Colour (0xff0b0b0b));
    g.fillRoundedRectangle (b, 4.0f);
    Path border; border.addRoundedRectangle (b, 4.0f);
    if (loaded)
    {
        g.setColour (colours::ledGreen.withAlpha (0.8f));
        g.strokePath (border, PathStrokeType (1.2f));
    }
    else
    {
        Path dashed;
        const float dashes[] = { 4.0f, 3.0f };
        PathStrokeType (1.0f).createDashedStroke (dashed, border, dashes, 2);
        g.setColour (colours::creme.withAlpha (0.7f));
        g.fillPath (dashed);
    }
    auto t = getLocalBounds().reduced (6, 2).withTrimmedRight (loaded ? 18 : 0);
    g.setColour (colours::creme);
    g.setFont (uiFont (10.0f));
    g.drawText (dsp::CabSim::slotName (slot), t.removeFromTop (t.getHeight() / 2), Justification::bottomLeft, false);
    g.setColour (loaded ? colours::creme : colours::creme.withAlpha (0.85f));
    g.setFont (uiFont (9.5f, false));
    g.drawText (loaded ? proc.getCabIRFile (amp, slot).getFileName() : String ("drop / click IR"), t, Justification::topLeft, true);
}

//==============================================================================
Pedalboard::AmpControls::AmpControls (JuicyboiAudioProcessor& p, int amp)
    : odDrive (p.apvts, params::ampId (amp, params::AmpSlot::odDrive), ParamSlider::Style::pedalKnob),
      odTone (p.apvts, params::ampId (amp, params::AmpSlot::odTone), ParamSlider::Style::pedalKnob),
      odLevel (p.apvts, params::ampId (amp, params::AmpSlot::odLevel), ParamSlider::Style::pedalKnob, [] (double v)
               { return "Boost Level " + String (v, 1) + " = " + String (taper::boostLevelKnobToDb (v), 1)
                        + " dB (6.7 = unity, 3.6 dB per number, 10 = +12 dB)"; }),
      odType (p.apvts, params::ampId (amp, params::AmpSlot::odType), "SD", "OD", true, true),
      odSwitch (p.apvts, params::ampId (amp, params::AmpSlot::odEnable), "Boost"),
      eqLevel (p.apvts, params::ampId (amp, params::AmpSlot::eqLevel), ParamSlider::Style::fader),
      eqSwitch (p.apvts, params::ampId (amp, params::AmpSlot::eqEnable), "Graphic EQ"),
      gateThreshold (p.apvts, params::ampId (amp, params::AmpSlot::gateThreshold), ParamSlider::Style::pedalKnob),
      gateHold (p.apvts, params::ampId (amp, params::AmpSlot::gateHold), ParamSlider::Style::pedalKnob),
      gateRelease (p.apvts, params::ampId (amp, params::AmpSlot::gateRelease), ParamSlider::Style::pedalKnob),
      gateSwitch (p.apvts, params::ampId (amp, params::AmpSlot::gateEnable), "Gate"),
      cabOn (p.apvts, params::ampId (amp, params::AmpSlot::cabEnable), "ON", "OFF", true, true),
      cabBlend (p.apvts, params::ampId (amp, params::AmpSlot::cabMicBlend), ParamSlider::Style::pedalKnob),
      cabLevel (p.apvts, params::ampId (amp, params::AmpSlot::cabLevel), ParamSlider::Style::pedalKnob, [] (double v)
                { return "Cab Level " + String (v, 1) + " = " + String (taper::cabLevelKnobToDb (v), 1)
                         + " dB (5 = unity, 4.8 dB per number)"; }),
      cabHpf (p.apvts, params::ampCabHpfId (amp), ParamSlider::Style::pedalKnob),
      cabLpf (p.apvts, params::ampCabLpfId (amp), ParamSlider::Style::pedalKnob)
{
    for (size_t b = 0; b < eqBands.size(); ++b)
        eqBands[b] = std::make_unique<ParamSlider> (p.apvts, params::ampEqBandId (amp, (int) b), ParamSlider::Style::fader);
    for (int s = 0; s < 2; ++s)
        irZones[(size_t) s] = std::make_unique<IRDropZone> (p, amp, s);
}

std::vector<Component*> Pedalboard::AmpControls::all()
{
    std::vector<Component*> v { &odDrive, &odTone, &odLevel, &odType, &odSwitch, &eqLevel, &eqSwitch,
                                &gateThreshold, &gateHold, &gateRelease, &gateSwitch, &cabOn, &cabBlend, &cabLevel,
                                &cabHpf, &cabLpf };
    for (auto& b : eqBands) v.push_back (b.get());
    for (auto& z : irZones) v.push_back (z.get());
    return v;
}

std::vector<Component*> Pedalboard::AmpControls::cabControls()
{
    std::vector<Component*> v { &cabOn, &cabBlend, &cabLevel, &cabHpf, &cabLpf };
    for (auto& z : irZones) v.push_back (z.get());
    return v;
}

Pedalboard::Pedalboard (JuicyboiAudioProcessor& p, Component* popupParent)
    : proc (p),
      boost (Pedal::Kind::stomp, colours::boostGreen, colours::boostGreenD, colours::creme, "BOOST"),
      eq (Pedal::Kind::stomp, colours::eqCreme, colours::eqCremeD, colours::ink, "GEQ-7"),
      gate (Pedal::Kind::stomp, Colour (0xff2a2a2a), Colour (0xff121212), colours::creme, "GATE"),
      cab (Pedal::Kind::sidePanel, Colour (0xff1c1c1c), Colour (0xff0d0d0d), colours::creme, "CAB"),
      tuner (p, popupParent),
      inputCal (p.apvts, pid::inputTrim, ParamSlider::Style::pedalKnob,
                [] (double v)
                {
                    return "Input Cal " + String (v, 1) + " = " + String (taper::inputKnobToDb (v), 1)
                           + " dB (shared by both amps; 5 = unity, 4.8 dB per number). "
                             "Set once for your interface level - this is not a preamp gain knob.";
                }),
      unused5150Input (p.apvts, jb::params::ampId (1, jb::params::AmpSlot::inputTrim), ParamSlider::Style::pedalKnob),
      preampOut (p.apvts, pid::preampOut, "PRE", "FULL", true, true)
{
    for (auto* pd : { &boost, &eq, &gate, &cab })
        addAndMakeVisible (*pd);
    addAndMakeVisible (tuner);

    auto popup = [popupParent] (ParamSlider& s) { s.setPopupDisplayEnabled (true, false, popupParent); };

    for (int amp = 0; amp < 2; ++amp)
    {
        sets[(size_t) amp] = std::make_unique<AmpControls> (p, amp);
        auto& c = *sets[(size_t) amp];

        // --- Boost
        {
            const float xs[] = { 39.0f, 107.0f, 175.0f };
            ParamSlider* ks[] = { &c.odDrive, &c.odTone, &c.odLevel };
            for (int i = 0; i < 3; ++i)
            {
                popup (*ks[i]);
                boost.addChildComponent (*ks[i]);
                ks[i]->setBounds (Rectangle<int> (60, 60).withCentre ({ (int) xs[i], 48 }));
            }
            c.odType.extraTip = "OD-808 = symmetric mid hump; SD-1 = asymmetric, open-top voicing.";
            boost.addChildComponent (c.odType);
            c.odType.setBounds (Rectangle<int> (100, 30).withCentre ({ 107, 116 }));
            boost.addChildComponent (c.odSwitch);
            c.odSwitch.setBounds (Rectangle<int> (52, 52).withCentre ({ 107, (int) switchY }));
        }
        // --- 7-band EQ (100 Hz .. 6.4 kHz) + Level
        {
            for (size_t b = 0; b < c.eqBands.size(); ++b)
            {
                c.eqBands[b]->extraTip = "(GEQ-7 slider, dB scale: +/-15 = slider end; double-click = 0 dB)";
                popup (*c.eqBands[b]);
                eq.addChildComponent (*c.eqBands[b]);
                c.eqBands[b]->setBounds (eqX0 + (int) b * eqPitch, 14, 32, 136);
            }
            c.eqLevel.extraTip = "(GEQ-7 Level, dB scale, ends = +/-14.9 dB; double-click = 0 dB)";
            popup (c.eqLevel);
            eq.addChildComponent (c.eqLevel);
            c.eqLevel.setBounds (eqLevelX, 14, 32, 136);
            eq.addChildComponent (c.eqSwitch);
            c.eqSwitch.setBounds (Rectangle<int> (52, 52).withCentre ({ 152, (int) switchY }));
        }
        // --- Gate
        {
            const float xs[] = { 36.0f, 100.0f, 164.0f };
            ParamSlider* ks[] = { &c.gateThreshold, &c.gateHold, &c.gateRelease };
            for (int i = 0; i < 3; ++i)
            {
                popup (*ks[i]);
                gate.addChildComponent (*ks[i]);
                ks[i]->setBounds (Rectangle<int> (60, 60).withCentre ({ (int) xs[i], 48 }));
            }
            gate.addChildComponent (c.gateSwitch);
            c.gateSwitch.setBounds (Rectangle<int> (52, 52).withCentre ({ 100, (int) switchY }));
        }
        // --- Cab side panel (Rev3.2.2: HPF / LPF under Blend / Level)
        {
            c.cabOn.extraTip = "Off = hard bypass (e.g. into a real power amp / FX return). No IRs loaded = passthrough; HPF/LPF still apply when Cab is on.";
            cab.addChildComponent (c.cabOn);
            c.cabOn.setBounds (Rectangle<int> (104, 24).withCentre ({ 74, 38 }));
            c.cabBlend.extraTip = "(left = IR A, right = IR B)";
            c.cabHpf.extraTip = "(post-IR high-pass, 12 dB/oct; 20 Hz = off / bypass)";
            c.cabLpf.extraTip = "(post-IR low-pass, 12 dB/oct; 20 kHz = off / bypass)";
            const float xs[] = { 40.0f, 108.0f };
            const int ys[] = { 78, 144 };
            ParamSlider* ks[] = { &c.cabBlend, &c.cabLevel, &c.cabHpf, &c.cabLpf };
            for (int i = 0; i < 4; ++i)
            {
                ks[i]->getProperties().set ("jbTick", colours::creme.toString());
                popup (*ks[i]);
                cab.addChildComponent (*ks[i]);
                ks[i]->setBounds (Rectangle<int> (50, 50).withCentre ({ (int) xs[i % 2], ys[i / 2] }));
            }
            for (int s = 0; s < 2; ++s)
            {
                cab.addChildComponent (*c.irZones[(size_t) s]);
                c.irZones[(size_t) s]->setBounds (10, 192 + s * 38, 128, 32);
            }
        }
    }

    // captions, titles and LEDs are shared by both sets; the LEDs follow the shown amp
    {
        const float xs[] = { 39.0f, 107.0f, 175.0f };
        const char* caps[] = { "DRIVE", "TONE", "LEVEL" };
        for (int i = 0; i < 3; ++i)
            boost.captions.push_back ({ Rectangle<float> (70.0f, 12.0f).withCentre ({ xs[i], 86.0f }), caps[i], 11.0f });
        boost.titleArea = { 0.0f, 140.0f, 214.0f, 40.0f };
        boost.titleSize = 32.0f;
        boost.ledCentre = { 169.0f, switchY };
        boost.ledAmount = [this] { return rawParam (proc, params::ampId (activeAmp(), params::AmpSlot::odEnable)) > 0.5f ? 1.0f : 0.0f; };
    }
    {
        const char* caps[] = { "100", "200", "400", "800", "1.6k", "3.2k", "6.4k" };
        for (int b = 0; b < 7; ++b)
            eq.captions.push_back ({ Rectangle<float> ((float) (eqX0 + b * eqPitch) - 2.0f, 152.0f, 36.0f, 13.0f), caps[b], 10.0f });
        eq.captions.push_back ({ Rectangle<float> ((float) eqLevelX - 2.0f, 152.0f, 36.0f, 13.0f), "LEVEL", 9.0f });
        eq.titleArea = { 0.0f, 172.0f, 304.0f, 32.0f };
        eq.titleSize = 24.0f;
        eq.ledCentre = { 216.0f, switchY };
        eq.ledAmount = [this] { return rawParam (proc, params::ampId (activeAmp(), params::AmpSlot::eqEnable)) > 0.5f ? 1.0f : 0.0f; };
    }
    {
        const float xs[] = { 40.0f, 110.0f, 180.0f };
        const char* caps[] = { "THRESH", "HOLD", "RELEASE" };
        for (int i = 0; i < 3; ++i)
            gate.captions.push_back ({ Rectangle<float> (70.0f, 12.0f).withCentre ({ xs[i], 86.0f }), caps[i], 11.0f });
        gate.titleArea = { 0.0f, 140.0f, 200.0f, 40.0f };
        gate.titleSize = 32.0f;
        gate.ledCentre = { 156.0f, switchY };
        gate.ledAmount = [this]
        {
            // lit whenever the gate is switched on (the parameter), independent of whether the
            // audio callback has run yet; colour (below) shows open / closed
            return rawParam (proc, params::ampId (activeAmp(), params::AmpSlot::gateEnable)) > 0.5f ? 1.0f : 0.0f;
        };
        gate.ledColour = [this]
        {
            // green = open, orange-red = closed (keyed from the dry input)
            const auto open = proc.getGateOpenAmount (activeAmp());
            return colours::ledRed.interpolatedWith (colours::ledGreen, std::isfinite (open) ? jlimit (0.0f, 1.0f, open) : 1.0f);
        };
    }
    {
        const float xs[] = { 40.0f, 108.0f };
        const char* caps[] = { "BLEND", "LEVEL", "HPF", "LPF" };
        const float capY[] = { 108.0f, 174.0f };
        for (int i = 0; i < 4; ++i)
            cab.captions.push_back ({ Rectangle<float> (64.0f, 12.0f).withCentre ({ xs[i % 2], capY[i / 2] }), caps[i], 10.5f });
        cab.titleArea = { 0.0f, 6.0f, 148.0f, 16.0f };
        cab.titleSize = 14.0f;
        cab.ledCentre = { 126.0f, 15.0f };
        cab.ledAmount = [this] { return rawParam (proc, params::ampId (activeAmp(), params::AmpSlot::cabEnable)) > 0.5f ? 1.0f : 0.0f; };
    }
    showAmp (activeAmp());

    front.onClick = [this] { if (onToggle) onToggle(); };
    front.setMouseCursor (MouseCursor::PointingHandCursor);
    addAndMakeVisible (front);

    inputCal.setPopupDisplayEnabled (true, false, popupParent);
    addAndMakeVisible (inputCal);
    inputCalLabel.setText ("INPUT CAL", dontSendNotification);
    inputCalLabel.setFont (uiFont (9.5f));
    inputCalLabel.setColour (Label::textColourId, colours::creme.withAlpha (0.9f));
    inputCalLabel.setJustificationType (Justification::centredLeft);
    inputCalLabel.setBorderSize ({});
    inputCalLabel.setInterceptsMouseClicks (false, false);
    inputCalLabel.setTooltip (inputCal.getTooltip());
    addAndMakeVisible (inputCalLabel);
    // Bound but unused / hidden: keeps host parameter index and the "every
    // parameter has a control" editor check, without a second Input knob.
    unused5150Input.setVisible (false);
    addChildComponent (unused5150Input);

    preampOut.extraTip = "Full Amp is the complete plugin (cab, speaker load, 5150 Presence/Resonance). "
                         "Preamp Out is for a real amp's effects return: preamp, boost, tone stack, post EQ and gate only.";
    addAndMakeVisible (preampOut);
    preampOutLabel.setText ("PREAMP OUT", dontSendNotification);
    preampOutLabel.setFont (uiFont (8.5f));
    preampOutLabel.setColour (Label::textColourId, colours::creme.withAlpha (0.9f));
    preampOutLabel.setJustificationType (Justification::centredLeft);
    preampOutLabel.setBorderSize ({});
    preampOutLabel.setInterceptsMouseClicks (false, false);
    preampOutLabel.setTooltip (preampOut.getTooltip());
    addAndMakeVisible (preampOutLabel);
    preampOut.onChange = [this] { syncPreampOutDim(); };
    syncPreampOutDim();

    // Switching a pedal updates its LED straight away (the 30 Hz poll stays as the safety net).
    for (auto& set : sets)
        for (auto* sw : { &set->odSwitch, &set->eqSwitch, &set->gateSwitch })
            sw->onChange = [this] { pollLedsNow(); };
    for (auto& set : sets)
        set->cabOn.onChange = [this] { pollLedsNow(); };
    startTimerHz (30);
}

int Pedalboard::activeAmp() const
{
    return proc.apvts.getRawParameterValue (pid::preampModel)->load() > 0.5f ? 1 : 0;
}

void Pedalboard::showAmp (int amp)
{
    if (amp == shownAmp) return;
    shownAmp = amp;
    for (int a = 0; a < 2; ++a)
        for (auto* c : sets[(size_t) a]->all())
            c->setVisible (a == amp);
    for (auto& z : sets[(size_t) amp]->irZones) z->refresh();
}

Pedalboard::~Pedalboard() { stopTimer(); }

void Pedalboard::pollLedsNow()
{
    for (auto* pd : { &boost, &eq, &gate, &cab })
        pd->pollLed();
}

void Pedalboard::timerCallback()
{
    showAmp (activeAmp());
    syncPreampOutDim();
    if (! open) return;
    pollLedsNow();
}

void Pedalboard::syncPreampOutDim()
{
    const bool pre = proc.apvts.getRawParameterValue (pid::preampOut)->load() > 0.5f;
    const auto a = pre ? 0.45f : 1.0f;
    cab.setAlpha (a);
    cab.setEnabled (! pre);
    for (int amp = 0; amp < 2; ++amp)
        for (auto* c : sets[(size_t) amp]->cabControls())
        {
            c->setEnabled (! pre);
            c->setAlpha (a);
        }
}

void Pedalboard::setOpen (bool o)
{
    open = o;
    front.open = o;
    for (auto* pd : { &boost, &eq, &gate, &cab })
        pd->setVisible (o);
    tuner.setVisible (o);
    front.repaint();
    resized();
    repaint();
}

void Pedalboard::resized()
{
    tuner.setBounds (tunerArea);
    boost.setBounds (boostArea);
    eq.setBounds (eqArea);
    gate.setBounds (gateArea);
    cab.setBounds (cabArea);
    front.setBounds (getLocalBounds().removeFromBottom (frontHeight));
    // Small setup trim on the left of the drawer front (always visible).
    const auto fb = front.getBounds();
    inputCal.setBounds (Rectangle<int> (32, 32).withCentre ({ fb.getX() + 30, fb.getCentreY() }));
    inputCalLabel.setBounds (fb.getX() + 50, fb.getY() + 8, 72, fb.getHeight() - 16);
    preampOutLabel.setBounds (fb.getX() + 124, fb.getY() + 8, 78, fb.getHeight() - 16);
    preampOut.setBounds (Rectangle<int> (116, 44).withCentre ({ fb.getX() + 256, fb.getCentreY() }));
    unused5150Input.setBounds ({});
}

void Pedalboard::paint (Graphics& g)
{
    if (! open) return;
    const auto interior = Rectangle<float> ((float) designWidth, (float) interiorHeight);
    // felt floor
    g.setColour (colours::felt);
    g.fillRect (interior);
    {
        Random rng (808);
        for (int i = 0; i < 5000; ++i)
        {
            g.setColour (Colours::white.withAlpha (0.025f));
            g.fillRect (rng.nextFloat() * interior.getWidth(), rng.nextFloat() * interior.getHeight(), 1.2f, 1.2f);
        }
    }
    // pedal rails
    for (auto y : { 100.0f, 190.0f, 280.0f })
    {
        g.setColour (Colours::black);
        g.fillRect (26.0f, y, interior.getWidth() - 52.0f, 14.0f);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.fillRect (26.0f, y, interior.getWidth() - 52.0f, 1.0f);
    }
    // back wall + side walls (wood)
    paintWood (g, { 0.0f, 0.0f, interior.getWidth(), 18.0f }, 11, true, 0.08f);
    paintWood (g, { 0.0f, 0.0f, 26.0f, interior.getHeight() }, 12, false);
    paintWood (g, { interior.getRight() - 26.0f, 0.0f, 26.0f, interior.getHeight() }, 13, false);
    ColourGradient sh (Colours::black.withAlpha (0.6f), 0, 18.0f, Colours::black.withAlpha (0.0f), 0, 46.0f, false);
    g.setGradientFill (sh);
    g.fillRect (26.0f, 18.0f, interior.getWidth() - 52.0f, 28.0f);
    for (auto x : { 26.0f, interior.getRight() - 26.0f })
    {
        ColourGradient side (Colours::black.withAlpha (0.55f), x, 0, Colours::black.withAlpha (0.0f), x < 100 ? x + 22.0f : x - 22.0f, 0, false);
        g.setGradientFill (side);
        g.fillRect (x < 100 ? x : x - 22.0f, 18.0f, 22.0f, interior.getHeight() - 18.0f);
    }
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawRect (interior.withTrimmedTop (18.0f).reduced (26.0f, 0.0f), 1.0f);
}

void Pedalboard::Front::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    paintWood (g, b, 21, true, 0.04f);
    ColourGradient bev (Colours::white.withAlpha (0.22f), 0, b.getY(), Colours::black.withAlpha (0.45f), 0, b.getBottom(), false);
    g.setGradientFill (bev);
    g.drawRect (b.reduced (0.5f), 1.5f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRect (b.withHeight (2.0f));

    // Recessed setup plate on the left (Input Cal knob sits on top of it).
    {
        const auto plate = Rectangle<float> (10.0f, 8.0f, 318.0f, b.getHeight() - 16.0f);
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (plate.translated (0.0f, 1.5f), 5.0f);
        g.setColour (Colour (0xff24150c));
        g.fillRoundedRectangle (plate, 5.0f);
        g.setColour (colours::woodHi.withAlpha (0.35f));
        g.drawRoundedRectangle (plate, 5.0f, 1.0f);
    }

    // chrome pull handle
    const auto handle = Rectangle<float> (190.0f, 14.0f).withCentre (b.getCentre());
    for (auto x : { handle.getX() + 6.0f, handle.getRight() - 6.0f })
    {
        Path post; post.addRoundedRectangle (x - 7.0f, handle.getY() - 5.0f, 14.0f, 24.0f, 3.0f);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillPath (post, AffineTransform::translation (1.0f, 2.0f));
        fillChrome (g, post, post.getBounds());
    }
    Path bar; bar.addRoundedRectangle (handle, 7.0f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillPath (bar, AffineTransform::translation (1.5f, 3.0f));
    fillChrome (g, bar, handle);
    g.setColour (Colours::black.withAlpha (0.4f));
    g.strokePath (bar, PathStrokeType (0.8f));

    // tiny open/close chevron, engraved
    Path chev;
    const auto cx = handle.getRight() + 28.0f, cy = b.getCentreY();
    if (open) { chev.startNewSubPath (cx - 6, cy + 3); chev.lineTo (cx, cy - 3); chev.lineTo (cx + 6, cy + 3); }
    else      { chev.startNewSubPath (cx - 6, cy - 3); chev.lineTo (cx, cy + 3); chev.lineTo (cx + 6, cy - 3); }
    g.setColour (colours::woodLo.withAlpha (0.9f));
    g.strokePath (chev, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
}
} // namespace jb::gui
