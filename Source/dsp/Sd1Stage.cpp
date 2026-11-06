#include "Sd1Stage.h"

namespace jb::dsp
{
void Sd1Stage::prepare (double sampleRate)
{
    fs = sampleRate;
    inputHP  .setup (OnePole::Type::highPass, inputHpHz, fs);
    gainLegHP.setup (OnePole::Type::highPass, 1.0 / (2.0 * kPi * rLeg * cLeg), fs);
    postLP   .setup (OnePole::Type::lowPass,  1.0 / (2.0 * kPi * rPost * cPost), fs);
    toneLP   .setup (OnePole::Type::lowPass,  1.0 / (2.0 * kPi * rToneSeries * cTone), fs);
    outputHP .setup (OnePole::Type::highPass, outputHpHz, fs);
    lastDrive = -1.0f;
    setParameters (0.5f, 0.5f, 1.0f);
    reset();
}

void Sd1Stage::reset()
{
    inputHP.reset(); gainLegHP.reset(); postLP.reset(); toneLP.reset(); outputHP.reset();
}

void Sd1Stage::setParameters (float drive01, float tone01, float levelGain)
{
    drive01 = std::clamp (drive01, 0.0f, 1.0f);
    tone01  = std::clamp (tone01,  0.0f, 1.0f);

    if (differs (drive01, lastDrive))
    {
        lastDrive = drive01;

        // 500k audio-taper Drive pot (Boutique)
        const auto taper = (std::pow (10.0, 2.0 * drive01) - 1.0) / 99.0;
        const auto rFb   = rFixed + rDrivePot * taper;

        // Excess gain above the leg corner: Rfb / Rleg (dry "+1" from input path)
        // thicknessGain errs toward surplus grit into Dual Rec–style high gain.
        // C6 omitted → no drive-dependent feedback HF clamp (open top).
        driveGain = thicknessGain * (float) (rFb / rLeg);
    }

    // Tone: 0 = mostly C5 LP (darker), 1 = treble restored/boosted (open top helps)
    const auto toneTaper = (std::pow (10.0, 1.5 * tone01) - 1.0) / (std::pow (10.0, 1.5) - 1.0);
    toneTreble = 0.08f + 1.45f * (float) toneTaper;
    level = levelGain;
}
} // namespace jb::dsp
