// Offline dump of the Rev2.2 Boost clipping stages, for tools/clippers/plot_clippers.py.
// usage: juicyboi_clipper_dump <od|sd> <drive01> <ampVolts> <freqHz> <fs> <seconds> <out.txt> [core|full]
// Writes "t vin vout" rows. "core" = input coupling + solved op-amp clipper only
// (what the ngspice netlists simulate); "full" = whole Boost stage at Tone 0.5 / Level 1.

#include "../Source/dsp/Od808Stage.h"
#include "../Source/dsp/Sd1Stage.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>

int main (int argc, char** argv)
{
    if (argc < 8) { std::fprintf (stderr, "usage: %s od|sd drive01 amp freq fs seconds out [core|full]\n", argv[0]); return 2; }
    const std::string type = argv[1];
    const auto drive = std::atof (argv[2]);
    const auto amp   = std::atof (argv[3]);
    const auto freq  = std::atof (argv[4]);
    const auto fs    = std::atof (argv[5]);
    const auto secs  = std::atof (argv[6]);
    const bool full  = argc > 8 && std::strcmp (argv[8], "full") == 0;

    jb::dsp::Od808Stage od;
    jb::dsp::Sd1Stage sd;
    od.prepare (fs); sd.prepare (fs);
    od.setParameters ((float) drive, 0.5f, 1.0f); od.snap();
    sd.setParameters ((float) drive, 0.5f, 1.0f); sd.snap();

    auto* f = std::fopen (argv[7], "w");
    if (f == nullptr) return 1;
    const auto n = (long) std::llround (secs * fs);
    int maxIt = 0; double itSum = 0;
    for (long i = 0; i < n; ++i)
    {
        const auto t = (double) i / fs;
        const auto x = (float) (amp * std::sin (2.0 * jb::dsp::kPi * freq * t));
        float y;
        int it;
        if (type == "od") { od.beginFrame(); y = full ? od.process (x, 0) : od.processClipper (x, 0); it = od.getClipper().lastIterations(); }
        else              { sd.beginFrame(); y = full ? sd.process (x, 0) : sd.processClipper (x, 0); it = sd.getClipper().lastIterations(); }
        maxIt = std::max (maxIt, it); itSum += it;
        std::fprintf (f, "%.9e %.9e %.9e\n", t, (double) x, (double) y);
    }
    std::fclose (f);
    std::fprintf (stderr, "newton iterations: mean %.2f max %d\n", itSum / (double) n, maxIt);
    return 0;
}
