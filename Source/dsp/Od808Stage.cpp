#include "Od808Stage.h"

namespace jb::dsp
{
void Od808Stage::prepare (double sampleRate)
{
    fs = sampleRate;
    inputHP  .setup (OnePole::Type::highPass, inputHpHz, fs);
    gainLegHP.setup (OnePole::Type::highPass, 1.0 / (2.0 * kPi * rLeg * cLeg), fs);
    toneLP   .setup (OnePole::Type::lowPass,  1.0 / (2.0 * kPi * rToneSeries * cTone), fs);
    outputHP .setup (OnePole::Type::highPass, outputHpHz, fs);
    lastDrive = -1.0f;
    setParameters (0.5f, 0.5f, 1.0f);
    reset();
}

void Od808Stage::reset()
{
    inputHP.reset(); gainLegHP.reset(); feedbackLP.reset(); toneLP.reset(); outputHP.reset();
}

void Od808Stage::setParameters (float drive01, float tone01, float levelGain)
{
    drive01 = std::clamp (drive01, 0.0f, 1.0f);
    tone01  = std::clamp (tone01,  0.0f, 1.0f);

    if (differs (drive01, lastDrive))
    {
        lastDrive = drive01;

        // 500k audio-taper Overdrive pot
        const auto taper = (std::pow (10.0, 2.0 * drive01) - 1.0) / 99.0;
        const auto rFb   = rFixed + rDrivePot * taper;

        // Non-inverting excess gain above the leg corner: Rfb / Rleg
        // (dry "+1" still comes from the input path in process())
        driveGain = (float) (rFb / rLeg);

        // C1 across the feedback network — more drive => lower LP corner
        feedbackLP.setup (OnePole::Type::lowPass, 1.0 / (2.0 * kPi * rFb * cFeedback), fs);
    }

    // 20k audio Tone: 0 = mostly 220n LP (dark), 1 = treble restored/boosted
    const auto toneTaper = (std::pow (10.0, 1.5 * tone01) - 1.0) / (std::pow (10.0, 1.5) - 1.0);
    toneTreble = 0.05f + 1.35f * (float) toneTaper;
    level = levelGain;
}
} // namespace jb::dsp
