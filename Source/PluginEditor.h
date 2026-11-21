#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "gui/Rev1Style.h"
#include "gui/AmpHead.h"
#include "gui/Pedalboard.h"

/** juicyboi Rev1 editor: a vector-drawn amp head (Rectifier / 5150 faceplates) on top of
    an opening pedalboard drawer. Everything is laid out at a fixed design size
    (1100 wide) and scaled as a whole, so the window resizes with a fixed aspect ratio
    and stays crisp at any size. */
class JuicyboiAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    static constexpr int designWidth = 1100;
    static constexpr int drawerTop = jb::gui::AmpHead::designHeight + 6;
    static constexpr int closedHeight = drawerTop + jb::gui::Pedalboard::frontHeight;
    static constexpr int openHeight = drawerTop + jb::gui::Pedalboard::openHeight;
    static constexpr float minScale = 0.7f, maxScale = 1.6f;

    explicit JuicyboiAudioProcessorEditor (JuicyboiAudioProcessor&);
    ~JuicyboiAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void setDrawerOpen (bool shouldBeOpen);
    bool isDrawerOpen() const noexcept { return drawerOpen; }
    int getDesignHeight() const noexcept { return drawerOpen ? openHeight : closedHeight; }

    jb::gui::AmpHead& getHead() noexcept { return head; }
    juce::Component& getRoot() noexcept { return root; }

private:
    struct Root final : juce::Component
    {
        void paint (juce::Graphics&) override;
    };

    void applyConstraints();

    JuicyboiAudioProcessor& processor;
    jb::gui::Rev1LookAndFeel lookAndFeel;
    Root root;
    jb::gui::AmpHead head;
    jb::gui::Pedalboard drawer;
    bool drawerOpen = true;
    juce::TooltipWindow tooltipWindow { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuicyboiAudioProcessorEditor)
};
