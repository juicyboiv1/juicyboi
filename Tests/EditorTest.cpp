// Rev1 editor checks, painted offscreen only (no native window / desktop interaction):
// every parameter has a bound control, the Model toggle swaps faceplates without
// touching other parameters, the drawer opens/closes with a fixed aspect ratio,
// and the head never labels its knobs (tooltips name them instead).
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "gui/AmpHead.h"
#include <cstdio>

namespace
{
int failures = 0;
void check (const char* what, bool ok)
{
    std::printf ("editor: %-64s %s\n", what, ok ? "ok" : "FAIL");
    if (! ok) ++failures;
}

juce::Component* findById (juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id) return &c;
    for (auto* child : c.getChildren())
        if (auto* f = findById (*child, id)) return f;
    return nullptr;
}

bool showing (juce::Component* c)
{
    if (c == nullptr) return false;
    // up to (not including) the editor itself, which is only visible once a host shows it
    for (; c->getParentComponent() != nullptr; c = c->getParentComponent())
        if (! c->isVisible()) return false;
    return true;
}

void setParam (JuicyboiAudioProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}
float getParam (JuicyboiAudioProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
} // namespace

int editorValidation()
{
    failures = 0;
    JuicyboiAudioProcessor p;
    setParam (p, "channel", 1.0f);
    setParam (p, "red_voice", 0.0f);
    std::unique_ptr<juce::AudioProcessorEditor> base (p.createEditor());
    auto* ed = dynamic_cast<JuicyboiAudioProcessorEditor*> (base.get());
    check ("editor is the amp-head editor", ed != nullptr);
    if (ed == nullptr) return failures;
    check ("revision plate reads Rev2 / Rev2 \xc2\xb7 5150 and the plugin is named juicyboi Rev2",
           jb::gui::AmpHead::revisionPlateText (false) == "Rev2"
           && jb::gui::AmpHead::revisionPlateText (true) == juce::String (juce::CharPointer_UTF8 ("Rev2 \xc2\xb7 5150"))
           && p.getName() == "juicyboi Rev2");
    auto& root = ed->getRoot();
    check ("opens at 1100 x open design height (scale 1.0)",
           ed->getWidth() == 1100 && ed->getHeight() == JuicyboiAudioProcessorEditor::openHeight);

    // 1. every parameter is bound to a control with the same component ID
    bool allBound = true;
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (findById (root, r->getParameterID()) == nullptr)
            {
                std::printf ("  no control for %s\n", r->getParameterID().toRawUTF8());
                allBound = false;
            }
    check ("every parameter has a control (same ID)", allBound);
    check ("IR A / IR B load targets present for both amps", findById (root, "ir_a") != nullptr && findById (root, "ir_b") != nullptr
           && findById (root, "m5150_ir_a") != nullptr && findById (root, "m5150_ir_b") != nullptr);

    // 2. head knobs: a creme caption BELOW each visible knob, nothing above; tooltips kept
    auto checkCaptions = [&] (const char* what, juce::StringArray expected)
    {
        auto& head = ed->getHead();
        bool below = true, noneAbove = true, tipsOk = true, insidePanel = true;
        juce::StringArray seen;
        for (auto* c : head.getChildren())
            if (auto* s = dynamic_cast<juce::Slider*> (c); s != nullptr && s->isVisible())
            {
                tipsOk = tipsOk && s->getTooltip().isNotEmpty();
                bool found = false;
                for (auto* lc : head.getChildren())
                    if (auto* l = dynamic_cast<juce::Label*> (lc); l != nullptr && l->isVisible())
                    {
                        const bool sameColumn = std::abs (l->getBounds().getCentreX() - s->getBounds().getCentreX()) <= 1;
                        if (! sameColumn) continue;
                        if (l->getY() >= s->getBottom()) { found = true; seen.add (l->getText()); }
                        if (l->getBottom() <= s->getY()) noneAbove = false;
                        insidePanel = insidePanel && l->getBottom() <= 399
                                      && l->findColour (juce::Label::textColourId) == jb::gui::colours::creme;
                    }
                below = below && found;
            }
        for (auto* lc : head.getChildren())
            if (auto* l = dynamic_cast<juce::Label*> (lc); l != nullptr && l->isVisible())
                for (auto* c : head.getChildren())
                    if (c != l && c->isVisible() && dynamic_cast<juce::Label*> (c) == nullptr && l->getBounds().intersects (c->getBounds()))
                        insidePanel = false; // overlaps a knob, toggle or lamp
        check ((juce::String (what) + ": caption below every knob").toRawUTF8(), below);
        check ((juce::String (what) + ": no labels above the knobs").toRawUTF8(), noneAbove);
        check ((juce::String (what) + ": captions creme, clear of controls and panel edge").toRawUTF8(), insidePanel);
        std::printf ("  %s captions seen: %s\n", what, seen.joinIntoString (" | ").toRawUTF8());
        check ((juce::String (what) + ": captions read " + expected.joinIntoString (" ")).toRawUTF8(),
               [&] { // "FULL/SHORT": the short form is allowed when the full caption does not fit its column
                   for (auto& e : expected)
                   {
                       bool any = false;
                       for (auto& alt : juce::StringArray::fromTokens (e, "/", "")) any = any || seen.contains (alt);
                       if (! any) return false;
                   }
                   return seen.size() == expected.size(); }());
        check ((juce::String (what) + ": every head knob keeps its hover tooltip").toRawUTF8(), tipsOk);
    };
    ed->getHead().syncModelNow();
    checkCaptions ("Rectifier", { "GAIN", "BASS", "MID", "TREBLE", "PRESENCE", "MASTER" });
    if (auto* in = dynamic_cast<juce::Slider*> (findById (root, "input_trim")))
        check ("Input Cal tooltip keeps the dB equivalent", in->getTooltip().contains ("= 0.0 dB"));
    check ("Input Cal lives on the drawer, not on the Rectifier faceplate; 5150 Input slot is hidden",
           showing (findById (root, "input_trim")) && findById (ed->getHead(), "input_trim") == nullptr
               && findById (root, "m5150_input_trim") != nullptr && ! showing (findById (root, "m5150_input_trim")));
    check ("Preamp Out toggle is on the drawer, not on the head, default Full Amp",
           showing (findById (root, "preamp_out")) && findById (ed->getHead(), "preamp_out") == nullptr
               && getParam (p, "preamp_out") < 0.5f);
    if (auto* ma = dynamic_cast<juce::Slider*> (findById (root, "master")))
        check ("Master tooltip keeps the dB equivalent", ma->getTooltip().contains ("-6.0 dB"));

    // 3. Rectifier faceplate
    ed->getHead().syncModelNow();
    check ("Rectifier faceplate: channel/voice/presence shown, 5150 knobs hidden",
           showing (findById (root, "channel")) && showing (findById (root, "red_voice")) && showing (findById (root, "pre_presence"))
               && ! showing (findById (root, "lead_presence")) && ! showing (findById (root, "lead_resonance")));
    check ("Red Voice dimmed/disabled on Orange", ! findById (root, "red_voice")->isEnabled());

    // 4. Model toggle (= preamp_model) swaps to the 5150 faceplate, other params untouched
    if (auto* model = dynamic_cast<jb::gui::ToggleSwitch*> (findById (root, "preamp_model")))
        model->flip();
    check ("Model toggle sets preamp_model = 5150", getParam (p, "preamp_model") > 0.5f);
    check ("faceplate fade starts from Rectifier", ed->getHead().getFaceplateMix() < 0.5f);
    ed->getHead().syncModelNow();
    checkCaptions ("5150", { "RHY PRE/R PRE", "LEAD PRE/L PRE", "LOW", "MID", "HIGH", "RHY POST/R POST", "LEAD POST/L POST",
                             "RESONANCE/RESO", "PRESENCE/PRES" });
    check ("5150 faceplate: 5150 Presence/Resonance shown, Rectifier-only hidden",
           showing (findById (root, "lead_presence")) && showing (findById (root, "lead_resonance"))
               && ! showing (findById (root, "channel")) && ! showing (findById (root, "red_voice")) && ! showing (findById (root, "pre_presence")));
    {   // 5150 faceplate: no Master, no Input (shared Input Cal stays on the drawer)
        bool all5150 = true;
        for (auto* id : { "m5150_channel", "m5150_bright", "m5150_crunch", "m5150_rhythm_pre", "m5150_lead_pre",
                          "m5150_low", "m5150_mid", "m5150_high", "m5150_rhythm_post", "m5150_lead_post" })
            all5150 = all5150 && showing (findById (root, id));
        check ("5150 faceplate: RHY/LEAD, Bright, Crunch, Pre/Post per channel; no Master or Input knob",
               all5150 && ! showing (findById (root, "master")) && ! showing (findById (root, "pre_gain"))
                   && ! showing (findById (root, "m5150_input_trim")) && showing (findById (root, "input_trim"))
                   && findById (ed->getHead(), "input_trim") == nullptr);
        check ("Bright/Crunch dimmed/disabled on Lead", ! findById (root, "m5150_bright")->isEnabled() && ! findById (root, "m5150_crunch")->isEnabled());
        setParam (p, "m5150_channel", 0.0f);
        ed->getHead().syncModelNow();
        check ("Bright/Crunch enabled on Rhythm", findById (root, "m5150_bright")->isEnabled() && findById (root, "m5150_crunch")->isEnabled());
        setParam (p, "m5150_channel", 1.0f);
        ed->getHead().syncModelNow();
        ed->getDrawer().syncAmpNow(); // the drawer follows the model on its timer
        check ("pedalboard shows the 5150 set (boost, EQ, gate, cab)",
               showing (findById (root, "m5150_od_drive")) && showing (findById (root, "m5150_eq_6k4")) && showing (findById (root, "m5150_gate_threshold"))
                   && showing (findById (root, "m5150_cab_level")) && ! showing (findById (root, "od_drive")) && ! showing (findById (root, "eq_6k4")));
        check ("5150 Presence, Resonance and cab are enabled in Full Amp",
               findById (root, "lead_presence")->isEnabled() && findById (root, "lead_resonance")->isEnabled()
                   && findById (root, "m5150_cab_level")->isEnabled());
        if (auto* po = dynamic_cast<jb::gui::ToggleSwitch*> (findById (root, "preamp_out")))
            po->flip();
        ed->getHead().syncModelNow();
        ed->getDrawer().syncAmpNow();
        check ("Preamp Out greys out 5150 Presence, Resonance and cab",
               getParam (p, "preamp_out") > 0.5f
                   && ! findById (root, "lead_presence")->isEnabled() && ! findById (root, "lead_resonance")->isEnabled()
                   && ! findById (root, "m5150_cab_level")->isEnabled() && ! findById (root, "m5150_cab_enable")->isEnabled());
        setParam (p, "preamp_out", 0.0f);
        ed->getHead().syncModelNow();
        ed->getDrawer().syncAmpNow();
    }
    check ("Rectifier channel/voice retained under 5150", getParam (p, "channel") > 0.5f && getParam (p, "red_voice") < 0.5f);
    setParam (p, "preamp_model", 0.0f);
    ed->getHead().syncModelNow();
    check ("back to Rectifier faceplate", showing (findById (root, "channel")) && ! showing (findById (root, "lead_presence")));
    ed->getDrawer().syncAmpNow();
    check ("pedalboard back on the Rectifier set", showing (findById (root, "od_drive")) && ! showing (findById (root, "m5150_od_drive")));

    // 5. drawer + fixed-aspect scaling
    check ("drawer open by default, pedals visible", ed->isDrawerOpen() && showing (findById (root, "od_drive")) && showing (findById (root, "eq_6k4")));
    const auto openH = ed->getHeight();
    ed->setDrawerOpen (false);
    check ("drawer closed hides pedals and shrinks the window",
           ! showing (findById (root, "od_drive")) && ed->getHeight() < openH && ! p.editorDrawerOpen);
    check ("Input Cal stays visible when the drawer is closed", showing (findById (root, "input_trim")));
    check ("Preamp Out stays visible when the drawer is closed", showing (findById (root, "preamp_out")));
    check ("closed aspect = 1100 x closed design height",
           std::abs ((double) ed->getWidth() / ed->getHeight() - 1100.0 / JuicyboiAudioProcessorEditor::closedHeight) < 0.01);
    ed->setDrawerOpen (true);
    ed->setSize (1650, juce::roundToInt (1650.0 * JuicyboiAudioProcessorEditor::openHeight / 1100.0));
    check ("scaled 1.5x keeps the whole design visible",
           std::abs (root.getTransform().mat00 - 1.5f) < 1e-4f && root.getHeight() == JuicyboiAudioProcessorEditor::openHeight);

    // 6. all 7 EQ bands present on the pedal
    int bands = 0;
    for (auto* id : { "eq_100", "eq_200", "eq_400", "eq_800", "eq_1k6", "eq_3k2", "eq_6k4" })
        bands += showing (findById (root, id)) ? 1 : 0;
    check ("EQ pedal shows all 7 bands + level", bands == 7 && showing (findById (root, "eq_level")));

    // 7. offscreen paint
    ed->setSize (1100, JuicyboiAudioProcessorEditor::openHeight);
    const auto img = ed->createComponentSnapshot (ed->getLocalBounds());
    check ("offscreen snapshot renders", img.isValid() && img.getWidth() == 1100);

    // 8. the editor never touches session state: identical before/after open + resize + close
    {
        JuicyboiAudioProcessor q;
        juce::MemoryBlock before, after;
        q.getStateInformation (before);
        { std::unique_ptr<juce::AudioProcessorEditor> e2 (q.createEditor()); e2->setSize (900, 702); }
        q.getStateInformation (after);
        check ("opening/closing the editor leaves session state identical", before == after);
    }
    return failures;
}
