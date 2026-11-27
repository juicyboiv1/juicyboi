#include "GraphicEq.h"

namespace jb::dsp
{
namespace
{
    // ---- Component values (redrawn GE-7 schematic; docs/graphic-eq.md) ----
    constexpr double R30 = 10e3, C22 = 47e-9, R29 = 470e3;            // input
    constexpr double R31 = 4.7e3, R28 = 470.0, C20 = 15e-9;           // U5a pre-emphasis
    constexpr double R27 = 10e3, R26 = 10e3, RPOT = 10e3;             // U4b Level stage
    constexpr double RL = 2.2e3, CL = 10e-6;                          // Level leg R2 / C16
    constexpr double R24 = 3.3e3, R25 = 3.3e3;                        // U4a band stage
    struct GyrParts { double cs, c2, r1, r2; };
    constexpr GyrParts gyrParts[6] {
        { 1.5e-6, 56e-9,  100e3, 330.0 },   // 100 Hz : C10, C13, R16, R17
        { 680e-9, 33e-9,  82e3,  330.0 },   // 200 Hz : C9,  C8,  R9,  R8
        { 330e-9, 15e-9,  100e3, 330.0 },   // 400 Hz : C7,  C14, R18, R19
        { 150e-9, 8.2e-9, 100e3, 330.0 },   // 800 Hz : C5,  C6,  R7,  R6
        { 100e-9, 3.9e-9, 82e3,  330.0 },   // 1.6 kHz: C4,  C15, R21, R22
        { 39e-9,  2.2e-9, 82e3,  330.0 },   // 3.2 kHz: C2,  C3,  R5,  R4
    };
    constexpr double C1 = 47e-9, R3 = 820.0;                          // 6.4 kHz leg
    constexpr double C19 = 1e-6, R32 = 1e6;                           // band stage coupling
    constexpr double R33 = 4.7e3, R36 = 470.0, C27 = 15e-9;           // de-emphasis
    constexpr double C26 = 47e-9, R34 = 1e6;                          // into the output follower
    constexpr double R14 = 1e3, C11 = 1e-6, R12 = 100e3;              // output network
    // Op-amps as single-pole integrators A(s) = wt / s
    constexpr double gbw4558 = 3.0e6, gbwTL022 = 0.5e6;
    constexpr double minPotR = 1.0e-3;                                // wiper at an end
    // Q1 emitter follower (2SC1815-class, beta 300), small-signal at its bias
    // point (ngspice .op: Ic 0.296 mA, gm 11.45 mS, gpi 36.2 uS, go 2.81 uS):
    // Re_eff = R15 10k || (R14 + R12) || ro.
    constexpr double qGm = 11.4527e-3, qGpi = 36.2186e-6, qGo = 2.81037e-6, R15 = 10e3;
    constexpr double qRe = 1.0 / (1.0 / R15 + 1.0 / (R14 + R12) + qGo);
    constexpr double followerGain = (qGm + qGpi) * qRe / (1.0 + (qGm + qGpi) * qRe);   // 0.990
    constexpr double followerRin  = (1.0 + (qGm + qGpi) * qRe) / qGpi;                   // 2.84 M
    constexpr double R34eff = 1.0 / (1.0 / R34 + 1.0 / followerRin);                    // 0.74 M

    using C = std::complex<double>;

    C legZ (int k, C s) noexcept
    {
        if (k < 6)
        {
            // gyrator: Cg from X to u, R1 u -> Vb, TL022 follower(u) -> 330R -> X
            const auto& p = gyrParts[k];
            const C hp = s * p.c2 * p.r1 / (1.0 + s * p.c2 * p.r1);
            const C fol = 1.0 / (1.0 + s / (2.0 * kPi * gbwTL022));
            const C yg = hp / p.r1 + (1.0 - fol * hp) / p.r2;
            return 1.0 / (s * p.cs) + 1.0 / yg;
        }
        return R3 + 1.0 / (s * C1);
    }

