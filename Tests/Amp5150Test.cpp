// Rev2.4 5150 engine checks (Source/dsp/Amp5150.h, docs/5150.md).
#include "dsp/Amp5150.h"
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
using namespace jb::dsp;
int failures = 0;
void check (const char* what, bool ok)
{
    std::printf ("5150: %-68s %s\n", what, ok ? "ok" : "FAIL");
    if (! ok) ++failures;
}
double db (double x) { return 20.0 * std::log10 (x); }

/** Steady-state gain of the time-domain network for a sine (least-squares fit). */
double measured (const PreNetworkParts& parts, double knob, double f, double fs)
{
    PreGainNetwork n; n.setParts (parts); n.setKnob (knob); n.prepare (fs);
    const int settle = (int) (0.25 * fs), len = (int) (std::ceil (20.0 * fs / f));
    double ss = 0.0, sc = 0.0;
    for (int i = 0; i < settle + len; ++i)
    {
        const double ph = 2.0 * kPi * f * (double) i / fs;
        n.beginFrame();
        const double y = n.process ((float) std::sin (ph), 0);
        if (i >= settle) { ss += y * std::sin (ph); sc += y * std::cos (ph); }
    }
    return 2.0 * std::sqrt (ss * ss + sc * sc) / (double) len;
}
} // namespace

