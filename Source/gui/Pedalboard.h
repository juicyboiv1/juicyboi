#pragma once

#include "Rev1Style.h"
#include "TunerPedal.h"
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
    /** The engraved pedal name ("BOOST", "GEQ-7", ...): never the plugin name (Rev3.2.1). */
    const juce::String& getTitle() const noexcept { return title; }

    struct Caption { juce::Rectangle<float> area; juce::String text; float size; };
    std::vector<Caption> captions;
    juce::Rectangle<float> titleArea;
    float titleSize = 30.0f;
    juce::Point<float> ledCentre;
    std::function<float()> ledAmount;            ///< 0 = off .. 1 = lit
    std::function<juce::Colour()> ledColour;
    /** Re-read the LED state; if it changed, repaint the WHOLE pedal (see docs/led-repaint.md). */
    void pollLed();
    /** Region invalidated when the LED changes: the whole pedal. */
    juce::Rectangle<int> ledRepaintArea() const noexcept { return getLocalBounds(); }
    /** The LED bezel + glow footprint in this pedal's coordinates (tests). */
    juce::Rectangle<float> ledFootprint() const noexcept;
    /** Forget what was last drawn so the next poll repaints (shown / resized). */
    float getShownLed() const noexcept { return shownLed; }
    void invalidateLed() noexcept { shownLed = -1.0f; shownColour = juce::Colour (0x00000000); }
    void visibilityChanged() override { invalidateLed(); pollLed(); }
    void parentHierarchyChanged() override { invalidateLed(); }

private:
    Kind kind;
    juce::Colour body, bodyDark, text;
    juce::String title;
    float shownLed = -1.0f;
    juce::Colour shownColour { 0x00000000 };
};

/** Drag-and-drop (or click-to-browse) IR target for one cab slot. */
class IRDropZone final : public juce::Component, public juce::FileDragAndDropTarget, public juce::TooltipClient
{
public:
    IRDropZone (JuicyboiAudioProcessor&, int amp, int slot);
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
    const int amp, slot;
    bool hovering = false;
    juce::TextButton clearButton { "x" };
    std::unique_ptr<juce::FileChooser> chooser;
};

/** The pedalboard drawer under the head: Tuner (Rev3.2, shared), Boost, GEQ-7 and Gate
    stompboxes plus the Cab side panel. Closed = only the wooden drawer front shows; click it to open/close.
    Rev2.4: there is one control set per amp (docs/per-amp-state.md); the set of the amp
    selected by `preamp_model` is shown, the other is hidden but stays bound.
    A small Input Cal trim and a Full Amp / Preamp Out toggle sit on the
    drawer front (shared by both amps, always visible). */
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

public:
    /** Which amp's controls are showing (tests). */
    int getShownAmp() const noexcept { return shownAmp; }
    /** Follow preamp_model now instead of on the next timer tick (tests / screenshots). */
    void syncAmpNow() { showAmp (activeAmp()); syncPreampOutDim(); }
private:

    struct Front final : juce::Component, juce::TooltipClient
    {
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }
        juce::String getTooltip() override { return open ? "Close the pedalboard drawer" : "Open the pedalboard drawer (Tuner, Boost, EQ, Gate, Cab)"; }
        std::function<void()> onClick;
        bool open = true;
    };

    JuicyboiAudioProcessor& proc;
    bool open = true;

    Pedal boost, eq, gate, cab;
    TunerPedal tuner;   // shared by both amps (Rev3.2)

public:
    TunerPedal& getTunerPedal() noexcept { return tuner; }
    Pedal& getGatePedal() noexcept { return gate; }
    Pedal& getBoostPedal() noexcept { return boost; }
    Pedal& getEqPedal() noexcept { return eq; }
    Pedal& getCabPedal() noexcept { return cab; }
    /** Run the LED poll of every pedal now (normally a 30 Hz timer, plus footswitch changes). */
    void pollLedsNow();
private:

    struct AmpControls
    {
        AmpControls (JuicyboiAudioProcessor&, int amp);
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
        std::vector<juce::Component*> all();
        std::vector<juce::Component*> cabControls();
    };
    std::array<std::unique_ptr<AmpControls>, 2> sets;
    int shownAmp = -1;
    int activeAmp() const;
    void showAmp (int amp);

    Front front;
    // Shared input calibration (always visible on the drawer front).
    ParamSlider inputCal;
    juce::Label inputCalLabel;
    // Hidden: keeps a bound control for unused m5150_input_trim (stable host index).
    ParamSlider unused5150Input;
    ToggleSwitch preampOut;
    juce::Label preampOutLabel;

    void syncPreampOutDim();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pedalboard)
};
} // namespace jb::gui
