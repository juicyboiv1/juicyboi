#pragma once
#include "Filters.h"

namespace jb::dsp
{
/** Relative small-signal closed-loop 5150 Presence response, not a power amp.
    R56=39k, R57=4.7k, VR9=10k, parallel C8=0.1u/C62=0.033u.
    Nominal feedback-node load: R57=4.7k via 22u coupling (AC-short assumption);
    resonance bypassed. R85/R81 load the signal-input grid, NOT this node.
    Assumed effective forward gain A=20 V/V, real/constant; no speaker/OT model.
    See docs/5150-presence.md for nodal equations, source and limitations.

    T(s,R)=A/(1+A*beta(s,R)); beta=Z/(Rf+Z),
    Z=RL || (R+1/(s*C)). Process T(s,R)/T(s,10k): position 0 is EXACT
    unity, preserving existing Lead sessions rather than recoloring the baseline.
    Two stable bilinear/TPT one-poles realize this rational transfer; no audio
    feedback loop or algebraic loop is evaluated. Parameter slew is 30 ms. */
class LeadPresence
{
public:
    static constexpr double feedbackR = 39000.0;
    static constexpr double tailR = 4700.0;
    static constexpr double capacitance = (0.1 + 0.033) * 1.0e-6;
    static constexpr double potR = 10000.0;
    static constexpr double forwardGain = 20.0; // nominal, NOT measured

    void setPosition (float value) noexcept
    {
        const auto v = std::clamp ((double) value, 0.0, 10.0);
        const auto r = potR * (std::pow (10.0, 2.0 * (1.0 - v / 10.0)) - 1.0) / 99.0;
        if (std::abs (r - targetR) < 1.0e-9) return;
        targetR = r;
        remaining = slewFrames;
        step = (targetR - currentR) / (double) slewFrames;
    }

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        slewFrames = std::max (1, (int) std::round (0.030 * fs));
        currentR = targetR; remaining = 0;
        state1.fill (0.0); state2.fill (0.0);
        update();
    }

    void beginFrame() noexcept
    {
        if (remaining > 0)
        {
            --remaining;
            currentR = remaining == 0 ? targetR : currentR + step;
            update();
        }
    }

    float process (float sample, int ch) noexcept
    {
        const auto x = (double) sample;
        // Trapezoidal-integrator states: k>0 ensures each pole is stable.
        auto& s1 = state1[(size_t) ch]; auto& s2 = state2[(size_t) ch];
        const auto low1 = s1 + alpha1 * (x - s1);
        s1 = 2.0 * low1 - s1;
        const auto low2 = s2 + alpha2 * (low1 - s2);
        s2 = 2.0 * low2 - s2;
        // Residual form: all three coefficients vanish exactly at reference R.
        return (float) (x + delta * x + weight1 * low1 + weight2 * low2);
    }

private:
    void update() noexcept
    {
        const auto load = tailR; // finite active-PI loading not resolved; documented nominal reduction
        const auto d0 = feedbackR + load;
        const auto n0 = d0 + forwardGain * load;
        const auto zero = capacitance * (feedbackR * load + currentR * d0) / d0;
        const auto pole = capacitance * (feedbackR * load + currentR * n0) / n0;
        const auto refZero = capacitance * (feedbackR * load + potR * d0) / d0;
        const auto refPole = capacitance * (feedbackR * load + potR * n0) / n0;
        const auto k1 = 1.0 / (2.0 * fs * pole), k2 = 1.0 / (2.0 * fs * refZero);
        alpha1 = k1 / (1.0 + k1); alpha2 = k2 / (1.0 + k2);
        if (std::abs (currentR - potR) < 1.0e-9)
        { delta = weight1 = weight2 = 0.0; return; }
        const auto high = zero * refPole / (pole * refZero);
        delta = high - 1.0;
        weight1 = (zero + refPole - high * (pole + refZero)) / refZero;
        weight2 = 1.0 - high - weight1;
    }

    double fs = 48000.0;
    double targetR = potR, currentR = potR, step = 0.0;
    double alpha1 = 0.01, alpha2 = 0.01;
    double delta = 0.0, weight1 = 0.0, weight2 = 0.0;
    std::array<double, kMaxChannels> state1 {}, state2 {};
    int slewFrames = 1440, remaining = 0;
};
} // namespace jb::dsp