    /** Graphic stage: Rin to the + input, Rf feedback, pots between the inputs
        (wiper a from the + end) with legs Z, op-amp A(s) = wt / s. */
    template <size_t N>
    C graphicStage (C s, double rin, double rf, const std::array<double, N>& a, const std::array<C, N>& z) noexcept
    {
        C a11 (1.0 / rin), a22 (1.0 / rf), c (0.0);
        for (size_t k = 0; k < N; ++k)
        {
            const auto ga = 1.0 / std::max (a[k] * RPOT, minPotR);
            const auto gb = 1.0 / std::max ((1.0 - a[k]) * RPOT, minPotR);
            const C gr = 1.0 / z[k];
            const C g = ga + gb + gr;
            a11 += ga * (gb + gr) / g;
            a22 += gb * (ga + gr) / g;
            c += ga * gb / g;
        }
        const C A = 2.0 * kPi * gbw4558 / s;
        const C r = (c + A / rf) / (a22 + A / rf);
        const C p = (1.0 / rin) / (a11 - c * r);
        return A * (p - p * r);
    }

    C levelStage (C s, double wl) noexcept
    {
        return graphicStage<1> (s, R27, R26, { wl }, { C (RL) + 1.0 / (s * CL) });
    }

    C bandStage (C s, const std::array<double, GraphicEq::numBands>& w) noexcept
    {
        std::array<C, GraphicEq::numBands> z {};
        for (int k = 0; k < GraphicEq::numBands; ++k)
            z[(size_t) k] = legZ (k, s);
        return graphicStage<GraphicEq::numBands> (s, R24, R25, w, z);
    }

    C inputAndEmphasis (C s) noexcept
    {
        const C hin = R29 / (R29 + R30 + 1.0 / (s * C22));
        const C pre = 1.0 + R31 / (R28 + 1.0 / (s * C20));
        return hin * pre;
    }

    C outputNetwork (C s) noexcept
    {
        const C zsh = R36 + 1.0 / (s * C27);
        const C zc  = 1.0 / (s * C26) + R34eff;
        const C zb  = 1.0 / (1.0 / zsh + 1.0 / zc);
        const C za  = 1.0 / (1.0 / R32 + 1.0 / (R33 + zb));
        const C va  = za / (1.0 / (s * C19) + za);
        const C vb  = va * zb / (R33 + zb);
        const C vc  = vb * R34eff / zc;
        return followerGain * vc * R12 / (R14 + 1.0 / (s * C11) + R12);
    }

    //==========================================================================
    /** Slider law tables (computed once): wiper position for a dB slider value. */
    struct SliderLaw
    {
        static constexpr int steps = 600;            // -15..+15 in 0.05 dB steps
        std::array<std::array<double, steps + 1>, GraphicEq::numBands + 1> wiper {};
        std::array<double, GraphicEq::numBands + 1> refHz {}, maxDb {};

        static double gainDb (int k, double hz, double a) noexcept
        {
            const C s (0.0, 2.0 * kPi * hz);
            if (k == GraphicEq::numBands)
                return 20.0 * std::log10 (std::abs (levelStage (s, a)));
            std::array<double, GraphicEq::numBands> w {};
            w.fill (0.5);
            w[(size_t) k] = a;
            return 20.0 * std::log10 (std::abs (bandStage (s, w)));
        }

        SliderLaw()
        {
            for (int k = 0; k <= GraphicEq::numBands; ++k)
            {
                // Reference: where full boost peaks (gyrator bands), 12.8 kHz for the
                // 6.4 kHz shelf-like leg, 1 kHz for Level (flat).
                double ref = 1000.0;
                if (k < 6)
                {
                    double best = -1.0;
                    for (int i = 0; i <= 2000; ++i)
                    {
                        const auto hz = 20.0 * std::pow (1000.0, i / 2000.0);
                        const auto g = gainDb (k, hz, 1.0);
                        if (g > best) { best = g; ref = hz; }
                    }
                }
                else if (k == 6)
                    ref = 12800.0;
                refHz[(size_t) k] = ref;
                maxDb[(size_t) k] = gainDb (k, ref, 1.0);

                for (int i = 0; i <= steps; ++i)
                {
                    const auto v = -15.0 + 30.0 * i / steps;
                    const auto target = v / 15.0 * maxDb[(size_t) k];
                    double lo = 0.0, hi = 1.0;
                    for (int it = 0; it < 60; ++it)
                    {
                        const auto mid = 0.5 * (lo + hi);
                        (gainDb (k, ref, mid) < target ? lo : hi) = mid;
                    }
                    wiper[(size_t) k][(size_t) i] = i == 0 ? 0.0 : (i == steps ? 1.0 : 0.5 * (lo + hi));
                }
                wiper[(size_t) k][steps / 2] = 0.5;
            }
        }

