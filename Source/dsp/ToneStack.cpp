#include "ToneStack.h"

namespace jb::dsp
{
//==============================================================================
void PassiveToneStack::prepare (double sampleRate) noexcept
{
    fs = sampleRate;
    alphaPerSample = 1.0 / (smoothingSeconds * fs);
    {
        const auto fw = std::min (tmb::prewarpHz, 0.25 * fs);
        K = 2.0 * kPi * fw / std::tan (kPi * fw / fs); // same constant as tmb::bilinear
    }
    counter = 0;
    moving = false;
    // Snap to whatever targets are already known (set before prepare).
    if (! haveComponents) setComponents (TmbComponents {});
    reset();
    ss = {};
    snap();
}

void PassiveToneStack::reset() noexcept
{
    for (auto& ch : hist) ch.fill (0.0);
}

void PassiveToneStack::setComponents (const TmbComponents& c) noexcept
{
    const std::array<double, 8> v { c.rt, c.rb, c.rm, c.rs, c.ct, c.cb, c.cm,
                                    std::pow (10.0, c.makeupDb / 20.0) };
    bool changed = false;
    for (size_t i = 0; i < v.size(); ++i)
    {
        changed = changed || std::abs (tgt[3 + i] - v[i]) > 1.0e-9 * std::abs (v[i]);
        tgt[3 + i] = v[i];
    }
    if (! haveComponents)
    {
        haveComponents = true;
        for (size_t i = 3; i < (size_t) numState; ++i) cur[i] = tgt[i];
        if (haveKnobs) snap();
        return;
    }
    if (changed) moving = true;
}

void PassiveToneStack::setKnobs (float bass, float mid, float treble) noexcept
{
    // Smoothed in the knob domain (0..10); tapers are applied per recompute, so a
    // sweep follows the pot law instead of racing through the low-resistance end.
    const std::array<double, 3> v { std::clamp ((double) treble, 0.0, 10.0), std::clamp ((double) mid, 0.0, 10.0),
                                    std::clamp ((double) bass, 0.0, 10.0) };
    bool changed = false;
    for (size_t i = 0; i < v.size(); ++i)
    {
        changed = changed || std::abs (tgt[i] - v[i]) > 1.0e-7;
        tgt[i] = v[i];
    }
    if (! haveKnobs)
    {
        haveKnobs = true;
        for (size_t i = 0; i < 3; ++i) cur[i] = tgt[i];
        if (haveComponents) snap();
        return;
    }
    if (changed) moving = true;
}

void PassiveToneStack::step (int frames) noexcept
{
    const auto alpha = 1.0 - std::exp (-(double) frames * alphaPerSample);
    bool done = true;
    for (size_t i = 0; i < (size_t) numState; ++i)
    {
        cur[i] += alpha * (tgt[i] - cur[i]);
        // Converged when within 1e-5 relative (inaudible: << 0.001 dB)
        const auto scale = i < 3 ? 10.0 : std::abs (tgt[i]);
        if (std::abs (tgt[i] - cur[i]) > 1.0e-5 * scale) done = false;
    }
    if (done) { cur = tgt; moving = false; }
    ssFrom = ss;
    ssTo = solve();
    ramp = 0;
}

void PassiveToneStack::applyState (const StateSpace& n) noexcept
{
    // Keep capacitor voltages continuous when a capacitor value changes (Red <-> Orange):
    // the history source scales with Gc.
    for (size_t k = 0; k < 3; ++k)
        if (ss.gc[k] > 0.0 && n.gc[k] != ss.gc[k])
            for (auto& ch : hist) ch[k] *= n.gc[k] / ss.gc[k];
    ss = n;
    gain = n.makeup;
}

TmbComponents PassiveToneStack::currentComponents() const noexcept
{
    TmbComponents c;
    c.rt = cur[3]; c.rb = cur[4]; c.rm = cur[5]; c.rs = cur[6];
    c.ct = cur[7]; c.cb = cur[8]; c.cm = cur[9];
    return c;
}

tmb::Digital PassiveToneStack::transferFunction() const noexcept
{
    return tmb::bilinear (tmb::analog (currentComponents(), tmb::treblePosition (cur[0]), tmb::midPosition (cur[1]),
                                       tmb::bassPosition (cur[2])), fs);
}

PassiveToneStack::StateSpace PassiveToneStack::solve() const noexcept
{
    // Nodal model, source node u (ideal), unknown nodes:
    enum { A, B, X, M, W, O, N };
    const auto c = currentComponents();
    const auto t = tmb::treblePosition (cur[0]), m = tmb::midPosition (cur[1]), l = tmb::bassPosition (cur[2]);
    const double gc[3] { c.ct * K, c.cb * K, c.cm * K };

    double Y[N][N] {};
    auto conductance = [&Y] (int a, int b, double g)
    {
        Y[a][a] += g; Y[b][b] += g; Y[a][b] -= g; Y[b][a] -= g;
    };
    conductance (A, O, 1.0 / ((1.0 - t) * c.rt));
    conductance (O, B, 1.0 / (t * c.rt));
    conductance (B, M, 1.0 / (l * c.rb));
    conductance (M, W, 1.0 / ((1.0 - m) * c.rm));
    Y[W][W] += 1.0 / (m * c.rm);              // mid wiper leg to ground
    Y[X][X] += 1.0 / c.rs;                    // slope resistor to the source
    Y[A][A] += gc[0];                         // Ct: source -> A
    conductance (X, B, gc[1]);                // Cb: X -> B
    conductance (X, W, gc[2]);                // Cm: X -> W

    // Right-hand side columns: [u, J_ct, J_cb, J_cm]
    // Ct (source->A): A gets +Gc*u - J0. Rs: X gets u/Rs. Cb (X->B): X +J1, B -J1. Cm (X->W): X +J2, W -J2.
    double R[N][4] {};
    R[A][0] = gc[0];  R[X][0] = 1.0 / c.rs;
    R[A][1] = -1.0;
    R[X][2] = 1.0;    R[B][2] = -1.0;
    R[X][3] = 1.0;    R[W][3] = -1.0;

    // Gaussian elimination with partial pivoting: V = Y^-1 R (N x 4)
    for (int k = 0; k < N; ++k)
    {
        int p = k;
        for (int r = k + 1; r < N; ++r) if (std::abs (Y[r][k]) > std::abs (Y[p][k])) p = r;
        if (p != k)
        {
            for (int q = 0; q < N; ++q) std::swap (Y[k][q], Y[p][q]);
            for (int q = 0; q < 4; ++q) std::swap (R[k][q], R[p][q]);
        }
        for (int r = k + 1; r < N; ++r)
        {
            const auto f = Y[r][k] / Y[k][k];
            if (f == 0.0) continue;
            for (int q = k; q < N; ++q) Y[r][q] -= f * Y[k][q];
            for (int q = 0; q < 4; ++q) R[r][q] -= f * R[k][q];
        }
    }
    double V[N][4] {};
    for (int k = N - 1; k >= 0; --k)
        for (int q = 0; q < 4; ++q)
        {
            auto acc = R[k][q];
            for (int j = k + 1; j < N; ++j) acc -= Y[k][j] * V[j][q];
            V[k][q] = acc / Y[k][k];
        }

    StateSpace n;
    n.yu = V[O][0];
    for (int q = 0; q < 3; ++q) n.yj[q] = V[O][q + 1];
    // capacitor voltages: Ct = u - vA, Cb = vX - vB, Cm = vX - vW
    n.vu[0] = 1.0 - V[A][0]; n.vu[1] = V[X][0] - V[B][0]; n.vu[2] = V[X][0] - V[W][0];
    for (int q = 0; q < 3; ++q)
    {
        n.vj[0][q] = -V[A][q + 1];
        n.vj[1][q] = V[X][q + 1] - V[B][q + 1];
        n.vj[2][q] = V[X][q + 1] - V[W][q + 1];
    }
    for (int k = 0; k < 3; ++k) n.gc[k] = gc[k];
    n.makeup = cur[10];
    return n;
}

//==============================================================================
void ToneStack::prepare (double sampleRate)
{
    fs = sampleRate;
    tmbStack.prepare (sampleRate);
    updatePresence();
    reset();
}

void ToneStack::reset()
{
    tmbStack.reset();
    presenceF.reset();
}

void ToneStack::setVoicing (const ChannelVoicing& v)
{
    tmbStack.setComponents (v.tmb);
    if (differs (v.presenceHz, presenceHz))
    {
        presenceHz = v.presenceHz;
        updatePresence();
    }
}

void ToneStack::setKnobs (float bass, float mid, float treble, float presence)
{
    tmbStack.setKnobs (bass, mid, treble);
    if (differs (presence, kPresence))
    {
        kPresence = presence;
        updatePresence();
    }
}

void ToneStack::updatePresence()
{
    const auto presenceDb = -6.0 + kPresence * 1.5; // -6..+9 dB (unchanged from v2/Rev1)
    presenceF.setCoeffs (BiquadCoeffs::highShelf (fs, presenceHz, 0.6, presenceDb));
}
} // namespace jb::dsp
