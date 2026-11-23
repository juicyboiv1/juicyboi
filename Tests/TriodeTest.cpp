// Rev3 solved triode stages (Source/dsp/Triode.h): docs/triode.md.
//  1) DC operating points vs the voltages printed on the Dual Rectifier RF-1F sheet
//  2) four stored ngspice transients (Tests/triode-ngspice-ref.txt, written by
//     tools/triode/plot_triode.py --export): plate voltage and grid-cathode voltage
//  3) bias shift: a loud burst charges the coupling cap (grid walks negative),
//     a quiet tone afterwards recovers with the coupling time constant
//  4) stability at the 4x rate of 44.1/48/96/192 kHz with hot / impulse / quiet /
//     silent input: finite, bounded Newton iterations, silence stays at rest
//  5) gated cascades: switching amp, Rectifier voice and 5150 channel wakes the
//     newly heard cascade from rest without a click, and it converges to a
//     warm-running processor
//  6) the calibration keeps every stage's small-signal gain (behavioural slope)
#include "PluginProcessor.h"
#include "dsp/TriodeParts.h"
#include "dsp/TriodeCalibration.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
int failures = 0;
void check (const char* what, bool ok)
{
    std::printf ("triode: %-72s %s\n", what, ok ? "ok" : "FAIL");
    if (! ok) ++failures;
}

using namespace jb::dsp;

bool partsFor (const std::string& n, triode::StageParts& p)
{
    using namespace triodeparts;
    if (n == "rV1A") p = rectoV1A(); else if (n == "rV2A") p = rectoV2A();
    else if (n == "rV2B") p = rectoV2B(); else if (n == "rV3A") p = rectoV3A();
    else if (n == "fV1A") p = f5150V1A(); else if (n == "fV1B") p = f5150V1B();
    else if (n == "fV2A") p = f5150V2A(); else if (n == "fV2B") p = f5150V2B();
    else if (n == "fV5B") p = f5150V5B(); else if (n == "fV5A") p = f5150V5A();
    else return false;
    return true;
}

void set (JuicyboiAudioProcessor& p, const char* id, float v)
{
    if (auto* a = p.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v));
}
} // namespace