int amp5150Validation()
{
    failures = 0;

    // 1) Pre Gain networks: time-domain MNA matches the analytic nodal magnitude
    {
        double worst = 0.0;
        for (int which = 0; which < 3; ++which)
        {
            const auto parts = which == 0 ? leadPreParts() : rhythmPreParts (which == 2 ? 1.0 : 0.0, which == 2 ? 1.0 : 0.0);
            for (double knob : { 1.0, 5.0, 10.0 })
                for (double f : { 100.0, 1000.0, 5000.0 })
                {
                    const auto m = measured (parts, knob, f, 192000.0), a = PreGainNetwork::magnitude (parts, knob, f);
                    worst = std::max (worst, std::abs (db (m) - db (a)));
                }
        }
        std::printf ("5150: Pre Gain network time-domain vs analytic worst %.4f dB\n", worst);
        check ("Lead/Rhythm Pre Gain networks match their nodal magnitude (<0.1 dB)", worst < 0.1);
    }
    // 2) Lead network: the .001 bright cap matters at low Pre, fades at 10
    {
        const auto p = leadPreParts();
        auto tilt = [&] (double k) { return db (PreGainNetwork::magnitude (p, k, 5000.0) / PreGainNetwork::magnitude (p, k, 300.0)); };
        std::printf ("5150: Lead Pre 5k re 300 Hz: knob 3 %+.1f dB, 6 %+.1f dB, 10 %+.1f dB\n", tilt (3.0), tilt (6.0), tilt (10.0));
        check ("Lead Pre bright cap: treble tilt at low Pre, near flat at 10", tilt (3.0) > 6.0 && tilt (10.0) < 3.0 && tilt (3.0) > tilt (6.0));
        check ("Lead Pre: more knob = more gain (1 kHz)",
               PreGainNetwork::magnitude (p, 10.0, 1000.0) > PreGainNetwork::magnitude (p, 6.0, 1000.0)
                   && PreGainNetwork::magnitude (p, 6.0, 1000.0) > PreGainNetwork::magnitude (p, 2.0, 1000.0));
    }
    // 3) Rhythm Bright (C10 470p, sheet value): the manual's "+6 dB at 2 kHz" falls between
    //    Pre 5 and 7; like any pot-top bright cap it fades out toward 10. Crunch = more gain.
    {
        auto brightAt = [] (double k) { return db (PreGainNetwork::magnitude (rhythmPreParts (1.0, 0.0), k, 2000.0)
                                                   / PreGainNetwork::magnitude (rhythmPreParts (0.0, 0.0), k, 2000.0)); };
        std::printf ("5150: Rhythm Bright at 2 kHz: Pre 3 %+.2f, 5 %+.2f, 7 %+.2f, 10 %+.2f dB\n", brightAt (3.0), brightAt (5.0), brightAt (7.0), brightAt (10.0));
        check ("Rhythm Bright: +6 dB at 2 kHz lies between Pre 5 and 7; gone at 10",
               brightAt (5.0) > 6.0 && brightAt (7.0) < 6.0 && brightAt (7.0) > 4.0 && std::abs (brightAt (10.0)) < 0.1);
        const auto b = db (PreGainNetwork::magnitude (rhythmPreParts (1.0, 0.0), 5.0, 2000.0)
                           / PreGainNetwork::magnitude (rhythmPreParts (0.0, 0.0), 5.0, 2000.0));
        const auto c = db (PreGainNetwork::magnitude (rhythmPreParts (0.0, 1.0), 5.0, 1000.0)
                           / PreGainNetwork::magnitude (rhythmPreParts (0.0, 0.0), 5.0, 1000.0));
        std::printf ("5150: Rhythm Bright at 2 kHz (Pre 5) %+.2f dB; Crunch at 1 kHz %+.2f dB\n", b, c);
        check ("Rhythm Crunch adds gain (1 kHz)", c > 6.0);
    }
    // 4) Bright / Crunch switching is click-free (morphs over ~30 ms)
    {
        Cascade5150 r; r.configure (defaultVoicing5150(), true); r.setPreGain (5.0f); r.setSwitches (false, false);
        const double fs = 192000.0; r.prepare (fs);
        double steadyStep = 0.0, switchStep = 0.0, prev = 0.0;
        const int n = (int) (0.6 * fs), flip = (int) (0.3 * fs);
        for (int i = 0; i < n; ++i)
        {
            if (i == flip) r.setSwitches (true, true);
            r.beginFrame();
            const double y = r.process ((float) (0.02 * std::sin (2.0 * kPi * 110.0 * i / fs)), 0);
            const double d = std::abs (y - prev); prev = y;
            if (i > (int) (0.2 * fs) && i < flip) steadyStep = std::max (steadyStep, d);
            if (i >= flip && i < flip + (int) (0.01 * fs)) switchStep = std::max (switchStep, d);
        }
        // the end state is louder; a click would be a step far above the louder steady slope
        Cascade5150 e; e.configure (defaultVoicing5150(), true); e.setPreGain (5.0f); e.setSwitches (true, true); e.prepare (fs);
        double endStep = 0.0; prev = 0.0;
        for (int i = 0; i < (int) (0.3 * fs); ++i)
        {
            e.beginFrame();
            const double y = e.process ((float) (0.02 * std::sin (2.0 * kPi * 110.0 * i / fs)), 0);
            if (i > (int) (0.2 * fs)) endStep = std::max (endStep, std::abs (y - prev));
            prev = y;
        }
        std::printf ("5150: switch max step %.3g (steady off %.3g, steady on %.3g)\n", switchStep, steadyStep, endStep);
        check ("Bright+Crunch switch: no step beyond the steady slopes", switchStep <= 1.2 * std::max (steadyStep, endStep));
    }
    // 5) Resonance acts at the 4x12 peak: more knob = higher closed-loop Zout = bigger bump
    {
        const auto& v = defaultVoicing5150();
        double last = -1.0; bool rising = true; double bump0 = 0.0, bump10 = 0.0;
        for (double k : { 0.0, 2.5, 5.0, 7.5, 10.0 })
        {
            const auto z = closedLoopZout (v.zOpen, LeadResonance::resistanceFor (k), 0.0, v.spkF0);
            SpeakerLoadParams sp { v.spkRe, v.spkRes, v.spkF0, v.spkQ, z };
            const auto bump = db (SpeakerLoad::magnitude (sp, v.spkF0) / SpeakerLoad::magnitude (sp, 1000.0));
            rising = rising && bump > last; last = bump;
            if (k == 0.0) bump0 = bump;
            if (k == 10.0) bump10 = bump;
        }
        std::printf ("5150: 4x12 bump at %.0f Hz re 1 kHz: Resonance 0 %+.2f dB, 10 %+.2f dB\n", v.spkF0, bump0, bump10);
        check ("speaker bump grows with Resonance (Resonance acts at the cab peak)", rising && bump10 > bump0 + 3.0 && bump0 > 0.5);
    }
    // 6) Post Gain taper: 0 = silent, 10 = full, 5 = -20.8 dB
    check ("Post Gain: 0 silent, 10 unity, 5 = -20.8 dB (two-decade audio taper)",
           Power5150::postGain (0.0f) == 0.0f && std::abs (Power5150::postGain (10.0f) - 1.0f) < 1e-6f
               && std::abs (db (Power5150::postGain (5.0f)) + 20.83) < 0.05);
    return failures;
}
