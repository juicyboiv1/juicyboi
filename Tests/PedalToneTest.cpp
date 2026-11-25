// Rev3.1 component-level Boost post-clip network (post LP, active tone stage,
// Level pot, output buffer): exact-derivation checks, ngspice agreement,
// processing vs coefficients, zipper-free knob moves and level vs Rev3.
#include "dsp/Od808Stage.h"
#include "dsp/Sd1Stage.h"

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
using C = std::complex<double>;

template <typename Stage>
double db (Stage& s, double f, bool post) // digital response of the snapped stage
{
    return 20.0 * std::log10 (std::abs (post ? s.getPost().template digitalPost<C> (f, false)
                                             : s.getPost().template digitalTone<C> (f)));
}

// ngspice reference (tools/pedaltone/plot_pedaltone.py --export):
// rows "pedal tone01 f |Htone|dB |Hpost|dB" (Hpost: clip out -> pedal out at Level = 0 dB knob, no makeup)
struct RefRow { std::string pedal; double tone, f, tone_db, post_db; };
std::vector<RefRow> loadRef()
{
    std::vector<RefRow> rows;
    std::ifstream in (std::string (JUICYBOI_TEST_DIR) + "/pedal-tone-ngspice-ref.txt");
    std::string line;
    while (std::getline (in, line))
    {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss (line);
        RefRow r; ss >> r.pedal >> r.tone >> r.f >> r.tone_db >> r.post_db;
        rows.push_back (r);
    }
    return rows;
}

template <typename Stage>
double guitarRmsDb (float drive, float tone, float level)
{
    const double fs = 192000.0;
    Stage s; s.prepare (fs); s.setParameters (drive, tone, level); s.snap();
    double acc = 0.0; const long n = (long) fs;
    for (long i = 0; i < n; ++i)
    {
        const auto t = (double) i / fs;
        const auto x = 0.18 * std::sin (2.0 * jb::dsp::kPi * 82.4 * t) + 0.05 * std::sin (2.0 * jb::dsp::kPi * 41.2 * t)
                       + 0.04 * std::sin (2.0 * jb::dsp::kPi * 733.0 * t);
        s.beginFrame();
        const auto y = (double) s.process ((float) x, 0);
        if (i >= n / 2) acc += y * y;
    }
    return 10.0 * std::log10 (acc / (double) (n - n / 2));
}
} // namespace

