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

    juce::ToggleButton gateEnable { "Gate" };
    std::unique_ptr<APVTS::ButtonAttachment> gateEnableAttachment;
    GateLed gateLed { processor };

    juce::ToggleButton cabEnable { "Cab" };
    std::unique_ptr<APVTS::ButtonAttachment> cabEnableAttachment;
    juce::ComboBox sm57Axis, md421Axis;
    juce::Label sm57Label, md421Label, cabHint;
    std::unique_ptr<APVTS::ComboBoxAttachment> sm57AxisAttachment, md421AxisAttachment;

    juce::ToggleButton odEnable { "Boost" }, bypass { "Bypass" };
    std::unique_ptr<APVTS::ButtonAttachment> odEnableAttachment, bypassAttachment;

    juce::ComboBox channel;
    juce::Label channelLabel;
    std::unique_ptr<APVTS::ComboBoxAttachment> channelAttachment;

    juce::GroupComponent inGroup { {}, "Input" }, odGroup { {}, "Boost" },
                         preGroup { {}, "Preamp" }, outGroup { {}, "Output" },
                         cabGroup { {}, "Cab  (Mesa-style 4x12 IR - user-supplied)" },
                         gateGroup { {}, "Gate" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuicyboiAudioProcessorEditor)
};
