// Offline dump of one Rev3 solved triode stage, for tools/triode/plot_triode.py.
// usage: juicyboi_triode_dump <stage> <amp> <freq> <fs> <seconds> <out.txt> [burstEnd burstAmp2]
//   stage: rV1A rV2A rV2B rV3A fV1A fV1B fV2A fV2B fV5B fV5A
//   Sine of `amp` volts at `freq` from rest (quiescent point). With burstEnd > 0
//   the amplitude drops to amp2 after burstEnd seconds (blocking recovery).
// Writes "t vs vplate_dev vgk" rows; prints the operating point and Newton stats.
// usage: juicyboi_triode_dump op   -> operating points of every stage
// usage: juicyboi_triode_dump bench <fs> -> ns per stage-sample (hot sine)

#include "../Source/dsp/TriodeParts.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

using namespace jb::dsp;

static bool partsFor (const std::string& n, triode::StageParts& p)
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

int main (int argc, char** argv)
{
    if (argc >= 2 && std::strcmp (argv[1], "op") == 0)
    {
        for (auto* n : { "rV1A", "rV2A", "rV2B", "rV3A", "fV1A", "fV1B", "fV2A", "fV2B", "fV5B", "fV5A" })
        {
            triode::StageParts p; partsFor (n, p);
            TriodeStage s; s.prepare (192000.0, p);
            const auto& op = s.operatingPoint();
            std::printf ("%s Vp %.2f Vk %.3f Vg %.4f Ip %.4f mA Vgk %.3f gain %.2f\n", n, op.vp, op.vk, op.vg, op.ip * 1e3, op.vg - op.vk, s.smallSignalGain());
        }
        return 0;
    }
    if (argc >= 2 && std::strcmp (argv[1], "bench") == 0)
    {
        const auto fs = argc > 2 ? std::atof (argv[2]) : 192000.0;
        double total = 0; long iters = 0, samples = 0;
        for (auto* n : { "rV1A", "rV2A", "rV2B", "rV3A", "fV1A", "fV1B", "fV2A", "fV2B", "fV5B", "fV5A" })
        {
            triode::StageParts p; partsFor (n, p);
            TriodeStage s; s.prepare (fs, p);
            const long N = (long) fs * 2;
            std::vector<double> x ((size_t) N);
            for (long i = 0; i < N; ++i) x[(size_t) i] = 6.0 * std::sin (2 * kPi * 110.0 * i / fs) * (0.5 + 0.5 * std::sin (2 * kPi * 0.7 * i / fs));
            volatile double sink = 0;
            const auto t0 = std::chrono::steady_clock::now();
            for (long i = 0; i < N; ++i) { sink = sink + s.process (x[(size_t) i], 0); iters += s.lastIterations(); }
            total += std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            samples += N;
        }
        std::printf ("ns per stage-sample %.1f, mean Newton iterations %.2f\n", total / samples * 1e9, (double) iters / samples);
        return 0;
    }
    if (argc < 7) { std::fprintf (stderr, "usage: %s stage amp freq fs seconds out [burstEnd amp2]\n", argv[0]); return 2; }
    triode::StageParts p;
    if (! partsFor (argv[1], p)) { std::fprintf (stderr, "unknown stage\n"); return 2; }
    const auto amp = std::atof (argv[2]), freq = std::atof (argv[3]), fs = std::atof (argv[4]), secs = std::atof (argv[5]);
    const auto burstEnd = argc > 8 ? std::atof (argv[7]) : 0.0;
    const auto amp2 = argc > 8 ? std::atof (argv[8]) : amp;
    TriodeStage s; s.prepare (fs, p);
    auto* f = std::fopen (argv[6], "w");
    if (! f) return 1;
    const auto n = (long) std::llround (secs * fs);
    int maxIt = 0; double itSum = 0;
    for (long i = 0; i < n; ++i)
    {
        const auto t = (double) i / fs;
        const auto a = (burstEnd > 0.0 && t >= burstEnd) ? amp2 : amp;
        const auto x = a * std::sin (2.0 * kPi * freq * t);
        const auto y = s.process (x, 0);
        maxIt = std::max (maxIt, s.lastIterations()); itSum += s.lastIterations();
        std::fprintf (f, "%.9e %.9e %.9e %.9e\n", t, x, y + s.operatingPoint().vp, s.gridBias (0));
    }
    std::fclose (f);
    std::fprintf (stderr, "newton iterations: mean %.2f max %d\n", itSum / (double) n, maxIt);
    return 0;
}
