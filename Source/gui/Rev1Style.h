#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

/** Rev1 amp-head GUI: shared palette, LookAndFeel and parameter-bound controls.
    Everything is drawn procedurally (no bitmap assets) so it stays crisp at any scale. */
namespace jb::gui
{
namespace colours
{
    inline const juce::Colour creme       { 0xfff2e8cc };
    inline const juce::Colour cremeDim    { 0xffc9bd9a };
    inline const juce::Colour ink         { 0xff141414 };
    inline const juce::Colour panelBlack  { 0xff0f0f0f };
    inline const juce::Colour tolex       { 0xff151515 };
    inline const juce::Colour chromeHi    { 0xfff7f7f4 };
    inline const juce::Colour chromeMid   { 0xffb9b9b4 };
    inline const juce::Colour chromeLo    { 0xff55554f };
    inline const juce::Colour blonde      { 0xffd8bf8a };
    inline const juce::Colour blondeHi    { 0xffefe0b5 };
    inline const juce::Colour blondeLo    { 0xff8d7446 };
    inline const juce::Colour blondeGap   { 0xff3b2e1e };
    inline const juce::Colour boostGreen  { 0xff2f5a36 };
    inline const juce::Colour boostGreenD { 0xff1d3a22 };
    inline const juce::Colour eqCreme     { 0xffeee4c8 };
    inline const juce::Colour eqCremeD    { 0xffcfc3a2 };
    inline const juce::Colour gateBlack   { 0xff1f1f1f };
    inline const juce::Colour ledGreen    { 0xff5cf07a };
    inline const juce::Colour ledOff      { 0xff1f3322 };
    inline const juce::Colour ledRed      { 0xffe0542a };
    inline const juce::Colour wood        { 0xff5c3518 };
    inline const juce::Colour woodHi      { 0xff8c5a30 };
    inline const juce::Colour woodLo      { 0xff2e190a };
    inline const juce::Colour felt        { 0xff141414 };
}

/** Sans-serif UI font (bold optional). */
juce::Font uiFont (float size, bool bold = true);

void drawChromeScrew (juce::Graphics&, juce::Point<float> centre, float radius);
void fillChrome (juce::Graphics&, const juce::Path&, juce::Rectangle<float> area, bool vertical = true);

/** Knob styles are chosen per slider through the "jbStyle" property. */
class Rev1LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    Rev1LookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 7; }

    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
    juce::Font getSliderPopupFont (juce::Slider&) override { return uiFont (13.0f); }
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>&) override;
};

/** Rotary knob or vertical fader bound to a parameter. Unlabelled on the head, so the
    hover tooltip names the control and its value (level knobs add the dB equivalent). */
class ParamSlider final : public juce::Slider
{
public:
    enum class Style { ampKnob, pedalKnob, fader };
    using TipFn = std::function<juce::String (double)>;

    ParamSlider (juce::AudioProcessorValueTreeState&, const char* paramId, Style, TipFn tip = {});
    juce::String getTooltip() override;
    juce::String extraTip; ///< appended to the generic "Name: value" tooltip

private:
    juce::RangedAudioParameter& param;
    TipFn tipFn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** Chrome bat toggle for a two-state parameter (bool or 2-way choice). Draws small
    position marks at both ends. `oneAtTop` (or right, when horizontal) chooses which end = index 1. */
class ToggleSwitch final : public juce::Component, public juce::TooltipClient
{
public:
    ToggleSwitch (juce::AudioProcessorValueTreeState&, const char* paramId, juce::String markForOne,
                  juce::String markForZero, bool oneAtTop = true, bool horizontal = false,
                  juce::Colour markColour = colours::creme);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::String getTooltip() override;
    bool isOne() const noexcept { return value > 0.5f; }
    void flip(); ///< same as a click (one undoable gesture)

    std::function<void()> onChange;
    juce::String extraTip;

private:
    juce::RangedAudioParameter& param;
    juce::String markOne, markZero;
    bool oneAtTop, horizontal;
    juce::Colour markColour;
    float value = 0.0f;
    juce::ParameterAttachment attachment;
};

/** Chrome stomp footswitch bound to a bool on/bypass parameter. */
class Footswitch final : public juce::Component, public juce::TooltipClient
{
public:
    Footswitch (juce::AudioProcessorValueTreeState&, const char* paramId, juce::String name);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::String getTooltip() override;
    bool isOn() const noexcept { return value > 0.5f; }
    std::function<void()> onChange;

private:
    juce::RangedAudioParameter& param;
    juce::String name;
    float value = 0.0f;
    bool pressed = false;
    juce::ParameterAttachment attachment;
};

void drawLed (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour lit, float amount);
} // namespace jb::gui
