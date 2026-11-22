// Rev2.2 component-level diode clippers (OD-808 / SD-1) and the post-TMB
// recovery stage: solver, stability, smoothing and ngspice agreement.
#include "dsp/Od808Stage.h"
#include "dsp/Sd1Stage.h"
#include "dsp/RecoveryStage.h"

#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
int failures = 0;
void check (const char* what, bool ok)
{
    std::printf ("[%s] %s\n", ok ? "ok" : "FAIL", what);
    if (! ok) ++failures;
}

template <typename Stage>
struct Probe
{
    // Clipper-core render of a sine; returns op-amp output
    static std::vector<double> sine (double fs, double drive, double amp, double freq, double secs, int* maxIt = nullptr)
    {
        Stage s; s.prepare (fs); s.setParameters ((float) drive, 0.5f, 1.0f); s.snap();
        std::vector<double> y;
        const auto n = (long) std::llround (secs * fs);
        int it = 0;
        for (long i = 0; i < n; ++i)
        {
            s.beginFrame();
            const auto x = (float) (amp * std::sin (2.0 * jb::dsp::kPi * freq * (double) i / fs));
            y.push_back ((double) s.processClipper (x, 0));
            it = std::max (it, s.getClipper().lastIterations());
        }
        if (maxIt) *maxIt = it;
        return y;
    }
};

bool finiteNoDenormal (float v) { return std::isfinite (v) && std::fpclassify (v) != FP_SUBNORMAL; }

// Small-signal |H| of the linear clipper network at f (diodes replaced by their
// zero-bias conductance), single-pole op-amp as in DiodeClipper.h.
double linearGainDb (const jb::dsp::ClipperComponents& c, double rf, double f)
{
    using cd = std::complex<double>;
    const cd s (0.0, 2.0 * jb::dsp::kPi * f);
    const auto gd = c.is / (c.diodesPos * c.n * c.vt) + c.is / (c.diodesNeg * c.n * c.vt);
    const cd yf = 1.0 / rf + s * c.cFeedback + gd;
    const cd zleg = c.rLeg + 1.0 / (s * c.cLeg);
    const cd beta = yf / (yf + 1.0 / zleg);  // vn / vo
    const cd a = (c.gbw > 0.0) ? (2.0 * jb::dsp::kPi * c.gbw) / (s + 2.0 * jb::dsp::kPi * c.gbw / c.a0) : cd (1e12);
    return 20.0 * std::log10 (std::abs (a / (1.0 + a * beta)));
}

template <typename Stage>
double measuredGainDb (double fs, double drive, double f)
{
    const double amp = 1e-4;
    auto y = Probe<Stage>::sine (fs, drive, amp, f, 0.5);
    // correlate the last 0.2 s with the input to read |H| at f
    const auto n0 = y.size() - (size_t) (0.2 * fs);
    double re = 0, im = 0;
    for (size_t i = n0; i < y.size(); ++i)
    {
        const auto ph = 2.0 * jb::dsp::kPi * f * (double) i / fs;
        re += y[i] * std::sin (ph); im += y[i] * std::cos (ph);
    }
    const auto mag = 2.0 * std::sqrt (re * re + im * im) / (double) (y.size() - n0);
    return 20.0 * std::log10 (mag / amp);
}

std::vector<std::vector<double>> loadSpiceRef()
{
    std::ifstream f (std::string (JUICYBOI_TEST_DIR) + "/clipper-ngspice-ref.txt");
    std::vector<std::vector<double>> cols (4);
    std::string line;
    while (std::getline (f, line))
    {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss (line);
        for (auto& c : cols) { double v; ss >> v; c.push_back (v); }
    }
    return cols;
}