int pedalToneValidation()
{
    failures = 0;
    const double fs = 192000.0;

    // 1) Digital (bilinear) tone stage vs the exact analog derivation, 20 Hz - 10 kHz
    {
        double worst = 0.0;
        for (int ped = 0; ped < 2; ++ped)
            for (double tone : { 0.0, 0.25, 0.5, 0.75, 1.0 })
            {
                jb::dsp::Od808Stage od; jb::dsp::Sd1Stage sd;
                od.prepare (fs); sd.prepare (fs);
                od.setParameters (0.5f, (float) tone, 1.0f); od.snap();
                sd.setParameters (0.5f, (float) tone, 1.0f); sd.snap();
                const auto comp = ped == 0 ? jb::dsp::Od808Stage::toneComponents() : jb::dsp::Sd1Stage::toneComponents();
                const auto tau = ped == 0 ? od.getPost().toneTaper (tone) : sd.getPost().toneTaper (tone);
                for (double f = 20.0; f <= 10000.0; f *= 1.1)
                {
                    const auto a = 20.0 * std::log10 (std::abs (jb::dsp::PedalPostStage::toneResponse (comp, tau, C (0.0, 2.0 * jb::dsp::kPi * f))));
                    const auto d = ped == 0 ? db (od, f, false) : db (sd, f, false);
                    worst = std::max (worst, std::abs (a - d));
                }
            }
        std::printf ("pedal tone: digital vs exact analog tone stage, worst %.4f dB (20 Hz-10 kHz, Tone 0..10)\n", worst);
        check ("bilinear tone stage within 0.1 dB of the exact analog response to 10 kHz", worst < 0.1);
    }

    // 2) vs ngspice (tone stage with ideal op-amp; whole post network with a real BJT follower)
    {
        const auto ref = loadRef();
        double wTone = 0.0, wPost = 0.0;
        for (const auto& r : ref)
        {
            if (r.f < 20.0 || r.f > 20000.0) continue;
            jb::dsp::Od808Stage od; jb::dsp::Sd1Stage sd;
            od.prepare (fs); sd.prepare (fs);
            od.setParameters (0.5f, (float) r.tone, 1.0f); od.snap();
            sd.setParameters (0.5f, (float) r.tone, 1.0f); sd.snap();
            const bool isOd = r.pedal == "od";
            const auto t = isOd ? db (od, r.f, false) : db (sd, r.f, false);
            const auto p = isOd ? db (od, r.f, true) : db (sd, r.f, true);
            wTone = std::max (wTone, std::abs (t - r.tone_db));
            wPost = std::max (wPost, std::abs (p - r.post_db));
        }
        std::printf ("pedal tone vs ngspice (%zu points, 20 Hz-20 kHz): tone stage worst %.3f dB, whole post network worst %.3f dB\n",
                     ref.size(), wTone, wPost);
        check ("tone stage within 0.25 dB of ngspice, post network within 0.5 dB (20 Hz-20 kHz)",
               ref.size() >= 100 && wTone < 0.25 && wPost < 0.5);
    }

    // 3) process() implements the coefficients: measured 1 kHz / 100 Hz gain of the post stage
    {
        double worst = 0.0;
        for (double f : { 100.0, 1000.0, 5000.0 })
        {
            jb::dsp::PedalPostStage ps;
            ps.prepare (fs, jb::dsp::Od808Stage::toneComponents(), jb::dsp::Od808Stage::outputComponents(), 1.0);
            ps.setTargets (0.7f, 1.0f); ps.snap(); ps.reset();
            double re = 0, im = 0; const long n = (long) (1.0 * fs), n0 = n / 2;
            for (long i = 0; i < n; ++i)
            {
                const auto ph = 2.0 * jb::dsp::kPi * f * (double) i / fs;
                ps.beginFrame();
                const auto y = (double) ps.process ((float) (0.01 * std::sin (ph)), 0);
                if (i >= n0) { re += y * std::sin (ph); im += y * std::cos (ph); }
            }
            const auto meas = 2.0 * std::sqrt (re * re + im * im) / (double) (n - n0) / 0.01;
            const auto pred = std::abs (ps.digitalPost<C> (f, false));
            worst = std::max (worst, std::abs (20.0 * std::log10 (meas / pred)));
        }
        std::printf ("pedal tone: measured vs coefficient response, worst %.4f dB\n", worst);
        check ("process() matches the digital response (< 0.02 dB)", worst < 0.02);
    }

    // 4) Tone / Level jumps are smoothed (no step in the output)
    {
        auto run = [] (auto& st, int what) -> double
        {
            const double r = 192000.0;
            st.prepare (r); st.setParameters (0.0f, what == 0 ? 0.0f : 0.5f, 0.3f); st.snap();
            double steady = 0.0, around = 0.0; float prev = 0.0f;
            for (long i = 0; i < (long) r; ++i)
            {
                if (i == (long) (0.5 * r))
                    st.setParameters (0.0f, what == 0 ? 1.0f : 0.5f, what == 0 ? 0.3f : 3.0f);
                st.beginFrame();
                const auto y = st.process ((float) (0.05 * std::sin (2.0 * jb::dsp::kPi * 220.0 * (double) i / r)), 0);
                const auto d = std::abs (y - prev); prev = y;
                if (i > (long) (0.3 * r) && i < (long) (0.5 * r)) steady = std::max (steady, (double) d);
                if (i >= (long) (0.5 * r) && i < (long) (0.5 * r) + 64) around = std::max (around, (double) d);
            }
            return around / steady;
        };
        jb::dsp::Od808Stage od; jb::dsp::Sd1Stage sd;
        const auto r = std::max ({ run (od, 0), run (sd, 0), run (od, 1), run (sd, 1) });
        std::printf ("pedal tone: worst sample-step right after a Tone 0->10 / Level jump = %.2fx steady\n", r);
        check ("Tone and Level jumps are zipper-free", r < 1.5);
    }

    // 5) Level vs Rev3 (fixture guitar mix, 192 kHz stage render). Rev3 dB values were
    //    measured from the Rev3 build (926ef71) with tools/pedaltone/compare_rev3.py.
    {
        struct K { bool od; float drive, tone; double rev3Db; };
        const K ks[] = {
            { true, 0.2f, 0.5f, -12.556 }, { true, 0.5f, 0.5f, -11.801 }, { true, 1.0f, 0.5f, -10.988 },
            { false, 0.2f, 0.5f, -9.987 }, { false, 0.5f, 0.5f, -7.706 }, { false, 1.0f, 0.5f, -7.116 },
        };
        double worst = 0.0;
        for (const auto& k : ks)
        {
            const auto d = (k.od ? guitarRmsDb<jb::dsp::Od808Stage> (k.drive, k.tone, 1.0f)
                                 : guitarRmsDb<jb::dsp::Sd1Stage> (k.drive, k.tone, 1.0f)) - k.rev3Db;
            std::printf ("pedal level vs Rev3: %s Drive %.0f Tone 5: %+.2f dB\n", k.od ? "OD-808" : "SD-1", 10.0 * k.drive, d);
            worst = std::max (worst, std::abs (d));
        }
        check ("Boost output within 1 dB of Rev3 at Tone 5 (Drive 2/5/10)", worst < 1.0);
    }
    return failures;
}
