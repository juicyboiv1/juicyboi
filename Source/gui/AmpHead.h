#pragma once

#include "Rev1Style.h"
#include <array>

class JuicyboiAudioProcessor;

namespace jb::gui
{
/** Rev1 amp head: tolex, corner caps, basketweave grille with the only juicyboi badge,
    and a black control panel with unlabelled chrome knobs (tooltips name them).
    The front-panel Model toggle is the `preamp_model` parameter: flipping it fades
    (~200 ms) between the Rectifier faceplate (blonde grille) and the all-black 5150 faceplate. */
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
    /** Revision plate text next to the knobs: "Rev2" / "Rev2 · 5150". */
    static juce::String revisionPlateText (bool leadFaceplate);
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
    struct LeadMark final : juce::Component, juce::TooltipClient
    {
        void paint (juce::Graphics&) override;
        juce::String getTooltip() override
        {
            return "5150 faceplate: the 5150-inspired model is the Lead channel (Rhythm is not modelled). "
                   "Flip the Model toggle for the Rectifier's Red/Orange channels.";
        }
    };

    JuicyboiAudioProcessor& proc;
    ParamSlider input, gain, bass, mid, treble, presence, leadPresence, leadResonance, master;
    ToggleSwitch standby, model, channel, voice;
    Pilot pilot;
    LeadMark leadMark;

    /** Small creme caption BELOW each head knob (nothing above the knobs). */
    std::array<juce::Label, 9> knobLabels;
    std::array<ParamSlider*, 9> labelledKnobs() noexcept;
    void syncKnobLabels();

    float mix = 0.0f;
    int shownModel = -1;
    double lastTime = 0.0;
    juce::Image cache[2];
    float cacheScale[2] { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpHead)
};
} // namespace jb::gui
