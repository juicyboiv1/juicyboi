// juicyboi_tonestack_dump <red|orange|lead> <fs> <bass> <mid> <treble>
// CSV on stdout: hz, analog_db (closed-form H at s=j*w, + makeup), digital_db (bilinear
// transfer function of the running stack), measured_db (steady-state sine through
// PassiveToneStack::process). Used by tools/tonestack/plot_tonestack.py.
#include "dsp/ToneStack.h"
#include "dsp/Lead5150.h"
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace jb::dsp;

int main (int argc, char** argv)
{
    if (argc < 6) { std::fprintf (stderr, "usage: %s <red|orange|lead> <fs> <bass> <mid> <treble>\n", argv[0]); return 2; }
    const std::string which = argv[1];
    const auto c = which == "lead" ? leadTmb() : (which == "orange" ? voicings::orangeTmb() : voicings::redTmb());
    const double fs = std::atof (argv[2]);
    const float b = (float) std::atof (argv[3]), m = (float) std::atof (argv[4]), t = (float) std::atof (argv[5]);
    const auto h = tmb::analog (c, tmb::treblePosition (t), tmb::midPosition (m), tmb::bassPosition (b));
    std::printf ("hz,analog_db,digital_db,measured_db\n");
    for (double hz = 20.0; hz < std::min (20000.0, 0.49 * fs); hz *= 1.06)
    {
        PassiveToneStack st; st.setComponents (c); st.setKnobs (b, m, t); st.prepare (fs);
        const auto an = 20.0 * std::log10 (std::abs (tmb::response (h, hz))) + c.makeupDb;
        const auto dg = 20.0 * std::log10 (std::abs (tmb::response (st.transferFunction(), hz, fs)) * st.makeupGain());
        double in = 0.0, out = 0.0;
        const int settle = (int) (fs * 0.4), meas = (int) std::max (fs * 0.1, 20.0 * fs / hz);
        for (int i = 0; i < settle + meas; ++i)
        {
            const auto x = std::sin (2.0 * kPi * hz * i / fs);
            st.beginFrame();
            const auto y = (double) st.process ((float) x, 0);
            if (i >= settle) { in += x * x; out += y * y; }
        }
        std::printf ("%.3f,%.5f,%.5f,%.5f\n", hz, an, dg, 10.0 * std::log10 (out / in));
    }
    return 0;
}
