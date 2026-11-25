#include "Od808Stage.h"

namespace jb::dsp
{
void Od808Stage::prepare (double sampleRate)
{
    fs = sampleRate;
    inputHP.setup (OnePole::Type::highPass, inputHpHz, fs);
    clipper.setComponents (clipperComponents());
    clipper.prepare (fs);
    setParameters (0.5f, 0.5f, 1.0f);
    post.prepare (fs, toneComponents(), outputComponents(), makeup);
    snap();
    reset();
}

void Od808Stage::reset()
{
    inputHP.reset(); clipper.reset(); post.reset();
}

void Od808Stage::setParameters (float drive01, float tone01, float levelGain)
{
    clipper.setDrive (std::clamp (drive01, 0.0f, 1.0f));
    post.setTargets (tone01, levelGain);
}

void Od808Stage::snap()
{
    clipper.snap();
    clipper.beginFrame();
    post.snap();
}
} // namespace jb::dsp
