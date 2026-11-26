#pragma once

#include "Filters.h"
#include <array>
#include <complex>

namespace jb::dsp
{
/** Rev3.2 "juicyboi GEQ-7": component-level model of a classic 7-band guitar
    graphic EQ pedal (GE-7-style topology, values from the published redrawn
    schematic; docs/graphic-eq.md). Placed post-ToneStack / pre-Cab and run
    inside the 4x oversampled region so the bilinear (trapezoidal) mapping is
    within 0.1 dB of the analogue circuit to 20 kHz.

    Circuit (Vcc/2 = AC ground; op-amps as single-pole integrators: U4a/U4b
    uPC4558 3 MHz, gyrator followers TL022 0.5 MHz, U5a ideal; Q1 a small-signal
    2SC1815-class follower at its bias point):
      in -R30 10k- C22 47n -+- U5a (+), R29 470k to Vb
      U5a: non-inverting pre-emphasis, R31 4.7k feedback, R28 470 + C20 15n leg
      U4b "Level" stage: R27 10k in, R26 10k feedback, 10k lin pot between the
           op-amp inputs, wiper -> R2 2.2k + C16 10u to Vb
      U4a band stage: R24 3.3k in, R25 3.3k feedback, seven 10k lin slider pots
           between the inputs, each wiper -> its band leg:
           100 Hz..3.2 kHz: series C + gyrator (C, R to Vb, TL022 follower, 330R)
           6.4 kHz: C1 47n + R3 820
      C19 1u (R32 1M) -> de-emphasis R33 4.7k / (R36 470 + C27 15n) ->
      C26 47n (R34 1M) -> emitter follower -> R14 1k -> C11 1u -> R12 100k out

    Every capacitor is a trapezoidal companion model, so the digital filter is
    exactly the bilinear transform of the circuit's transfer function. Each leg
    reduces to a Thevenin source per sample and the two slider stages have a
    closed-form solve (no matrices), so pot moves are per-sample smoothed with
    no zipper and no coefficient recalculation.

    Slider law: the dB parameters (kept from Rev3.1, IDs unchanged) are a
    dB-calibrated slider scale. +/-15 is the slider end (wiper at the op-amp
    minus / plus input); in between, the wiper is placed so the band's peak
    gain is (value / 15) x that band's maximum. Level likewise (+/-14.9 dB). */
class GraphicEq
{
public:
    static constexpr int numBands = 7;
    static constexpr std::array<double, numBands> bandHz { 100.0, 200.0, 400.0, 800.0, 1600.0, 3200.0, 6400.0 };
    static constexpr float maxDb = 15.0f;

    void prepare (double sampleRate, int maxBlock);
    void reset();

    /** gainsDb: 7 band slider values (dB scale), levelDb: Level slider (dB scale). */
    void setParameters (bool enabled, const std::array<float, numBands>& gainsDb, float levelDb) noexcept;

    /** Tests / tools: set the wiper positions directly (0 = full cut .. 1 = full boost), snapped. */
    void setWipers (bool enabled, const std::array<double, numBands>& wipers, double levelWiperPos) noexcept;

    /** In-place processing of numCh channel pointers, len samples (at the prepared rate). */
    void process (float* const* chans, int numCh, int len) noexcept;

    /** True when the EQ is currently doing any work (on, or fading). */
    bool isActive() const noexcept { return active; }

    /** Analogue magnitude (dB) of the whole pedal at the current target settings. */
    double getMagnitudeDb (double hz) const noexcept;

    //==========================================================================
    // Circuit helpers (also used by tests / tools)
    /** Slider dB value -> wiper position (0 = at the stage's + input = full cut, 1 = full boost). */
    static double bandWiper (int band, double db) noexcept;
    static double levelWiper (double db) noexcept;
    /** Reference frequency used for a band's slider law, and its gain at full boost (dB). */
    static double bandRefHz (int band) noexcept;
    static double bandMaxDb (int band) noexcept;
    static double levelMaxDb() noexcept;
    /** Analogue transfer function of the whole pedal for given wiper positions. */
    static std::complex<double> circuitResponse (double hz, const std::array<double, numBands>& wipers, double levelWiper) noexcept;
    /** Analogue transfer function of the band stage alone (U4a). */
    static std::complex<double> bandStageResponse (double hz, const std::array<double, numBands>& wipers) noexcept;

private:
    struct Cap { double j = 0.0; };            // trapezoidal companion history (i = G v - J)
    struct Gyr { Cap cs, c2; double hf = 0.0; }; // + follower integrator history
    struct ChannelState
    {
        Cap c22, c20, cL, c1, c19, c27, c26, c11;
        double hoLevel = 0.0, hoBand = 0.0;       // op-amp integrator histories
        std::array<Gyr, 6> gyr;
    };
    /** Wiper-dependent solve coefficients of one slider stage. */
    template <int N>
    struct StageCoeffs
    {
        double a11 = 0, a22 = 0, c = 0;
        std::array<double, N> wP {}, wN {}, ga {}, gb {}, gr {}, invG {};
    };
    template <int N>
    static void computeStage (StageCoeffs<N>&, double rin, double rf, const std::array<double, N>& wipers,
                              const std::array<double, N>& legR) noexcept;

    float processSample (float x, ChannelState& s) noexcept;
    void updateWiperTerms() noexcept;
    void resetState() noexcept;

    double fs = 192000.0;
    // Constant per sample rate
    double gC22 = 0, gC20 = 0, gCL = 0, gC1 = 0, gC19 = 0, gC27 = 0, gC26 = 0, gC11 = 0;
    std::array<double, 6> gCs {}, gC2 {}, gyrA {}, gyrGg {}, gyrInvG2g1 {}; // gyrator constants
    double kf = 0.0, betaF = 0.0;            // follower integrator (TL022)
    double kOp = 0.0;                        // band / Level op-amp integrator (4558)
    std::array<double, numBands> legR {};    // Thevenin resistance of each band leg
    std::array<double, 1> levelLegR {};

    // Wipers: target and smoothed
    std::array<double, numBands> wTarget {}, wNow {};
    double wlTarget = 0.5, wlNow = 0.5;
    double wCoef = 0.0;
    bool wipersMoving = true;
    StageCoeffs<numBands> band;
    StageCoeffs<1> level;

    std::array<ChannelState, kMaxChannels> ch {};

    float mixTarget = 0.0f, mixCurrent = 0.0f, mixStep = 0.0f;
    bool active = false;
};
} // namespace jb::dsp