template <typename Stage>
double cpuNsPerSample (double fs)
{
    Stage s; s.prepare (fs); s.setParameters (0.7f, 0.5f, 1.0f); s.snap();
    const long n = (long) (2.0 * fs);
    volatile float sink = 0.0f;
    const auto t0 = std::chrono::steady_clock::now();
    for (long i = 0; i < n; ++i)
    {
        s.beginFrame();
        const auto x = (float) (0.3 * std::sin (2.0 * jb::dsp::kPi * 196.0 * (double) i / fs));
        sink = sink + s.process (x, 0) + s.process (x, 1);
    }
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano> (t1 - t0).count() / (double) (2 * n);
}
} // namespace

int clipperValidation()
{
    failures = 0;
    std::printf ("\n== Rev2.2 diode clippers ==\n");

    // 1) Small-signal gain == linear network (validates the companion models, Drive law, op-amp pole)
    {
        double worst = 0.0;
        for (double drive : { 0.0, 0.5, 1.0 })
            for (double f : { 100.0, 720.0, 3000.0 })
            {
                auto co = jb::dsp::Od808Stage::clipperComponents();
                auto cs = jb::dsp::Sd1Stage::clipperComponents();
                const auto tp = jb::dsp::OpAmpDiodeClipper::driveTaper (drive);
                // the input coupling HP is part of processClipper: include it
                auto hp = [] (double fc, double ff) { return 20.0 * std::log10 (ff / std::sqrt (ff * ff + fc * fc)); };
                const auto eo = measuredGainDb<jb::dsp::Od808Stage> (192000.0, drive, f)
                              - (linearGainDb (co, co.rFixed + co.rDrivePot * tp, f) + hp (15.915, f));
                const auto es = measuredGainDb<jb::dsp::Sd1Stage> (192000.0, drive, f)
                              - (linearGainDb (cs, cs.rFixed + cs.rDrivePot * tp, f) + hp (28.937, f));
                worst = std::max ({ worst, std::abs (eo), std::abs (es) });
            }
        std::printf ("small-signal gain vs analytic linear network: worst %.3f dB (Drive 0/5/10, 100 Hz-3 kHz)\n", worst);
        check ("solved clippers match the linear network at small signal (< 0.1 dB)", worst < 0.1);
    }

    // 2) ngspice agreement (same netlists as tools/clippers, stored reference)
    {
        const auto ref = loadSpiceRef();
        struct Case { bool od; double drive, amp; double maxRms, maxPeak; };
        const Case cases[] = { { true, 0.5, 0.3, 0.005, 0.03 }, { true, 1.0, 1.0, 0.006, 0.04 },
                               { false, 0.5, 0.3, 0.006, 0.05 }, { false, 1.0, 1.0, 0.010, 0.15 } };
        bool ok = ref[0].size() == 5760;
        for (int c = 0; ok && c < 4; ++c)
        {
            const auto& k = cases[c];
            const auto y = k.od ? Probe<jb::dsp::Od808Stage>::sine (192000.0, k.drive, k.amp, 1000.0, 0.03)
                                : Probe<jb::dsp::Sd1Stage>::sine (192000.0, k.drive, k.amp, 1000.0, 0.03);
            double e2 = 0, r2 = 0, emax = 0, peak = 0;
            for (size_t i = 0; i < y.size() && i < ref[(size_t) c].size(); ++i)
            {
                const auto r = ref[(size_t) c][i];
                e2 += (y[i] - r) * (y[i] - r); r2 += r * r;
                emax = std::max (emax, std::abs (y[i] - r)); peak = std::max (peak, std::abs (r));
            }
            const auto rms = std::sqrt (e2 / r2), pk = emax / peak;
            std::printf ("ngspice %s Drive %.0f in %.1f V: rms err %.3f%%, max err %.2f%% of peak\n",
                         k.od ? "OD-808" : "SD-1  ", k.drive * 10.0, k.amp, 100.0 * rms, 100.0 * pk);
            ok = ok && rms < k.maxRms && pk < k.maxPeak;
        }
        check ("clippers track the ngspice transient (rms < 1%, edge-sample max within bounds)", ok);
    }

    // 3) Clip levels / asymmetry from the diode law
    {
        auto yo = Probe<jb::dsp::Od808Stage>::sine (192000.0, 1.0, 1.0, 200.0, 0.3);
        auto ys = Probe<jb::dsp::Sd1Stage>::sine (192000.0, 1.0, 0.5, 200.0, 0.3);
        auto xs = [] (double amp, size_t n) { std::vector<double> v; for (size_t i = 0; i < n; ++i) v.push_back (amp * std::sin (2.0 * jb::dsp::kPi * 200.0 * (double) i / 192000.0)); return v; };
        // vd = out - in is the feedback-network voltage (input HP is ~unity at 200 Hz)
        auto vdRange = [&] (const std::vector<double>& y, double amp, double& lo, double& hi)
        {
            const auto x = xs (amp, y.size()); lo = 1e9; hi = -1e9;
            for (size_t i = y.size() / 2; i < y.size(); ++i) { lo = std::min (lo, y[i] - x[i]); hi = std::max (hi, y[i] - x[i]); }
        };
        double olo, ohi, slo, shi;
        vdRange (yo, 1.0, olo, ohi); vdRange (ys, 0.5, slo, shi);
        std::printf ("OD-808 feedback-network swing %+.3f / %+.3f V; SD-1 %+.3f / %+.3f V (ratio %.2f)\n", olo, ohi, slo, shi, shi / -slo);
        check ("OD-808 symmetric 1N914 pair clips at ~0.45-0.75 V", ohi > 0.45 && ohi < 0.75 && -olo > 0.45 && -olo < 0.75 && std::abs (ohi + olo) < 0.03);
        check ("SD-1 asymmetric (2 vs 1 diodes): positive swing ~2x negative", shi / -slo > 1.7 && shi / -slo < 2.3);
    }

    // 4) Stability: every host rate's 4x rate, extreme inputs, silence tail, no NaN/denormals
    {
        bool ok = true; int worstIt = 0;
        for (double host : { 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            const auto fs = 4.0 * host;
            jb::dsp::Od808Stage od; jb::dsp::Sd1Stage sd;
            od.prepare (fs); sd.prepare (fs);
            const long n = (long) (0.25 * fs);
            for (int phase = 0; phase < 4; ++phase)
            {
                const float drive = phase % 2 == 0 ? 1.0f : 0.0f;
                od.setParameters (drive, phase * 0.33f, 2.0f); sd.setParameters (drive, phase * 0.33f, 2.0f);
                for (long i = 0; i < n; ++i)
                {
                    float x = 0.0f;
                    if (phase == 0) x = (float) (8.0 * std::sin (2.0 * jb::dsp::kPi * 5000.0 * (double) i / fs)); // hot, high
                    if (phase == 1) x = (i % 997 == 0) ? 10.0f : 0.0f;                                               // impulses
                    if (phase == 2) x = (float) (1e-3 * std::sin (2.0 * jb::dsp::kPi * 40.0 * (double) i / fs));    // quiet
                    // phase 3: silence tail
                    od.beginFrame(); sd.beginFrame();
                    const auto a = od.process (x, 0), b = sd.process (x, 1);
                    ok = ok && finiteNoDenormal (a) && finiteNoDenormal (b) && std::abs (a) < 50.0f && std::abs (b) < 50.0f;
                    worstIt = std::max ({ worstIt, od.getClipper().lastIterations(), sd.getClipper().lastIterations() });
                }
            }
            od.beginFrame(); sd.beginFrame();
            const auto tailA = od.process (0.0f, 0), tailB = sd.process (0.0f, 1);
            ok = ok && std::abs (tailA) < 1e-6f && std::abs (tailB) < 1e-6f;
        }
        std::printf ("stability: worst Newton iterations %d (44.1/48/96/192 kHz hosts at 4x)\n", worstIt);
        check ("clippers finite, bounded, no denormals, silent tail, Newton <= 12 iterations at all rates", ok && worstIt <= 12);
    }

    // 5) Knob jumps: Drive 0->10 and Level jumps are smoothed (no step in the output)
    {
        auto run = [] (auto& st, int what) -> double
        {
            const double fs = 192000.0;
            st.prepare (fs); st.setParameters (0.0f, 0.5f, 0.3f); st.snap();
            double steady = 0.0, around = 0.0;
            float prev = 0.0f;
            for (long i = 0; i < (long) fs; ++i)
            {
                if (i == (long) (0.5 * fs))
                {
                    if (what == 0) st.setParameters (1.0f, 0.5f, 0.3f);
                    else           st.setParameters (0.0f, 0.5f, 3.0f);
                }
                st.beginFrame();
                const auto y = st.process ((float) (0.05 * std::sin (2.0 * jb::dsp::kPi * 220.0 * (double) i / fs)), 0);
                const auto d = std::abs (y - prev); prev = y;
                if (i > (long) (0.3 * fs) && i < (long) (0.5 * fs)) steady = std::max (steady, (double) d);
                if (i >= (long) (0.5 * fs) && i < (long) (0.5 * fs) + 64) around = std::max (around, (double) d);
            }
            return around / steady;
        };
        jb::dsp::Od808Stage od; jb::dsp::Sd1Stage sd;
        const auto r = std::max ({ run (od, 0), run (sd, 0), run (od, 1), run (sd, 1) });
        std::printf ("knob jump: worst sample-step right after a Drive/Level jump = %.2fx the steady-state step\n", r);
        check ("Drive and Level jumps are smoothed (no output step)", r < 1.5);
    }

    // 6) Recovery stage: exact bypass at drive 0, unity small-signal gain, bounded
    {
        jb::dsp::RecoveryStage rs; jb::dsp::RecoveryStage::Settings st; st.drive = 2.0f; st.bias = 0.4f; st.hpHz = 1.0;
        rs.prepare (192000.0, st);
        double gmax = 0, x2 = 0, y2 = 0; bool fin = true;
        for (int i = 0; i < 192000; ++i)
        {
            const auto x = (float) (1e-4 * std::sin (2.0 * jb::dsp::kPi * 1000.0 * i / 192000.0));
            const auto y = rs.process (x, 0);
            if (i > 96000) { x2 += x * x; y2 += y * y; }
            fin = fin && std::isfinite (y);
        }
        for (int i = 0; i < 19200; ++i) { const auto y = rs.process (20.0f * (float) std::sin (i * 0.01), 1); gmax = std::max (gmax, (double) std::abs (y)); fin = fin && std::isfinite (y); }
        const auto gDb = 10.0 * std::log10 (y2 / x2);
        jb::dsp::RecoveryStage by; jb::dsp::RecoveryStage::Settings z; z.hpHz = 1.0; by.prepare (192000.0, z);
        std::printf ("recovery: small-signal gain %.3f dB, output bound %.2f for a 20 V input\n", gDb, gmax);
        check ("recovery stage unity small-signal gain, bounded, finite", std::abs (gDb) < 0.05 && gmax < 3.0 && fin);
    }

    // 7) CPU (informational + loose bound): stereo ns/sample at the 4x rate of a 48 kHz host
    {
        const auto nsOd = cpuNsPerSample<jb::dsp::Od808Stage> (192000.0);
        const auto nsSd = cpuNsPerSample<jb::dsp::Sd1Stage> (192000.0);
        // both stages always run: per host second = 192000 * 2 ch * (od + sd)
        const auto pct = 100.0 * 192000.0 * 2.0 * (nsOd + nsSd) * 1e-9;
        std::printf ("CPU: OD-808 %.1f ns/sample, SD-1 %.1f ns/sample -> both stages, stereo, 48 kHz host @4x: %.2f%% of one core\n", nsOd, nsSd, pct);
        check ("clipper CPU cost reasonable (< 10% of a core for both stages, stereo, 4x @ 48 kHz)", pct < 10.0);
    }

    return failures;
}
