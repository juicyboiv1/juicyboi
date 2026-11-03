#include "PreampCascade.h"

namespace jb::dsp
{
void PreampCascade::prepare (double sampleRate, const ChannelVoicing& voicing)
{
    fs = sampleRate;
    v = voicing;

    inputTight.setup (OnePole::Type::highPass, v.inputTightHpHz, fs);
    postLP    .setup (OnePole::Type::lowPass,  v.postLpHz, fs);
    dcBlock   .setup (OnePole::Type::highPass, 10.0, fs);

    for (int i = 0; i < ChannelVoicing::maxStages; ++i)
    {
        stageHP[(size_t) i].setup (OnePole::Type::highPass, v.stages[(size_t) i].couplingHpHz, fs);
        stageLP[(size_t) i].setup (OnePole::Type::lowPass,  v.stages[(size_t) i].plateLpHz,    fs);
    }

    // ~20 ms smoothing for the Gain pot
    potSmoothCoef = (float) (1.0 - std::exp (-1.0 / (0.02 * fs)));
    potGain = potTarget;
    reset();
}

void PreampCascade::reset()
{
    inputTight.reset(); postLP.reset(); dcBlock.reset();
    for (auto& f : stageHP) f.reset();
    for (auto& f : stageLP) f.reset();
}

void PreampCascade::setGain (float gain01) noexcept
{
    gain01 = std::clamp (gain01, 0.0f, 1.0f);
    // Exponential (audio-taper-like) pot law between min and max attenuation
    potTarget = v.gainPotMin * std::pow (v.gainPotMax / v.gainPotMin, gain01);
}
} // namespace jb::dsp