int triodeValidation()
{
    failures = 0;

    // 1) operating points vs RF-1F (plate / cathode volts printed on the sheet)
    {
        struct Ref { triode::StageParts p; double vp, vk, tolP, tolK; const char* n; };
        const Ref refs[] { { triodeparts::rectoV1A(), 200.0, 1.6, 10.0, 0.3, "V1A" },
                           { triodeparts::rectoV2A(), 280.0, 2.0, 10.0, 0.3, "V2A" },
                           { triodeparts::rectoV2B(), 384.0, 6.0, 10.0, 2.0, "V2B" },
                           { triodeparts::rectoV3A(), 213.0, 1.6, 10.0, 0.3, "V3A" } };
        bool ok = true;
        for (const auto& r : refs)
        {
            const auto op = triode::solveOperatingPoint (r.p);
            std::printf ("triode: RF-1F %s plate %.1f V (sheet %.0f), cathode %.2f V (sheet %.1f)\n", r.n, op.vp, r.vp, op.vk, r.vk);
            ok = ok && std::abs (op.vp - r.vp) < r.tolP && std::abs (op.vk - r.vk) < r.tolK;
        }
        check ("Rectifier operating points match the RF-1F sheet voltages", ok);
    }

    // 2) stored ngspice transients
    {
        std::ifstream in (std::string (JUICYBOI_TEST_DIR) + "/triode-ngspice-ref.txt");
        std::map<std::pair<std::string, double>, std::vector<std::array<double, 3>>> cases;
        std::string line;
        while (std::getline (in, line))
        {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss (line);
            std::string st; double amp, t, vp, vgk;
            ss >> st >> amp >> t >> vp >> vgk;
            cases[{ st, amp }].push_back ({ t, vp, vgk });
        }
        bool ok = cases.size() == 4;
        for (const auto& [key, rows] : cases)
        {
            triode::StageParts p; partsFor (key.first, p);
            const double fs = 192000.0;
            TriodeStage s; s.prepare (fs, p);
            const auto n = (long) std::llround (rows.back()[0] * fs) + 1;
            std::vector<double> vp ((size_t) n), vgk ((size_t) n);
            for (long i = 0; i < n; ++i)
            {
                vp[(size_t) i] = s.process (key.second * std::sin (2.0 * kPi * 1000.0 * (double) i / fs), 0) + s.operatingPoint().vp;
                vgk[(size_t) i] = s.gridBias (0);
            }
            double e2 = 0, r2 = 0, worst = 0, gw = 0, mean = 0;
            for (const auto& r : rows) mean += r[1] / (double) rows.size();
            double swing = 0;
            for (const auto& r : rows) swing = std::max (swing, std::abs (r[1] - s.operatingPoint().vp));
            for (const auto& r : rows)
            {
                const auto i = (size_t) std::llround (r[0] * fs);
                const auto e = vp[i] - r[1];
                e2 += e * e; r2 += (r[1] - mean) * (r[1] - mean);
                worst = std::max (worst, std::abs (e));
                gw = std::max (gw, std::abs (vgk[i] - r[2]));
            }
            const auto rmsPct = 100.0 * std::sqrt (e2 / r2), worstPct = 100.0 * worst / swing;
            std::printf ("triode: ngspice %s %g V: rms err %.4f %%, worst %.4f %% of swing, Vgk worst %.2f mV\n",
                         key.first.c_str(), key.second, rmsPct, worstPct, gw * 1e3);
            ok = ok && rmsPct < 0.1 && worstPct < 1.0 && gw < 0.02;
        }
        check ("4 stored ngspice transients: rms < 0.1 %, worst < 1 % of swing, Vgk < 20 mV", ok);
    }

    // 3) bias shift and recovery (V3A: R225 220K stopper, C27 .02u into R106 330K)
    {
        const double fs = 192000.0;
        TriodeStage s; s.prepare (fs, triodeparts::rectoV3A());
        double minShift = 0.0, shiftAtEnd = 0.0, shift50 = 0.0;
        for (long i = 0; i < (long) (0.4 * fs); ++i)
        {
            const auto t = (double) i / fs;
            s.process ((t < 0.1 ? 40.0 : 0.5) * std::sin (2.0 * kPi * 100.0 * t), 0);
            minShift = std::min (minShift, s.couplingShift (0));
            if (i == (long) (0.15 * fs)) shift50 = s.couplingShift (0);
            shiftAtEnd = s.couplingShift (0);
        }
        std::printf ("triode: V3A bias shift: deepest %.2f V, 50 ms after the burst %.2f V, after 300 ms %.3f V\n", minShift, shift50, shiftAtEnd);
        check ("loud burst shifts the coupling cap negative (> 1 V) and it recovers (< 0.2 V)", minShift < -1.0 && std::abs (shiftAtEnd) < 0.2);
        (void) shift50;
    }

    // 4) stability matrix
    {
        bool ok = true; int maxIt = 0;
        for (double base : { 44100.0, 48000.0, 96000.0, 192000.0 })
            for (const auto* n : { "rV1A", "rV2A", "rV2B", "rV3A", "fV1A", "fV1B", "fV2A", "fV2B", "fV5B", "fV5A" })
            {
                triode::StageParts p; partsFor (n, p);
                const auto fs = 4.0 * base;
                TriodeStage s; s.prepare (fs, p);
                // silence stays at rest
                double rest = 0;
                for (int i = 0; i < 1000; ++i) rest = std::max (rest, std::abs (s.process (0.0, 0)));
                ok = ok && rest < 1.0e-6;
                for (int k = 0; k < 3; ++k)
                    for (long i = 0; i < (long) (0.05 * fs); ++i)
                    {
                        const auto t = (double) i / fs;
                        const auto x = k == 0 ? 300.0 * std::sin (2.0 * kPi * 5000.0 * t)
                                     : k == 1 ? (i == 10 ? 500.0 : 0.0)
                                              : 1.0e-4 * std::sin (2.0 * kPi * 440.0 * t);
                        const auto y = s.process (x, 0);
                        ok = ok && std::isfinite (y) && std::abs (y) < 1000.0;
                        maxIt = std::max (maxIt, s.lastIterations());
                    }
            }
        std::printf ("triode: stability matrix worst Newton iterations %d\n", maxIt);
        check ("finite and bounded at 4x of 44.1/48/96/192k (hot/impulse/quiet/silent), <= 20 iterations", ok && maxIt <= 20);
    }

    // 5) gated cascades: click-free wake-up, converges to a warm processor
    {
        auto setup = [] (JuicyboiAudioProcessor& p, bool warm)
        {
            p.keepAllCascadesWarm = warm;
            set (p, "gate_enable", 0.0f); set (p, "m5150_gate_enable", 0.0f);
            set (p, "cab_enable", 0.0f); set (p, "m5150_cab_enable", 0.0f);
            p.setPlayConfigDetails (1, 2, 48000.0, 128);
            p.prepareToPlay (48000.0, 128);
        };
        JuicyboiAudioProcessor gated, warm;
        setup (gated, false); setup (warm, true);
        juce::MidiBuffer midi;
        float maxStep = 0.0f, steadyStep = 0.0f, lateErr = 0.0f, peak = 0.0f;
        float prev = 0.0f;
        for (int b = 0; b < 1500; ++b)
        {
            // switches: amp (b 200, 700), Rectifier voice Red v2 -> v1 (b 400), 5150 channel (b 900), Orange (b 1200)
            for (auto* p : { &gated, &warm })
            {
                if (b == 200) set (*p, "preamp_model", 1.0f);
                if (b == 400) set (*p, "red_voice", 0.0f);
                if (b == 700) set (*p, "preamp_model", 0.0f);
                if (b == 900) set (*p, "m5150_channel", 0.0f);
                if (b == 1200) set (*p, "channel", 1.0f);
            }
            juce::AudioBuffer<float> x (2, 128), y (2, 128);
            for (int i = 0; i < 128; ++i)
            {
                const auto t = (double) (b * 128 + i) / 48000.0;
                x.setSample (0, i, (float) (0.15 * std::sin (2.0 * kPi * 110.0 * t)));
            }
            x.setSample (1, 0, 0.0f);
            y.makeCopyOf (x);
            gated.processBlock (x, midi); warm.processBlock (y, midi);
            for (int i = 0; i < 128; ++i)
            {
                const auto v = x.getSample (0, i);
                const auto step = std::abs (v - prev); prev = v;
                peak = std::max (peak, std::abs (v));
                if (b > 20 && b < 200) steadyStep = std::max (steadyStep, step);
                if (b > 20) maxStep = std::max (maxStep, step);
                // 0.4 s after each switch (and before the next) the gated processor matches the warm one
                const bool late = (b > 350 && b < 400) || (b > 650 && b < 700) || (b > 1150 && b < 1200) || b > 1450;
                if (late) lateErr = std::max (lateErr, std::abs (v - y.getSample (0, i)));
            }
        }
        std::printf ("triode: gated switching: max step %.4f (steady %.4f), gated vs warm 0.4 s after a switch %.6f (peak %.3f)\n",
                     (double) maxStep, (double) steadyStep, (double) lateErr, (double) peak);
        check ("gated wake-up is click-free (max step < 1.5x the steady clipped-wave step)", maxStep < 1.5f * steadyStep);
        check ("a woken cascade converges to the warm-running output (< 1 % of peak)", lateErr < 0.01f * peak);
    }

    // 6) calibration keeps the behavioural small-signal gain
    {
        bool ok = true;
        const auto v = voicings::redV2();
        for (int i = 0; i < 4; ++i)
        {
            const auto& st = v.stages[(size_t) i];
            const auto m = triode::mapStage (v.tubes[(size_t) i], st.gain, st.bias, st.posHardness, st.negHardness, v.triodeDrive[(size_t) i]);
            const auto phys = triode::clipPoints (v.tubes[(size_t) i]);
            const auto bh = triode::behaviouralClip (st.gain, st.bias, st.posHardness, st.negHardness);
            ok = ok && std::abs (m.inScale * phys.gain * m.outScale / bh.gain - 1.0) < 1.0e-9;
        }
        check ("drive mapping keeps each stage's small-signal gain", ok);
    }
    return failures;
}
