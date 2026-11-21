#pragma once

#include "Rev1Style.h"
#include <array>

class JuicyboiAudioProcessor;

namespace jb::gui
{
/** One stompbox (or the cab side panel): draws its enclosure, title, knob captions
    and status LED; its controls are child components positioned in local coords. */
class Pedal final : public juce::Component
{
public:
    enum class Kind { stomp, sidePanel };
    Pedal (Kind, juce::Colour body, juce::Colour bodyDark, juce::Colour textColour, juce::String title);

    void paint (juce::Graphics&) override;

    struct Caption { juce::Rectangle<float> area; juce::String text; float size; };
    std::vector<Caption> captions;
    juce::Rectangle<float> titleArea;
    float titleSize = 30.0f;
    juce::Point<float> ledCentre;
    std::function<float()> ledAmount;            ///< 0 = off .. 1 = lit
    std::function<juce::Colour()> ledColour;
    void pollLed();

private:
    Kind kind;
    juce::Colour body, bodyDark, text;
    juce::String title;
    float shownLed = -1.0f;
    juce::Colour shownColour;
};

/** Drag-and-drop (or click-to-browse) IR target for one cab slot. */
class IRDropZone final : public juce::Component, public juce::FileDragAndDropTarget, public juce::TooltipClient
{
public:
    IRDropZone (JuicyboiAudioProcessor&, int slot);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { hovering = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { hovering = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;
    juce::String getTooltip() override;
    void refresh();

private:
    JuicyboiAudioProcessor& proc;
    const int slot;
    bool hovering = false;
    juce::TextButton clearButton { "x" };
    std::unique_ptr<juce::FileChooser> chooser;
};

/** The pedalboard drawer under the head: Boost, 7-band EQ and Gate stompboxes plus the
    Cab side panel. Closed = only the wooden drawer front shows; click it to open/close. */
class Pedalboard final : public juce::Component, private juce::Timer
{
public:
    static constexpr int designWidth = 1100, interiorHeight = 350, frontHeight = 62;
    static constexpr int openHeight = interiorHeight + frontHeight;

    Pedalboard (JuicyboiAudioProcessor&, juce::Component* popupParent);
    ~Pedalboard() override;

    void setOpen (bool);
    bool isOpen() const noexcept { return open; }
    std::function<void()> onToggle;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Front final : juce::Component, juce::TooltipClient
    {
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }
        juce::String getTooltip() override { return open ? "Close the pedalboard drawer" : "Open the pedalboard drawer (Boost, EQ, Gate, Cab)"; }
        std::function<void()> onClick;
        bool open = true;
    };

    JuicyboiAudioProcessor& proc;
    bool open = true;

    Pedal boost, eq, gate, cab;
    ParamSlider odDrive, odTone, odLevel;
    ToggleSwitch odType;
    Footswitch odSwitch;

    std::array<std::unique_ptr<ParamSlider>, 7> eqBands;
    ParamSlider eqLevel;
    Footswitch eqSwitch;

    ParamSlider gateThreshold, gateHold, gateRelease;
    Footswitch gateSwitch;

    ToggleSwitch cabOn;
    ParamSlider cabBlend, cabLevel;
    std::array<std::unique_ptr<IRDropZone>, 2> irZones;

    Front front;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pedalboard)
};
} // namespace jb::gui
