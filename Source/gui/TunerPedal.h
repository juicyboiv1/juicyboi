#pragma once

#include "Rev1Style.h"
#include "../dsp/Tuner.h"
#include <optional>

class JuicyboiAudioProcessor;

namespace jb::gui
{
/** Rev3.2 "TUNE" pedal: a small gloss-black enclosure with a dark
    display (fan of vertical LED segments across -50..+50 cents, a centre
    needle bar, flat/sharp arrows, a big note name) and a footswitch.
    Tuner on = output muted (TU-3 style). Original artwork (docs/tuner.md). */
class TunerPedal final : public juce::Component, private juce::Timer
{
public:
    TunerPedal (JuicyboiAudioProcessor&, juce::Component* popupParent);
    ~TunerPedal() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Screenshots / tests: draw this reading instead of the live one. */
    std::optional<jb::dsp::Tuner::Display> previewDisplay;
    /** Screenshots / tests: pick up the current reading and on/off state now (normally a 30 Hz timer). */
    void refreshNow() { timerCallback(); }
    /** Segments lit at the given deviation (tests): index 0..numSegments-1, centre = numSegments/2. */
    static int segmentForCents (double cents) noexcept;
    static constexpr int numSegments = 21;
    static constexpr double inTuneCents = 2.0;

    /** The engraved pedal name (just the model name, Rev3.2.1). */
    static juce::String nameplateText() { return "TUNE"; }
    static constexpr float ledRadius = 5.0f;
    /** Mute LED position / glow footprint in this pedal's coordinates; lit only while the tuner is on (tests). */
    juce::Point<float> ledCentre() const noexcept;
    juce::Rectangle<float> ledFootprint() const noexcept;
    bool isLedLit() const noexcept { return shownOn; }

    Footswitch& getFootswitch() noexcept { return footswitch; }
    ParamSlider& getReferenceKnob() noexcept { return refKnob; }

private:
    void timerCallback() override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float>);

    JuicyboiAudioProcessor& proc;
    Footswitch footswitch;
    ParamSlider refKnob;
    juce::Rectangle<float> displayArea;
    jb::dsp::Tuner::Display shown;
    bool shownOn = false;
};
} // namespace jb::gui
