#include "Od808Stage.h"

namespace jb::dsp
{
void Od808Stage::prepare (double sampleRate)
{
    fs = sampleRate;
    inputHP  .setup (OnePole::Type::highPass, inputHpHz, fs);
    gainLegHP.setup (OnePole::Type::highPass, 1.0 / (2.0 * kPi * rLeg * cLeg), fs);
    toneLP   .setup (OnePole::Type::lowPass,  toneLpHz, fs);
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

        // Log-ish (audio taper) pot approximation
        const auto taper = (std::pow (10.0, 2.0 * drive01) - 1.0) / 99.0;
        const auto rFb   = rFixed + rDrivePot * taper;

        // Op-amp non-inverting gain above the leg corner: 1 + Rfb/Rleg.
        // Stored as (gain - 1) since the "+1" is the dry path in process().
        driveGain = (float) (rFb / rLeg);

        // Feedback cap corner falls as Rfb grows (more drive = darker clip)
        feedbackLP.setup (OnePole::Type::lowPass, 1.0 / (2.0 * kPi * rFb * cFeedback), fs);
    }

    // Tone: 0 -> mostly the ~720 Hz LP (dark), 1 -> treble restored/boosted.
    toneTreble = 0.08f + 1.32f * tone01;
    level = levelGain;
}
} // namespace jb::dsp
