#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <array>

/** V1 editor with a green/gold LookAndFeel for clearer rotary knobs. */
class JuicyboiAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit JuicyboiAudioProcessorEditor (JuicyboiAudioProcessor&);
    ~JuicyboiAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    /** Dark panel, gold dials, green accents — higher contrast than stock LAF. */
    struct JuicyboiLookAndFeel final : juce::LookAndFeel_V4
    {
        JuicyboiLookAndFeel()
        {
            setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff12160f));
            setColour (juce::GroupComponent::outlineColourId,     juce::Colour (0xff3d5c2e));
            setColour (juce::GroupComponent::textColourId,        juce::Colour (0xffd4af37));
            setColour (juce::Label::textColourId,                 juce::Colour (0xffe8e0c8));
            setColour (juce::ToggleButton::textColourId,          juce::Colour (0xffe8e0c8));
            setColour (juce::ToggleButton::tickColourId,          juce::Colour (0xffd4af37));
            setColour (juce::ToggleButton::tickDisabledColourId,  juce::Colour (0xff5a6b4a));
            setColour (juce::ComboBox::backgroundColourId,        juce::Colour (0xff1a2214));
            setColour (juce::ComboBox::outlineColourId,           juce::Colour (0xffd4af37));
            setColour (juce::ComboBox::textColourId,              juce::Colour (0xffe8e0c8));
            setColour (juce::ComboBox::arrowColourId,             juce::Colour (0xffd4af37));
            setColour (juce::PopupMenu::backgroundColourId,       juce::Colour (0xff1a2214));
            setColour (juce::PopupMenu::textColourId,             juce::Colour (0xffe8e0c8));
            setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2f4a22));
            setColour (juce::Slider::textBoxTextColourId,         juce::Colour (0xfff0e6c8));
            setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colour (0xff1a2214));
            setColour (juce::Slider::textBoxOutlineColourId,      juce::Colour (0xff3d5c2e));
            setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (0xffd4af37));
            setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2a3d20));
            setColour (juce::Slider::thumbColourId,               juce::Colour (0xfff5e6a3));
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, const float rotaryStartAngle,
                               const float rotaryEndAngle, juce::Slider& slider) override
        {
            const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                                    .reduced (6.0f);
            const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const auto centre = bounds.getCentre();
            const auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
            const auto lineW = juce::jmax (2.5f, radius * 0.09f);
            const auto arcR = radius - lineW * 0.5f;

            // Track
            juce::Path backgroundArc;
            backgroundArc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                                         rotaryStartAngle, rotaryEndAngle, true);
            g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
            g.strokePath (backgroundArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                                               juce::PathStrokeType::rounded));

            // Value arc (gold)
            if (slider.isEnabled())
            {
                juce::Path valueArc;
                valueArc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                                        rotaryStartAngle, toAngle, true);
                g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
                g.strokePath (valueArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                                              juce::PathStrokeType::rounded));
            }

            // Knob body
            const auto knobR = arcR - lineW * 1.6f;
            juce::ColourGradient body (juce::Colour (0xff3a5c28), centre.x, centre.y - knobR,
                                       juce::Colour (0xff1a2812), centre.x, centre.y + knobR, false);
            g.setGradientFill (body);
            g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);

            g.setColour (juce::Colour (0xffd4af37).withAlpha (0.85f));
            g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.5f);

            // Pointer
            juce::Path pointer;
            const float pointerLen = knobR * 0.72f;
            const float pointerThick = lineW * 0.9f;
            pointer.addRoundedRectangle (-pointerThick * 0.5f, -pointerLen, pointerThick, pointerLen, 1.0f);
            g.setColour (slider.findColour (juce::Slider::thumbColourId));
            g.fillPath (pointer, juce::AffineTransform::rotation (toAngle).translated (centre.x, centre.y));
        }

        /** Vertical EQ faders: dark slot, gold fill from the 0 dB centre,
            green/gold cap. Other linear sliders fall back to the stock look. */
        void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               juce::Slider::SliderStyle style, juce::Slider& slider) override
        {
            if (style != juce::Slider::LinearVertical)
            {
                LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
                return;
            }

            const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
            const auto cx = b.getCentreX();
            const auto top = b.getY() + 4.0f, bottom = b.getBottom() - 4.0f;
            const auto zeroY = (float) slider.getPositionOfValue (0.0);
            const auto slotW = 5.0f;

            // Tick marks every 5 dB (±15)
            g.setColour (juce::Colour (0xff3d5c2e));
            for (int db = -15; db <= 15; db += 5)
            {
                const auto ty = (float) slider.getPositionOfValue ((double) db);
                const auto tw = db == 0 ? 11.0f : 7.0f;
                g.fillRect (cx - slotW - tw, ty - 0.5f, tw - 2.0f, 1.0f);
                g.fillRect (cx + slotW + 2.0f, ty - 0.5f, tw - 2.0f, 1.0f);
            }

            // Slot
            g.setColour (juce::Colour (0xff0b0e09));
            g.fillRoundedRectangle (cx - slotW * 0.5f, top, slotW, bottom - top, 2.5f);
            g.setColour (juce::Colour (0xff2a3d20));
            g.drawRoundedRectangle (cx - slotW * 0.5f, top, slotW, bottom - top, 2.5f, 1.0f);

            // Gold fill from 0 dB to the thumb
            if (slider.isEnabled())
            {
                const auto y0 = juce::jmin (zeroY, sliderPos), y1 = juce::jmax (zeroY, sliderPos);
                g.setColour (juce::Colour (0xffd4af37));
                g.fillRoundedRectangle (cx - slotW * 0.5f + 1.0f, y0, slotW - 2.0f, juce::jmax (1.0f, y1 - y0), 1.5f);
            }

            // Fader cap
            const auto capW = juce::jmin (30.0f, b.getWidth() - 6.0f), capH = 12.0f;
            const auto cap = juce::Rectangle<float> (capW, capH).withCentre ({ cx, sliderPos });
            juce::ColourGradient body (juce::Colour (0xff3a5c28), cx, cap.getY(),
                                       juce::Colour (0xff1a2812), cx, cap.getBottom(), false);
            g.setGradientFill (body);
            g.fillRoundedRectangle (cap, 2.5f);
            g.setColour (juce::Colour (0xffd4af37).withAlpha (slider.isEnabled() ? 0.9f : 0.4f));
            g.drawRoundedRectangle (cap, 2.5f, 1.2f);
            g.setColour (juce::Colour (0xfff5e6a3));
            g.fillRect (cap.getX() + 3.0f, sliderPos - 0.75f, cap.getWidth() - 6.0f, 1.5f);
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuicyboiLookAndFeel)
    };

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const char* paramId, const juce::String& text);
    juce::Rectangle<int> layoutCab (juce::Rectangle<int>);
    void layoutGate (juce::Rectangle<int>);
    void layoutEq (juce::Rectangle<int>);
    void setupFader (Knob&, const char* paramId, const juce::String& text);
    void updateEqEnablement();

    /** Dual-colour gate status LED: green = open, orange-red = closed,
        dim when the gate is bypassed. Polls the processor at 30 Hz. */
    struct GateLed final : juce::Component, juce::Timer
    {
        explicit GateLed (JuicyboiAudioProcessor& p) : proc (p) { startTimerHz (30); }
        void timerCallback() override;
        void paint (juce::Graphics&) override;

        JuicyboiAudioProcessor& proc;
        float shown = 1.0f;
        bool on = false;
    };

    /** Drag-and-drop IR target for one cab slot (.wav/.aif/.aiff/.flac). */
    struct IRDropZone final : juce::Component, juce::FileDragAndDropTarget
    {
        IRDropZone (JuicyboiAudioProcessor& p, int s);
        void paint (juce::Graphics&) override;
        void resized() override;
        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void fileDragEnter (const juce::StringArray&, int, int) override { hovering = true; repaint(); }
        void fileDragExit (const juce::StringArray&) override { hovering = false; repaint(); }
        void filesDropped (const juce::StringArray& files, int, int) override;
        void refresh();

        JuicyboiAudioProcessor& proc;
        const int slot;
        bool hovering = false;
        juce::TextButton clearButton { "x" };
    };

    std::array<std::unique_ptr<IRDropZone>, jb::dsp::CabSim::numSlots> irZones;

    JuicyboiAudioProcessor& processor;
    JuicyboiLookAndFeel lookAndFeel;

    Knob input, odDrive, odTone, odLevel, gain, bass, mid, treble, presence, master, cabBlend, cabLevel;
    Knob gateThreshold, gateHold, gateRelease;

    // Graphic EQ (post-ToneStack / pre-Cab): 7 vertical band faders + Level
    std::array<Knob, jb::dsp::GraphicEq::numBands> eqBands;
    Knob eqLevel;
    juce::ToggleButton eqEnable { "On" };
    std::unique_ptr<APVTS::ButtonAttachment> eqEnableAttachment;

    juce::ToggleButton gateEnable { "Gate" };
    std::unique_ptr<APVTS::ButtonAttachment> gateEnableAttachment;
    GateLed gateLed { processor };

    juce::ToggleButton cabEnable { "Cab" };
    std::unique_ptr<APVTS::ButtonAttachment> cabEnableAttachment;
    juce::Label cabHint;

    juce::ToggleButton odEnable { "Boost" }, bypass { "Bypass" };
    std::unique_ptr<APVTS::ButtonAttachment> odEnableAttachment, bypassAttachment;

    juce::ComboBox channel;
    juce::Label channelLabel;
    std::unique_ptr<APVTS::ComboBoxAttachment> channelAttachment;

    // Red-channel voicing A/B (v1 = original tight Red, v2 = max-gain Red)
    juce::ComboBox redVoice;
    juce::Label redVoiceLabel;
    std::unique_ptr<APVTS::ComboBoxAttachment> redVoiceAttachment;
    void updateRedVoiceEnablement();

    juce::GroupComponent inGroup { {}, "Input" }, odGroup { {}, "Boost" },
                         preGroup { {}, "Preamp" }, outGroup { {}, "Output" },
                         cabGroup { {}, "Cab  (Mesa-style 4x12 IR - user-supplied)" },
                         gateGroup { {}, "Gate" }, eqGroup { {}, "Graphic EQ" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuicyboiAudioProcessorEditor)
};
