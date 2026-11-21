// Offscreen editor screenshots (no window): juicyboi_screenshots <dir> [scale]
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
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
    shoot (ed, "rev1-rectifier");

    set (p, "preamp_model", 1.0f);
    set (p, "lead_presence", 5.0f);
    set (p, "lead_resonance", 4.0f);
    ed.getHead().syncModelNow();
    shoot (ed, "rev1-5150");

    ed.setDrawerOpen (true);
    shoot (ed, "rev1-5150-drawer-open");

    set (p, "preamp_model", 0.0f);
    ed.getHead().syncModelNow();
    shoot (ed, "rev1-rectifier-drawer-open");
    return 0;
}
