// Offscreen editor screenshots (no window): juicyboi_screenshots <dir> [scale]
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/CabSim.h"
#include "dsp/Filters.h"
#include <cmath>
#include <complex>
#include <functional>
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
    // Rev3.2.2: cab HPF / LPF knobs (set away from the bypass ends so they read)
    {
        set (p, "tuner_on", 0.0f);
        tp.previewDisplay.reset();
        tp.refreshNow();
        set (p, "cab_enable", 1.0f);
        set (p, "cab_hpf", 80.0f);
        set (p, "cab_lpf", 5000.0f);
        set (p, "preamp_model", 0.0f);
        ed.setDrawerOpen (true);
        ed.getHead().syncModelNow();
        ed.getDrawer().syncAmpNow();
        const auto cabRect = juce::Rectangle<int> (920, tb.getY(), 148, tb.getHeight()).expanded (8);
        shootPart (drawer, cabRect, "rev3.2.2-cab-hpf-lpf", 4.0f);
        shootPart (drawer, drawer.getLocalBounds(), "rev3.2.2-pedalboard-rectifier", 2.0f);
        shoot (ed, "rev3.2.2-editor-rectifier-drawer-open");
        set (p, "preamp_model", 1.0f);
        set (p, "m5150_cab_enable", 1.0f);
        set (p, "m5150_cab_hpf", 80.0f);
        set (p, "m5150_cab_lpf", 5000.0f);
        ed.getHead().syncModelNow();
        ed.getDrawer().syncAmpNow();
        shootPart (drawer, cabRect, "rev3.2.2-cab-hpf-lpf-5150", 4.0f);
        shootPart (drawer, drawer.getLocalBounds(), "rev3.2.2-pedalboard-5150", 2.0f);
        shoot (ed, "rev3.2.2-editor-5150-drawer-open");
        set (p, "preamp_model", 0.0f);
        ed.getHead().syncModelNow();
        ed.getDrawer().syncAmpNow();
    }
    // Filter-response plot (analytic digital biquads, 48 kHz)
    {
        using C = jb::dsp::CabSim;
        const double fs = 48000.0;
        const int W = 1400, H = 800, l = 70, r = 36, tpad = 48, b = 56;
        juce::Image img (juce::Image::ARGB, W, H, true);
        juce::Graphics g (img);
        g.fillAll (juce::Colour (0xff161616));
        const juce::Rectangle<float> plot ((float) l, (float) tpad, (float) (W - l - r), (float) (H - tpad - b));
        g.setColour (juce::Colour (0xff222222));
        g.fillRect (plot);
        const float fMin = 20.0f, fMax = 20000.0f, dbMin = -36.0f, dbMax = 6.0f;
        auto xOf = [&] (double f) { return plot.getX() + plot.getWidth() * (float) (std::log (f / fMin) / std::log (fMax / fMin)); };
        auto yOf = [&] (double db) { return plot.getY() + plot.getHeight() * (float) ((dbMax - db) / (dbMax - dbMin)); };
        g.setColour (juce::Colour (0xff333333));
        for (auto f : { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 })
            g.drawVerticalLine (juce::roundToInt (xOf ((double) f)), plot.getY(), plot.getBottom());
        for (int db = -36; db <= 6; db += 6)
            g.drawHorizontalLine (juce::roundToInt (yOf ((double) db)), plot.getX(), plot.getRight());
        g.setColour (juce::Colour (0xff888888));
        g.drawRect (plot, 1.0f);
        g.setFont (juce::FontOptions (12.0f));
        auto labelHz = [] (int f) -> juce::String
        {
            if (f >= 1000) return juce::String (f / 1000) + " k";
            return juce::String (f);
        };
        for (auto f : { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 })
            g.drawText (labelHz (f), juce::Rectangle<float> (xOf ((double) f) - 22.0f, plot.getBottom() + 4.0f, 44.0f, 16.0f),
                        juce::Justification::centred, false);
        for (int db = -36; db <= 6; db += 6)
            g.drawText ((db > 0 ? "+" : "") + juce::String (db),
                        juce::Rectangle<float> (4.0f, yOf ((double) db) - 8.0f, (float) l - 8.0f, 16.0f),
                        juce::Justification::centredRight, false);
        auto mag = [&] (const jb::dsp::BiquadCoeffs& c, double f)
        {
            const auto w = std::complex<double> (0.0, 2.0 * juce::MathConstants<double>::pi * f / fs);
            const auto z = std::exp (w), z2 = z * z;
            const auto num = (double) c.b0 * z2 + (double) c.b1 * z + (double) c.b2;
            const auto den = z2 + (double) c.a1 * z + (double) c.a2;
            return 20.0 * std::log10 (std::max (std::abs (num / den), 1.0e-12));
        };
        auto stroke = [&] (const juce::Colour& col, const std::function<double (double)>& dbOf)
        {
            juce::Path path;
            bool first = true;
            for (int i = 0; i <= 400; ++i)
            {
                const auto f = (double) fMin * std::pow ((double) fMax / fMin, (double) i / 400.0);
                const auto y = yOf (juce::jlimit ((double) dbMin, (double) dbMax, dbOf (f)));
                if (first) path.startNewSubPath (xOf (f), y);
                else       path.lineTo (xOf (f), y);
                first = false;
            }
            g.setColour (col);
            g.strokePath (path, juce::PathStrokeType (2.2f));
        };
        const auto hp80  = jb::dsp::BiquadCoeffs::highPass (fs, 80.0,   (double) C::butterworthQ);
        const auto lp5k  = jb::dsp::BiquadCoeffs::lowPass  (fs, 5000.0, (double) C::butterworthQ);
        const auto hp20  = jb::dsp::BiquadCoeffs::highPass (fs, 20.0,   (double) C::butterworthQ);
        const auto lp20k = jb::dsp::BiquadCoeffs::lowPass  (fs, 20000.0,(double) C::butterworthQ);
        stroke (juce::Colour (0xff6ea8ff), [&] (double f) { return mag (hp80, f); });
        stroke (juce::Colour (0xffff8a4a), [&] (double f) { return mag (lp5k, f); });
        stroke (juce::Colour (0xff9dde6e), [&] (double f) { return mag (hp80, f) + mag (lp5k, f); });
        stroke (juce::Colour (0xff888888), [&] (double f) { return mag (hp20, f) + mag (lp20k, f); });
        g.setFont (juce::FontOptions (16.0f));
        g.setColour (juce::Colours::white);
        g.drawText ("Cab HPF / LPF  (12 dB/oct Butterworth, 48 kHz)",
                    juce::Rectangle<float> (plot.getX(), 8.0f, plot.getWidth(), 28.0f),
                    juce::Justification::centredLeft, false);
        g.setFont (juce::FontOptions (13.0f));
        const struct { juce::Colour c; const char* t; } legs[] = {
            { juce::Colour (0xff6ea8ff), "HPF 80 Hz" },
            { juce::Colour (0xffff8a4a), "LPF 5 kHz" },
            { juce::Colour (0xff9dde6e), "HPF 80 + LPF 5 kHz" },
            { juce::Colour (0xff888888), "defaults 20 Hz / 20 kHz (bypassed in the plugin)" },
        };
        float lx = plot.getX() + 8.0f, ly = plot.getY() + 10.0f;
        for (auto& e : legs)
        {
            g.setColour (e.c);
            g.fillRect (lx, ly + 3.0f, 18.0f, 4.0f);
            g.setColour (juce::Colour (0xffdddddd));
            g.drawText (e.t, juce::Rectangle<float> (lx + 24.0f, ly - 4.0f, 420.0f, 18.0f),
                        juce::Justification::centredLeft, false);
            ly += 18.0f;
        }
        g.setColour (juce::Colour (0xff888888));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("Hz", juce::Rectangle<float> (plot.getRight() - 30.0f, plot.getBottom() + 22.0f, 40.0f, 16.0f),
                    juce::Justification::centredLeft, false);
        g.drawText ("dB", juce::Rectangle<float> (8.0f, 14.0f, 40.0f, 16.0f), juce::Justification::centredLeft, false);
        const auto f = dir.getChildFile ("rev3.2.2-cab-filters-response.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat png;
        const bool ok = os.openedOk() && png.writeImageToStream (img, os);
        std::printf ("%s %dx%d %s\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight(), ok ? "ok" : "FAIL");
    }
    return 0;
}
