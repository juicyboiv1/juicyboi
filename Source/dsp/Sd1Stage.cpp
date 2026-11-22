#include "Sd1Stage.h"

namespace jb::dsp
{
void Sd1Stage::prepare (double sampleRate)
{
    fs = sampleRate;
    inputHP .setup (OnePole::Type::highPass, inputHpHz, fs);
    postLP  .setup (OnePole::Type::lowPass,  1.0 / (2.0 * kPi * rPost * cPost), fs);
    toneLP  .setup (OnePole::Type::lowPass,  1.0 / (2.0 * kPi * rToneSeries * cTone), fs);
    outputHP.setup (OnePole::Type::highPass, outputHpHz, fs);
    clipper.setComponents (clipperComponents());
    clipper.prepare (fs);
    smooth = (float) (1.0 - std::exp (-1.0 / (0.020 * fs)));
    setParameters (0.5f, 0.5f, 1.0f);
    snap();
    reset();
}

void Sd1Stage::reset()
{
    inputHP.reset(); postLP.reset(); toneLP.reset(); outputHP.reset(); clipper.reset();
}

void Sd1Stage::setParameters (float drive01, float tone01, float levelGain)
{
    drive01 = std::clamp (drive01, 0.0f, 1.0f);
    tone01  = std::clamp (tone01,  0.0f, 1.0f);
    clipper.setDrive (drive01);

    const auto toneTaper = (std::pow (10.0, 1.5 * tone01) - 1.0) / (std::pow (10.0, 1.5) - 1.0);
    toneTarget  = 0.08f + 1.45f * (float) toneTaper;
    levelTarget = levelGain;
}

void Sd1Stage::snap()
{
    clipper.snap();
    clipper.beginFrame();
    toneTreble = toneTarget;
    level = levelTarget;
}
} // namespace jb::dsp
