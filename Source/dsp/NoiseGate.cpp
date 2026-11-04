#include "NoiseGate.h"
#include <cmath>

namespace jb::dsp
{
static float onePoleCoeff (double sampleRate, double ms)
{
    return (float) std::exp (-1.0 / (juce::jmax (1.0e-3, ms) * 0.001 * sampleRate));
}

void NoiseGate::prepare (double sampleRate)
{
    sr = sampleRate;
    dcR    = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 20.0 / sr); // 20 Hz DC block on key
    envAtt = onePoleCoeff (sr, 0.1);
    envRel = onePoleCoeff (sr, detReleaseMs);
    gainAtt = onePoleCoeff (sr, attackMs);
    enableMix.reset (sr, 0.01);
    reset();
}

void NoiseGate::reset()
{
    state = State::Closed;
    env = 0.0f;
    gain = 0.0f;
    dcX1 = dcY1 = 0.0f;
    holdCounter = 0;
    enableMix.setCurrentAndTargetValue (enableMix.getTargetValue());
}

void NoiseGate::setParameters (bool enabled, float thresholdDb, float holdMs, float releaseMs)
{
    enableMix.setTargetValue (enabled ? 1.0f : 0.0f);

    openThr  = juce::Decibels::decibelsToGain (thresholdDb, -200.0f);
    closeThr = juce::Decibels::decibelsToGain (thresholdDb - hysteresisDb, -200.0f);
    holdSamples = (int) std::lround (juce::jmax (0.0f, holdMs) * 0.001 * sr);

    // fall 60 dB over the release time
    const auto relSamples = juce::jmax (1.0, (double) releaseMs * 0.001 * sr);
    gainRel = (float) std::pow (0.001, 1.0 / relSamples);
}

void NoiseGate::process (const float* key, float* const* chans, int numChannels, int len)
{
    // True bypass when off and the fade-out has finished.
    if (! enableMix.isSmoothing() && enableMix.getTargetValue() < 0.5f)
    {
        if (active.load (std::memory_order_relaxed))
        {
            active.store (false, std::memory_order_relaxed);
            state = State::Closed; env = 0.0f; gain = 0.0f; holdCounter = 0; dcX1 = dcY1 = 0.0f;
        }
        openAmount.store (1.0f, std::memory_order_relaxed);
        return;
    }

    active.store (true, std::memory_order_relaxed);

    for (int i = 0; i < len; ++i)
    {
        // Key detector: DC block -> rectify -> peak follower
        const auto x = key[i];
        const auto hp = x - dcX1 + dcR * dcY1;
        dcX1 = x; dcY1 = hp;
        const auto rect = std::abs (hp);
        env = rect > env ? rect + envAtt * (env - rect)
                         : rect + envRel * (env - rect);

        // State machine with hysteresis
        if (env >= openThr)
        {
            state = State::Open;
            holdCounter = holdSamples;
        }
        else if (state == State::Open && env < closeThr)
        {
            state = holdSamples > 0 ? State::Hold : State::Closing;
        }
        else if (state == State::Hold)
        {
            if (env >= closeThr)            holdCounter = holdSamples; // still ringing above close thr
            else if (--holdCounter <= 0)    state = State::Closing;
        }

        switch (state)
        {
            case State::Open:
            case State::Hold:
                gain = 1.0f + gainAtt * (gain - 1.0f);
                break;
            case State::Closing:
                gain *= gainRel;
                if (gain < closedFloorGain) { gain = 0.0f; state = State::Closed; }
                break;
            case State::Closed:
                gain = 0.0f;
                break;
        }

        const auto mix = enableMix.getNextValue();
        const auto g = 1.0f + mix * (gain - 1.0f);
        for (int ch = 0; ch < numChannels; ++ch)
            chans[ch][i] *= g;
    }

    openAmount.store (gain, std::memory_order_relaxed);
}
} // namespace jb::dsp
