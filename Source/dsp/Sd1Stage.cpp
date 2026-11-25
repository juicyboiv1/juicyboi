#include "Sd1Stage.h"

namespace jb::dsp
{
void Sd1Stage::prepare (double sampleRate)
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

void Sd1Stage::reset()
{
    inputHP.reset(); clipper.reset(); post.reset();
}

void Sd1Stage::setParameters (float drive01, float tone01, float levelGain)
{
    clipper.setDrive (std::clamp (drive01, 0.0f, 1.0f));
    post.setTargets (tone01, levelGain);
}

void Sd1Stage::snap()
{
    clipper.snap();
    clipper.beginFrame();
    post.snap();
}
} // namespace jb::dsp
