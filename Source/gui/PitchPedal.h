#pragma once

#include "Rev1Style.h"

namespace jb::gui
{
/** Rev3.3 PITCH pedal pieces (the pedal itself is a slim jb::gui::Pedal on the
    board, laid out in Pedalboard.cpp; docs/pitch-shift.md).

    ModeButton: a small latching push button with an LED, bound to one of the
    pedal's bool mode parameters (WHAM / TRANS / HARM / POLY). Click = flip. */
class ModeButton final : public juce::Component, public juce::TooltipClient
{
public:
    ModeButton (juce::AudioProcessorValueTreeState&, const char* paramId, juce::String label,
                juce::String help, juce::Colour lit = colours::ledGreen);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::String getTooltip() override;
    bool isOn() const noexcept { return value > 0.5f; }
    void flip();
    const juce::String& getLabel() const noexcept { return label; }

    std::function<void()> onChange;

private:
    juce::RangedAudioParameter& param;
    juce::String label, help;
    juce::Colour litColour;
    float value = 0.0f;
    juce::ParameterAttachment attachment;
};

namespace pitchLayout
{
    /** Slim enclosure, the same footprint family as the TUNE pedal. */
    inline constexpr int width = 104;
    inline constexpr int knobSize = 40;
    inline constexpr float colX[2] = { 30.0f, 74.0f };
    inline constexpr float knobY[2] = { 38.0f, 102.0f };
    inline constexpr float captionDy = 25.0f;   // caption under the knob centre
    inline constexpr float valueDy = 35.0f;     // value readout under the caption
    inline constexpr int modeW = 44, modeH = 17;
    inline constexpr float modeY[2] = { 160.0f, 180.0f };
    inline constexpr float switchY = 238.0f;
    inline constexpr int switchSize = 46;
}
} // namespace jb::gui
