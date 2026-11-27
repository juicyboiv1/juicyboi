#pragma once

#include "Rev1Style.h"
#include <array>

class JuicyboiAudioProcessor;

namespace jb::gui
{
/** Rev1 amp head: tolex, corner caps, basketweave grille with the only amp badge,
    and a black control panel with unlabelled chrome knobs (tooltips name them).
    The front-panel Model toggle is the `preamp_model` parameter: flipping it fades
    (~200 ms) between the Rectifier faceplate (blonde grille) and the all-black 5150 faceplate.
    Rev2.4: each faceplate is bound to its own amp's parameter set (docs/per-amp-state.md). */
class AmpHead final : public juce::Component, private juce::Timer
{
public:
    static constexpr int designWidth = 1100, designHeight = 440;

    AmpHead (JuicyboiAudioProcessor&, juce::Component* popupParent);
    ~AmpHead() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Skip the faceplate fade (tests / screenshots). */
    void syncModelNow();
    /** Revision plate text next to the knobs: "Rev3" / "Rev3 · 5150". */
    static juce::String revisionPlateText (bool leadFaceplate);
    /** The badge plate on the grille, in AmpHead coordinates (the lettering is jagged metal, see MetalBadge.h). */
    static juce::Rectangle<float> badgeBounds();
    /** 0 = Rectifier faceplate, 1 = 5150 faceplate (fade in between). */
    float getFaceplateMix() const noexcept { return mix; }

private:
    void timerCallback() override;
    void updateFaceplate();
    void layoutKnobs (int model);
    int targetModel() const;
    void paintStatic (juce::Graphics&, int model);
    const juce::Image& getCache (int model, float physicalScale);

    struct Pilot final : juce::Component, juce::TooltipClient
    {
        void paint (juce::Graphics&) override;
        juce::String getTooltip() override { return lit ? "Pilot lamp: amp on" : "Pilot lamp: bypassed (standby)"; }
        bool lit = true;
    };
    JuicyboiAudioProcessor& proc;
    // Rectifier faceplate (amp 0 set). No Input knob: input_trim is the shared
    // calibration in the pedalboard setup strip, not on the head.
    ParamSlider gain, bass, mid, treble, presence, master;
    // 5150 faceplate (amp 1 set, Rev2.4): no Master, no Input - per-channel Pre and Post Gain
    ParamSlider rhythmPre, leadPre, low5, mid5, high5, rhythmPost, leadPost, leadResonance, leadPresence;
    ToggleSwitch standby, model, channel, voice;
    ToggleSwitch channel5, bright, crunch;
    Pilot pilot;

    static constexpr int numRectKnobs = 6, num5150Knobs = 9, numKnobs = numRectKnobs + num5150Knobs;
    /** Small creme caption BELOW each head knob (nothing above the knobs). */
    std::array<juce::Label, numKnobs> knobLabels;
    std::array<ParamSlider*, numKnobs> labelledKnobs() noexcept;
    void syncKnobLabels();

    float mix = 0.0f;
    int shownModel = -1;
    double lastTime = 0.0;
    juce::Image cache[2];
    float cacheScale[2] { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpHead)
};
} // namespace jb::gui