        double lookup (int k, double db) const noexcept
        {
            const auto x = (std::clamp (db, -15.0, 15.0) + 15.0) / 30.0 * steps;
            const auto i = std::min ((int) x, steps - 1);
            const auto f = x - i;
            const auto& t = wiper[(size_t) k];
            return t[(size_t) i] + f * (t[(size_t) i + 1] - t[(size_t) i]);
        }
    };

    const SliderLaw& law()
    {
        static const SliderLaw l;
        return l;
    }

    inline void capUpdate (double& j, double g, double v) noexcept { j = 2.0 * g * v - j; }
}

//==============================================================================
double GraphicEq::bandWiper (int band, double db) noexcept { return law().lookup (band, db); }
double GraphicEq::levelWiper (double db) noexcept           { return law().lookup (numBands, db); }
double GraphicEq::bandRefHz (int band) noexcept             { return law().refHz[(size_t) band]; }
double GraphicEq::bandMaxDb (int band) noexcept             { return law().maxDb[(size_t) band]; }
double GraphicEq::levelMaxDb() noexcept                     { return law().maxDb[numBands]; }

std::complex<double> GraphicEq::bandStageResponse (double hz, const std::array<double, numBands>& w) noexcept
{
    return bandStage (C (0.0, 2.0 * kPi * hz), w);
}

std::complex<double> GraphicEq::circuitResponse (double hz, const std::array<double, numBands>& w, double wl) noexcept
{
    const C s (0.0, 2.0 * kPi * hz);
    return inputAndEmphasis (s) * levelStage (s, wl) * bandStage (s, w) * outputNetwork (s);
}

//==============================================================================
void GraphicEq::prepare (double sampleRate, int)
{
    law(); // build the slider tables off the audio thread
    fs = sampleRate;
    const auto g = [this] (double c) { return 2.0 * c * fs; };
    gC22 = g (C22); gC20 = g (C20); gCL = g (CL); gC1 = g (C1);
    gC19 = g (C19); gC27 = g (C27); gC26 = g (C26); gC11 = g (C11);
    kf = 2.0 * kPi * gbwTL022 / (2.0 * fs);
    betaF = kf / (1.0 + kf);
    kOp = 2.0 * kPi * gbw4558 / (2.0 * fs);
    for (size_t k = 0; k < 6; ++k)
    {
        const auto& p = gyrParts[k];
        gCs[k] = g (p.cs);
        gC2[k] = g (p.c2);
        const auto g1 = 1.0 / p.r1;
        gyrInvG2g1[k] = 1.0 / (gC2[k] + g1);
        gyrA[k]  = gC2[k] * gyrInvG2g1[k];                       // u = a X + b
        gyrGg[k] = gyrA[k] * g1 + (1.0 - betaF * gyrA[k]) / p.r2; // i = gg X + hg
        legR[k]  = 1.0 / gCs[k] + 1.0 / gyrGg[k];
    }
    legR[6] = R3 + 1.0 / gC1;
    levelLegR[0] = RL + 1.0 / gCL;

    wCoef = 1.0 - std::exp (-1.0 / (0.010 * fs));       // 10 ms slider smoothing
    mixStep = (float) (1.0 / (0.010 * sampleRate));     // 10 ms enable fade
    wNow = wTarget;
    wlNow = wlTarget;
    wipersMoving = false;
    updateWiperTerms();
    mixCurrent = mixTarget;
    active = mixCurrent > 0.0f;
    reset();
}

void GraphicEq::reset()
{
    resetState();
}

void GraphicEq::resetState() noexcept
{
    for (auto& s : ch)
        s = ChannelState {};
}

void GraphicEq::setParameters (bool enabled, const std::array<float, numBands>& gainsDb, float levelDb) noexcept
{
    for (size_t i = 0; i < (size_t) numBands; ++i)
    {
        const auto w = bandWiper ((int) i, (double) gainsDb[i]);
        if (differs (w, wTarget[i])) { wTarget[i] = w; wipersMoving = true; }
    }
    const auto wl = levelWiper ((double) levelDb);
    if (differs (wl, wlTarget)) { wlTarget = wl; wipersMoving = true; }

    const auto newMix = enabled ? 1.0f : 0.0f;
    if (newMix > 0.5f && ! active)
    {
        // Waking from true bypass: clean circuit state at the current settings.
        wNow = wTarget;
        wlNow = wlTarget;
        wipersMoving = false;
        updateWiperTerms();
        resetState();
        active = true;
    }
    mixTarget = newMix;
}

void GraphicEq::setWipers (bool enabled, const std::array<double, numBands>& w, double wl) noexcept
{
    wTarget = wNow = w;
    wlTarget = wlNow = wl;
    wipersMoving = false;
    updateWiperTerms();
    mixTarget = mixCurrent = enabled ? 1.0f : 0.0f;
    active = enabled;
    resetState();
}

template <int N>
void GraphicEq::computeStage (StageCoeffs<N>& st, double rin, double rf, const std::array<double, N>& w,
                              const std::array<double, N>& legRs) noexcept
{
    st.a11 = 1.0 / rin;
    st.a22 = 1.0 / rf;
    st.c = 0.0;
    for (size_t k = 0; k < (size_t) N; ++k)
    {
        const auto ga = 1.0 / std::max (w[k] * RPOT, minPotR);
        const auto gb = 1.0 / std::max ((1.0 - w[k]) * RPOT, minPotR);
        const auto gr = 1.0 / legRs[k];
        const auto invG = 1.0 / (ga + gb + gr);
        st.ga[k] = ga; st.gb[k] = gb; st.gr[k] = gr; st.invG[k] = invG;
        st.a11 += ga * (gb + gr) * invG;
        st.a22 += gb * (ga + gr) * invG;
        st.c   += ga * gb * invG;
        st.wP[k] = ga * gr * invG;
        st.wN[k] = gb * gr * invG;
    }
}

void GraphicEq::updateWiperTerms() noexcept
{
    computeStage<numBands> (band, R24, R25, wNow, legR);
    computeStage<1> (level, R27, R26, { wlNow }, levelLegR);
}

namespace
{
    /** One sample of a slider stage with leg Thevenin sources e[k]; returns Vout and
        fills the leg currents. ho = op-amp integrator history (updated). */
    template <int N, typename Coeffs>
    double solveStage (const Coeffs& st, double vin, double rin, double rf, double kOp,
                       const std::array<double, N>& e, std::array<double, N>& iLeg, double& ho) noexcept
    {
        double eP = 0.0, eN = 0.0;
        for (size_t k = 0; k < (size_t) N; ++k)
        {
            eP += st.wP[k] * e[k];
            eN += st.wN[k] * e[k];
        }
        const auto kr = kOp / rf;
        const auto r1 = vin / rin + eP;
        const auto r2 = eN + ho / rf;
        const auto det = st.a11 * (st.a22 + kr) - st.c * (st.c + kr);
        const auto p = (r1 * (st.a22 + kr) + st.c * r2) / det;
        const auto n = (st.a11 * r2 + (st.c + kr) * r1) / det;
        const auto d = p - n;
        const auto vout = kOp * d + ho;
        ho = vout + kOp * d;
        for (size_t k = 0; k < (size_t) N; ++k)
        {
            const auto w = (st.ga[k] * p + st.gb[k] * n + st.gr[k] * e[k]) * st.invG[k];
            iLeg[k] = (w - e[k]) * st.gr[k];
        }
        return vout;
    }
}

float GraphicEq::processSample (float xIn, ChannelState& s) noexcept
{
    const double x = xIn;

    // Input: R30 + C22 into R29
    const auto e22 = s.c22.j / gC22;
    const auto iin = (x - e22) / (R30 + 1.0 / gC22 + R29);
    const auto vp = iin * R29;
    capUpdate (s.c22.j, gC22, (iin + s.c22.j) / gC22);

    // U5a pre-emphasis (R28 + C20 leg, R31 feedback; ideal op-amp)
    const auto e20 = s.c20.j / gC20;
    const auto ig = (vp - e20) / (R28 + 1.0 / gC20);
    const auto vpre = vp + R31 * ig;
    capUpdate (s.c20.j, gC20, (ig + s.c20.j) / gC20);

    // U4b Level stage (one leg: R2 + C16)
    double vlev;
    {
        std::array<double, 1> e { s.cL.j / gCL }, i {};
        vlev = solveStage<1> (level, vpre, R27, R26, kOp, e, i, s.hoLevel);
        capUpdate (s.cL.j, gCL, (i[0] + s.cL.j) / gCL);
    }

    // U4a band stage: seven legs as Thevenin sources this sample
    double vband;
    {
        std::array<double, numBands> e {}, i {};
        std::array<double, 6> hg {}, bU {}, gam {};
        for (size_t k = 0; k < 6; ++k)
        {
            const auto& gy = s.gyr[k];
            bU[k] = -gy.c2.j * gyrInvG2g1[k];          // u = a X + b
            gam[k] = gy.hf / (1.0 + kf);               // y = betaF u + gamma
            const auto r2 = gyrParts[k].r2;
            hg[k] = bU[k] / gyrParts[k].r1 - (betaF * bU[k] + gam[k]) / r2;
            e[k] = gy.cs.j / gCs[k] - hg[k] / gyrGg[k];
        }
        e[6] = s.c1.j / gC1;

        vband = solveStage<numBands> (band, vlev, R24, R25, kOp, e, i, s.hoBand);

        for (size_t k = 0; k < 6; ++k)
        {
            auto& gy = s.gyr[k];
            capUpdate (gy.cs.j, gCs[k], (i[k] + gy.cs.j) / gCs[k]);
            const auto xg = (i[k] - hg[k]) / gyrGg[k];
            const auto u = gyrA[k] * xg + bU[k];
            const auto y = betaF * u + gam[k];
            capUpdate (gy.c2.j, gC2[k], xg - u);
            gy.hf = y + kf * (u - y);
        }
        capUpdate (s.c1.j, gC1, (i[6] + s.c1.j) / gC1);
    }

    // C19 / R32 -> de-emphasis R33, (R36 + C27) -> C26 / (R34 || follower input)
    double vc;
    {
        const auto r1t = R36 + 1.0 / gC27, e1 = s.c27.j / gC27;
        const auto r2t = 1.0 / gC26 + R34eff, e2 = s.c26.j / gC26;
        const auto gB = 1.0 / r1t + 1.0 / r2t;
        const auto hB = e1 / r1t + e2 / r2t;
        const auto kA = gC19 + 1.0 / R32 + 1.0 / R33;
        const auto vb = (gC19 * vband - s.c19.j + kA * R33 * hB) / (kA * (1.0 + R33 * gB) - 1.0 / R33);
        const auto va = vb * (1.0 + R33 * gB) - R33 * hB;
        const auto i1 = (vb - e1) / r1t, i2 = (vb - e2) / r2t;
        capUpdate (s.c19.j, gC19, vband - va);
        capUpdate (s.c27.j, gC27, (i1 + s.c27.j) / gC27);
        capUpdate (s.c26.j, gC26, (i2 + s.c26.j) / gC26);
        vc = i2 * R34eff;
    }

    // Emitter follower -> R14 -> C11 -> R12
    const auto e11 = s.c11.j / gC11;
    const auto io = (followerGain * vc - e11) / (R14 + 1.0 / gC11 + R12);
    capUpdate (s.c11.j, gC11, (io + s.c11.j) / gC11);
    return (float) (io * R12);
}

void GraphicEq::process (float* const* chans, int numCh, int len) noexcept
{
    if (! active || len <= 0)
        return; // true bypass

    numCh = std::min (numCh, kMaxChannels);

    for (int n = 0; n < len; ++n)
    {
        if (wipersMoving)
        {
            bool still = true;
            for (size_t k = 0; k < (size_t) numBands; ++k)
            {
                const auto d = wTarget[k] - wNow[k];
                if (std::abs (d) > 1.0e-7) { wNow[k] += wCoef * d; still = false; }
                else wNow[k] = wTarget[k];
            }
            const auto dl = wlTarget - wlNow;
            if (std::abs (dl) > 1.0e-7) { wlNow += wCoef * dl; still = false; }
            else wlNow = wlTarget;
            updateWiperTerms();
            wipersMoving = ! still;
        }

        if (mixCurrent < mixTarget)      mixCurrent = std::min (mixTarget, mixCurrent + mixStep);
        else if (mixCurrent > mixTarget) mixCurrent = std::max (mixTarget, mixCurrent - mixStep);

        // smoothstep-shaped fade: no slope kink at either end of the 10 ms ramp
        const auto m = mixCurrent * mixCurrent * (3.0f - 2.0f * mixCurrent);
        for (int c = 0; c < numCh; ++c)
        {
            const auto x = chans[c][n];
            const auto y = processSample (x, ch[(size_t) c]);
            chans[c][n] = mixCurrent >= 1.0f ? y : x + m * (y - x);
        }
    }

    if (mixTarget <= 0.0f && mixCurrent <= 0.0f)
        active = false; // fully faded out -> true bypass from next block
}

double GraphicEq::getMagnitudeDb (double hz) const noexcept
{
    return 20.0 * std::log10 (std::abs (circuitResponse (hz, wTarget, wlTarget)) + 1e-30);
}
} // namespace jb::dsp
