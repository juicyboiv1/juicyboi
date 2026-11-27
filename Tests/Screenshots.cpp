// Offscreen editor screenshots (no window): juicyboi_screenshots <dir> [scale]
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File dir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1]))
                                   : juce::File::getCurrentWorkingDirectory());
    const float scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 1.0f;
    const juce::String suffix = std::abs (scale - 1.0f) > 1.0e-6f ? "@" + juce::String (scale, 1) + "x" : juce::String();
    dir.createDirectory();

    auto shoot = [&] (JuicyboiAudioProcessorEditor& ed, const juce::String& name)
    {
        const auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, scale);
        const auto f = dir.getChildFile (name + suffix + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat png;
        const bool ok = os.openedOk() && png.writeImageToStream (img, os);
        std::printf ("%s %dx%d %s\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight(), ok ? "ok" : "FAIL");
    };
    auto set = [] (JuicyboiAudioProcessor& p, const char* id, float v)
    {
        auto* prm = p.apvts.getParameter (id);
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };

    JuicyboiAudioProcessor p;
    set (p, "od_enable", 1.0f);
    set (p, "eq_enable", 1.0f);
    set (p, "cab_enable", 1.0f);
    const std::pair<const char*, float> eq[] = { { "eq_100", 3.0f }, { "eq_200", 1.0f }, { "eq_400", -2.0f }, { "eq_800", -4.0f },
                                                 { "eq_1k6", 1.5f }, { "eq_3k2", 4.0f }, { "eq_6k4", -3.0f } };
    for (auto& [id, v] : eq)
        set (p, id, v);
    std::unique_ptr<juce::AudioProcessorEditor> base (p.createEditor());
    auto& ed = dynamic_cast<JuicyboiAudioProcessorEditor&> (*base);

    ed.setDrawerOpen (false);
    ed.getHead().syncModelNow();
    shoot (ed, "rev2.4-rectifier");

    set (p, "preamp_model", 1.0f);
    set (p, "lead_presence", 5.0f);
    set (p, "lead_resonance", 4.0f);
    set (p, "m5150_gate_enable", 1.0f); // the 5150 has its own pedalboard settings (Rev2.4)
    ed.getHead().syncModelNow();
    ed.getDrawer().syncAmpNow();
    shoot (ed, "rev2.4-5150-lead");

    set (p, "m5150_channel", 0.0f);
    set (p, "m5150_crunch", 1.0f);
    ed.getHead().syncModelNow();
    shoot (ed, "rev2.4-5150-rhythm-crunch");
    set (p, "m5150_channel", 1.0f);
    ed.getHead().syncModelNow();

    ed.setDrawerOpen (true);
    shoot (ed, "rev2.4-5150-drawer-open");

    set (p, "preamp_out", 1.0f);
    ed.getHead().syncModelNow();
    ed.getDrawer().syncAmpNow();
    shoot (ed, "preamp-out-5150-drawer-open");
    set (p, "preamp_out", 0.0f);
    ed.getHead().syncModelNow();
    ed.getDrawer().syncAmpNow();

    set (p, "preamp_model", 0.0f);
    ed.getHead().syncModelNow();
    ed.getDrawer().syncAmpNow();
    shoot (ed, "rev2.4-rectifier-drawer-open");

    // Rev3.2: tuner pedal (preview readings; the live display needs audio) and GEQ-7
    auto shootPart = [&] (juce::Component& c, juce::Rectangle<int> area, const juce::String& name, float sc)
    {
        const auto img = c.createComponentSnapshot (area, true, sc);
        const auto f = dir.getChildFile (name + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat png;
        const bool ok = os.openedOk() && png.writeImageToStream (img, os);
        std::printf ("%s %dx%d %s\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight(), ok ? "ok" : "FAIL");
    };
    auto& drawer = ed.getDrawer();
    auto& tp = drawer.getTunerPedal();
    const auto tb = tp.getBounds();
    const auto eqRect = juce::Rectangle<int> (396, tb.getY(), 304, tb.getHeight()).expanded (6);
    const auto tunerRect = tb.expanded (6);
    auto reading = [] (int note, double cents)
    {
        jb::dsp::Tuner::Display d;
        d.hasPitch = true; d.midiNote = note; d.cents = cents;
        d.hz = 440.0 * std::pow (2.0, (note - 69 + cents / 100.0) / 12.0); d.rawHz = d.hz;
        return d;
    };

    set (p, "tuner_on", 1.0f);
    tp.previewDisplay = reading (40, 0.4);           // E2 in tune
    tp.refreshNow();
    shoot (ed, "rev3.2-editor-tuner-on");
    shootPart (drawer, tunerRect, "rev3.2-tuner-E2-in-tune", 3.0f);
    tp.previewDisplay = reading (45, -17.0);          // A2 flat
    tp.refreshNow();
    shootPart (drawer, tunerRect, "rev3.2-tuner-A2-flat", 3.0f);
    tp.previewDisplay = reading (61, 31.0);           // C#4 sharp
    tp.refreshNow();
    shootPart (drawer, tunerRect, "rev3.2-tuner-Csharp4-sharp", 3.0f);
    tp.previewDisplay = jb::dsp::Tuner::Display {};   // no signal
    tp.refreshNow();
    shootPart (drawer, tunerRect, "rev3.2-tuner-no-signal", 3.0f);
    set (p, "tuner_on", 0.0f);
    tp.previewDisplay.reset();
    tp.refreshNow();
    shootPart (drawer, tunerRect, "rev3.2-tuner-off", 3.0f);
    shootPart (drawer, eqRect, "rev3.2-geq7", 3.0f);
    shoot (ed, "rev3.2-editor-drawer-open");
    // Rev3.2.1: jagged metal amp badge close-ups (6x), full UI and the pedalboard (no "juicyboi" on any pedal)
    {
        const auto badge = jb::gui::AmpHead::badgeBounds().expanded (10.0f).toNearestInt();
        set (p, "tuner_on", 0.0f);
        ed.setDrawerOpen (false);
        set (p, "preamp_model", 0.0f);
        ed.getHead().syncModelNow();
        shootPart (ed.getHead(), badge, "rev3.2.1-badge-rectifier", 6.0f);
        shoot (ed, "rev3.2.1-editor-rectifier");
        set (p, "preamp_model", 1.0f);
        ed.getHead().syncModelNow();
        shootPart (ed.getHead(), badge, "rev3.2.1-badge-5150", 6.0f);
        shoot (ed, "rev3.2.1-editor-5150");
        ed.setDrawerOpen (true);
        ed.getDrawer().syncAmpNow();
        shoot (ed, "rev3.2.1-editor-5150-drawer-open");
        shootPart (drawer, drawer.getLocalBounds(), "rev3.2.1-pedalboard-5150", 2.0f);
        set (p, "preamp_model", 0.0f);
        ed.getHead().syncModelNow();
        ed.getDrawer().syncAmpNow();
        shoot (ed, "rev3.2.1-editor-rectifier-drawer-open");
        shootPart (drawer, drawer.getLocalBounds(), "rev3.2.1-pedalboard-rectifier", 2.0f);
        set (p, "tuner_on", 1.0f);
        tp.previewDisplay = reading (40, 0.4);
        tp.refreshNow();
        shootPart (drawer, tunerRect, "rev3.2.1-tuner-on", 4.0f);
        set (p, "tuner_on", 0.0f);
        tp.previewDisplay.reset();
        tp.refreshNow();
        shootPart (drawer, tunerRect, "rev3.2.1-tuner-off", 4.0f);
        shootPart (drawer, eqRect, "rev3.2.1-geq7", 4.0f);
    }
    return 0;
}
