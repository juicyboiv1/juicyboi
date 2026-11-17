#pragma once
#include "Filters.h"

namespace jb::dsp
{
/** Relative small-signal closed-loop 5150 Resonance response, not a power amp.
    Same declared feedback-divider reduction as LeadPresence (R56=39k, R57=4.7k,
    VR9 10k + C8||C62 = .133u, nominal forward gain A=20, resistive load), with
    the Resonance network added in SERIES with R56:
        VR8 = 1M audio rheostat (POT0752, "1M5%A") in PARALLEL with C12 = .0068u.
    At low frequencies C12 is open, the feedback resistance rises from 39k toward
    39k+VR8, less NFB reaches the PI tail, and the closed-loop gain rises. Above
    1/(2*pi*VR8*C12) C12 shorts VR8 and the loop returns to the Presence reference.
    VR8 = 0 (fully CCW) shorts the network: that is the EXACT current Lead sound.
    See docs/5150-resonance.md for the nodal equations, source and limitations.

        Rf(s)   = R56 + Rr/(1 + s*Rr*C12)
        Z(s)    = RL || (Rp + 1/(s*Cp))
        T(s)    = A / (1 + A*Z/(Rf+Z))
        H(s)    = T(s; Rp, Rr) / T(s; Rp, 0)

    H is an exact factor, so Presence (Rp) and Resonance (Rr) interact as in one
    loop: LeadPresence * LeadResonance == T(Rp,Rr)/T(10k,0). H is a 3rd/3rd-order
    real-pole/real-zero rational function (RC-impedance sums), realized as three
    trapezoidal (TPT, unwarped bilinear) first-order sections in double, the same
    discretization as LeadPresence. No audio feedback loop is evaluated.
    No speaker impedance peak, OT low-frequency roll-off, PI/tube nonlinearity or
    damping-factor effect is modeled; with a resistive load the result is a broad
    low-frequency shelf, not a speaker-resonance peak. Parameter slew is 30 ms. */
class LeadResonance
{
public:
    static constexpr double feedbackR = 39000.0;          // R56
    static constexpr double tailR = 4700.0;               // R57 (nominal node load)
    static constexpr double presenceC = (0.1 + 0.033) * 1.0e-6; // C8 || C62
    static constexpr double resonanceC = 0.0068e-6;       // C12
    static constexpr double potR = 1.0e6;                 // VR8 1M audio
    static constexpr double forwardGain = 20.0;           // nominal, NOT measured

    /** Knob 0..10 -> VR8 series resistance. Two-decade audio law, 0 = 0 ohm. */
    static double resistanceFor (double knob) noexcept
    {
        const auto p = std::clamp (knob, 0.0, 10.0) / 10.0;
        return potR * (std::pow (10.0, 2.0 * p) - 1.0) / 99.0;
    }

    void setPosition (float value) noexcept
    {
        const auto r = resistanceFor ((double) value);
        if (std::abs (r - targetR) < 1.0e-9) return;
        targetR = r;
        remaining = slewFrames;
        step = (targetR - currentR) / (double) slewFrames;
    }

    void prepare (double sampleRate, double presenceResistance = 10000.0) noexcept
    {
        fs = sampleRate;
        slewFrames = std::max (1, (int) std::round (0.030 * fs));
        currentR = targetR; remaining = 0;
        presenceR = presenceResistance;
        for (auto& s : state) s.fill (0.0);
        update();
    }

    /** Once per sample frame. presenceResistance = LeadPresence::resistance(). */
    void beginFrame (double presenceResistance) noexcept
    {
        bool dirty = std::abs (presenceResistance - presenceR) > 1.0e-9;
        presenceR = presenceResistance;
        if (remaining > 0)
        {
            --remaining;
            currentR = remaining == 0 ? targetR : currentR + step;
            dirty = true;
        }
        if (dirty) update();
    }

    float process (float sample, int ch) noexcept
    {
        // Filter states always run so leaving 0 is continuous; at exactly 0 ohm
        // (settled) the output is the bit-identical input (current Lead sound).
        auto y = (double) sample * gain;
        for (size_t i = 0; i < 3; ++i)
        {
            auto& s = state[i][(size_t) ch];
            const auto low = s + alpha[i] * (y - s);
            s = 2.0 * low - s;
            y = direct[i] * y + (1.0 - direct[i]) * low;
        }
        return identity ? sample : (float) y;
    }

    double resistance() const noexcept { return currentR; }

private:
    struct Quad { double c0, c1, c2; };

    // (1 + s*t1)(1 + s*t2) = 1 + s*c1/c0 + s^2*c2/c0, real roots (RC impedance).
    static void taus (const Quad& q, double& t1, double& t2) noexcept
    {
        const auto sum = q.c1 / q.c0, prod = std::max (0.0, q.c2 / q.c0);
        const auto disc = std::max (0.0, sum * sum - 4.0 * prod);
        t1 = 0.5 * (sum + std::sqrt (disc));
        t2 = t1 > 0.0 ? prod / t1 : 0.0;
    }

    // Numerator of (Rf + k*Z) over common denominator (1+s*Rr*C12)(1+s*(RL+Rp)*Cp).
    static Quad loopPoly (double rr, double rp, double k) noexcept
    {
        const auto a = rr * resonanceC, tp1 = rp * presenceC, tp2 = (tailR + rp) * presenceC;
        const auto rl = k * tailR;
        return { feedbackR + rr + rl,
                 (feedbackR + rr) * tp2 + feedbackR * a + rl * (tp1 + a),
                 feedbackR * a * tp2 + rl * tp1 * a };
    }

    void update() noexcept
    {
        identity = remaining == 0 && currentR <= 0.0;
        const auto rr = std::max (0.0, currentR), rp = std::max (0.0, presenceR);
        const auto n = loopPoly (rr, rp, 1.0), d = loopPoly (rr, rp, 1.0 + forwardGain);
        const auto n0 = loopPoly (0.0, rp, 1.0), d0 = loopPoly (0.0, rp, 1.0 + forwardGain);
        double zn1, zn2, pd1, pd2;
        taus (n, zn1, zn2); taus (d, pd1, pd2);
        const auto pn0 = n0.c1 / n0.c0, zd0 = d0.c1 / d0.c0;
        // H = G * (1+s zn1)(1+s zn2)(1+s zd0) / ((1+s pd1)(1+s pd2)(1+s pn0))
        gain = (n.c0 * d0.c0) / (d.c0 * n0.c0);
        std::array<double, 3> zeros { zn1, zn2, zd0 }, poles { pd1, pd2, pn0 };
        std::sort (zeros.begin(), zeros.end()); std::sort (poles.begin(), poles.end());
        for (size_t i = 0; i < 3; ++i)
        {
            const auto tp = std::max (poles[i], 1.0e-12);
            const auto k = 1.0 / (2.0 * fs * tp);
            alpha[i] = k / (1.0 + k);
            direct[i] = zeros[i] / tp;   // (1+s tz)/(1+s tp) = tz/tp + (1-tz/tp)/(1+s tp)
        }
    }

    double fs = 48000.0;
    double targetR = 0.0, currentR = 0.0, step = 0.0, presenceR = 10000.0;
    double gain = 1.0;
    std::array<double, 3> alpha { 0.5, 0.5, 0.5 }, direct { 1.0, 1.0, 1.0 };
    std::array<std::array<double, kMaxChannels>, 3> state {};
    int slewFrames = 1440, remaining = 0;
    bool identity = true;
};
} // namespace jb::dsp
