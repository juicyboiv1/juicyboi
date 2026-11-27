// Offline renders of a whole Boost stage (OD-808 / SD-1) for
// tools/pedaltone/compare_rev3.py. Built against Rev3.1 (this tree) and, by the
// script, against a Rev3 checkout, so both versions are measured identically.
// usage:
//   pedal_compare ir   <od|sd> drive01 tone01 levelGain fs secs amp out.f32
//   pedal_compare sine <od|sd> drive01 tone01 levelGain fs secs amp freq out.f32
//   pedal_compare gtr  <od|sd> drive01 tone01 levelGain fs secs out.f32   (82.4 + 41.2 + 733 Hz, fixture mix)
//   pedal_compare cpu  <od|sd> fs                                         (ns per stereo frame)
//   pedal_compare resp <od|sd> tone01 levelGain fs   (Rev3.1 only: "f |Htone| argHtone |Hpost| argHpost",
//                                                      Hpost without makeup, digital coefficients)
#include "dsp/Od808Stage.h"
#include "dsp/Sd1Stage.h"

#include <chrono>
#include <complex>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

template <typename Stage>
static int run (int argc, char** argv)
{
    const std::string mode = argv[1];
    if (mode == "cpu")
    {
        const auto fs = std::atof (argv[3]);
        Stage s; s.prepare (fs); s.setParameters (0.5f, 0.5f, 1.0f); s.snap();
        const long n = (long) (3.0 * fs);
        volatile float sink = 0.0f;
        const auto t0 = std::chrono::steady_clock::now();
        for (long i = 0; i < n; ++i)
        {
            const auto x = (float) (0.3 * std::sin (2.0 * jb::dsp::kPi * 196.0 * (double) i / fs));
            s.beginFrame();
            sink = sink + s.process (x, 0) + s.process (x, 1);
        }
        const auto ns = std::chrono::duration<double, std::nano> (std::chrono::steady_clock::now() - t0).count() / (double) n;
        std::printf ("%.3f\n", ns);
        return 0;
    }
#ifdef PEDAL_RESP
    if (mode == "resp")
    {
        Stage s; s.prepare (std::atof (argv[5])); s.setParameters (0.5f, (float) std::atof (argv[3]), (float) std::atof (argv[4])); s.snap();
        for (double f = 10.0; f <= 40000.0; f *= std::pow (2.0, 1.0 / 24.0))
        {
            const auto t = s.getPost().template digitalTone<std::complex<double>> (f);
            const auto p = s.getPost().template digitalPost<std::complex<double>> (f, false);
            std::printf ("%.6f %.9e %.9e %.9e %.9e\n", f, std::abs (t), std::arg (t), std::abs (p), std::arg (p));
        }
        return 0;
    }
#endif
    const auto drive = (float) std::atof (argv[3]);
    const auto tone  = (float) std::atof (argv[4]);
    const auto level = (float) std::atof (argv[5]);
    const auto fs    = std::atof (argv[6]);
    const auto secs  = std::atof (argv[7]);
    Stage s; s.prepare (fs); s.setParameters (drive, tone, level); s.snap();
    const auto n = (long) std::llround (secs * fs);
    std::vector<float> out ((size_t) n);
    const char* path = argv[argc - 1];
    const double amp = (mode == "gtr") ? 1.0 : std::atof (argv[8]);
    const double freq = (mode == "sine") ? std::atof (argv[9]) : 0.0;
    for (long i = 0; i < n; ++i)
    {
        const auto t = (double) i / fs;
        double x = 0.0;
        if (mode == "ir") x = (i == 0) ? amp : 0.0;
        else if (mode == "sine") x = amp * std::sin (2.0 * jb::dsp::kPi * freq * t);
        else x = 0.18 * std::sin (2.0 * jb::dsp::kPi * 82.4 * t) + 0.05 * std::sin (2.0 * jb::dsp::kPi * 41.2 * t)
                 + 0.04 * std::sin (2.0 * jb::dsp::kPi * 733.0 * t);
        s.beginFrame();
        out[(size_t) i] = s.process ((float) x, 0);
    }
    auto* f = std::fopen (path, "wb");
    if (! f) return 1;
    std::fwrite (out.data(), sizeof (float), out.size(), f);
    std::fclose (f);
    return 0;
}

int main (int argc, char** argv)
{
    if (argc < 4) { std::fprintf (stderr, "see header for usage\n"); return 2; }
    return std::string (argv[2]) == "od" ? run<jb::dsp::Od808Stage> (argc, argv) : run<jb::dsp::Sd1Stage> (argc, argv);
}
