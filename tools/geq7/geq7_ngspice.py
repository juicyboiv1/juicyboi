#!/usr/bin/env python3
"""juicyboi Rev3.2 GEQ-7: ngspice AC validation of the GE-7-style graphic EQ circuit.

Builds the whole pedal (input network, pre-emphasis, Level stage, 7-band stage with
op-amp gyrators, de-emphasis, 2SC1815 emitter follower, output network) for a set of
slider positions, runs ngspice AC, and compares with the analytic model (ge7_model.py,
same equations as Source/dsp/GraphicEq.cpp).

  python3 geq7_ngspice.py            # 4558 (3 MHz) band/Level op-amps, TL022 (0.5 MHz) gyrator followers
  python3 geq7_ngspice.py --real     # also ideal op-amps (shows how much the op-amp bandwidth matters)
  python3 geq7_ngspice.py --export   # write Tests/geq7-ngspice-ref.txt (4558 / TL022 single-pole op-amps)
  python3 geq7_ngspice.py --plot out.png
"""
import os, subprocess, sys, tempfile
import numpy as np
import ge7_model as m

SETTINGS = [
    ("flat",        [0.5] * 7, 0.5),
    ("all+",        [1.0] * 7, 0.5),
    ("all-",        [0.0] * 7, 0.5),
    ("alt+-",       [1, 0, 1, 0, 1, 0, 1], 0.5),
    ("alt-+",       [0, 1, 0, 1, 0, 1, 0], 0.5),
    ("mixed",       [0.92, 0.3, 0.5, 0.95, 0.08, 0.7, 0.88], 0.8),
    ("mid-scoop",   [0.9, 0.7, 0.2, 0.05, 0.25, 0.75, 0.9], 0.35),
    ("level+",      [0.5] * 7, 1.0),
    ("level-",      [0.5] * 7, 0.0),
] + [("solo%d+" % k, [1.0 if i == k else 0.5 for i in range(7)], 0.5) for k in range(7)]

GYR_NAMES = ["100", "200", "400", "800", "1k6", "3k2"]


def netlist(w, wl, outfile, gbw_band=3e6, gbw_gyr=0.5e6, a0=1e9):
    def pot(name, a, b, wiper, pos):
        return ("R%sa %s %s %g\nR%sb %s %s %g\n" %
                (name, a, wiper, max(pos * 10e3, 1e-3), name, wiper, b, max((1 - pos) * 10e3, 1e-3)))
    s = []
    s.append("* juicyboi Rev3.2 GEQ-7 (GE-7-style) validation netlist, AC\n")
    s.append(".subckt OPA inp inn out params: gbw=1e13 a0=1e9\n"
             "G1 0 v inp inn 1\nC1 v 0 {1/(2*3.14159265358979*gbw)}\nR1 v 0 {a0}\nE1 out 0 v 0 1\n.ends\n")
    s.append("Vcc vcc 0 DC 9\nVr vr 0 DC 4.5\nVin src 0 DC 0 AC 1\n")
    # input + U5a pre-emphasis
    s.append("R30 src n1 10k\nC22 n1 p 47n\nR29 p vr 470k\n")
    s.append("XU5a p m1 o1 OPA params: gbw=1e13 a0=1e9\nR31 o1 m1 4.7k\nR28 m1 x1 470\nC20 x1 vr 15n\n")
    # U4b Level stage
    s.append("R27 o1 p2 10k\nXU4b p2 m2 o2 OPA params: gbw=%g a0=%g\nR26 o2 m2 10k\n" % (gbw_band, a0))
    s.append(pot("LV", "p2", "m2", "wl", wl))
    s.append("R2 wl xl 2.2k\nC16 xl vr 10u\n")
    # U4a band stage
    s.append("R24 o2 p3 3.3k\nXU4a p3 m3 o3 OPA params: gbw=%g a0=%g\nR25 o3 m3 3.3k\n" % (gbw_band, a0))
    for k in range(6):
        cs, c2, r1, r2 = m.GYR[k]
        n = GYR_NAMES[k]
        s.append(pot("B" + n, "p3", "m3", "w" + n, w[k]))
        s.append("CS%s w%s g%s %g\nCG%s g%s u%s %g\nRG%s u%s vr %g\n" % (n, n, n, cs, n, n, n, c2, n, n, r1))
        s.append("XF%s u%s f%s f%s OPA params: gbw=%g a0=%g\nRR%s f%s g%s %g\nRL%s f%s 0 470k\n" %
                 (n, n, n, n, gbw_gyr, a0, n, n, n, r2, n, n))
    s.append(pot("B6k4", "p3", "m3", "w6k4", w[6]))
    s.append("C1 w6k4 g6k4 47n\nR3 g6k4 vr 820\n")
    # output: C19, de-emphasis, follower, output network (unloaded)
    s.append("C19 o3 a 1u\nR32 a vr 1meg\nR33 a b 4.7k\nR36 b y 470\nC27 y vr 15n\n")
    s.append("C26 b c 47n\nR34 c vr 1meg\nQ1 vcc c e QNPN\nR15 e 0 10k\nR14 e z 1k\nC11 z out 1u\nR12 out 0 100k\n")
    s.append(".model QNPN NPN(IS=1e-14 BF=300 VAF=100 CJE=8p CJC=3p RB=50)\n")
    s.append(".ac dec 40 20 20k\n.control\nrun\nwrdata %s vdb(out)\n.endc\n.end\n" % outfile)
    return "".join(s)


def run(w, wl, **kw):
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "o.txt")
        cir = os.path.join(d, "g.cir")
        open(cir, "w").write(netlist(w, wl, out, **kw))
        subprocess.run(["ngspice", "-b", cir], check=True, capture_output=True)
        a = np.loadtxt(out)
        return a[:, 0], a[:, 1]


def main():
    real = "--real" in sys.argv
    export = "--export" in sys.argv
    plot = sys.argv[sys.argv.index("--plot") + 1] if "--plot" in sys.argv else None
    rows = []
    worst = 0.0
    curves = []
    for name, w, wl in SETTINGS:
        f, db = run(w, wl)
        ana = 20 * np.log10(np.abs(m.H_total(f, w, wl)))
        e = np.abs(db - ana).max()
        worst = max(worst, e)
        line = "%-10s 4558 3 MHz / TL022 0.5 MHz: max |ngspice - analytic| = %.4f dB" % (name, e)
        if real:
            fr, dbr = run(w, wl, gbw_band=1e13, gbw_gyr=1e13)
            ai = 20 * np.log10(np.abs(m.H_total(fr, w, wl, ideal=True)))
            line += ";  ideal op-amps vs ideal analytic: %.4f dB; op-amp GBW effect up to %.2f dB" % (
                np.abs(dbr - ai).max(), np.abs(db - dbr).max())
        print(line)
        curves.append((name, f, db))
        for fi, di in zip(f, db):
            rows.append("%s %.6f %.6f\n" % (" ".join("%.6f" % x for x in list(w) + [wl]), fi, di))
    print("worst: %.4f dB" % worst)
    if export:
        path = os.path.join(os.path.dirname(__file__), "..", "..", "Tests", "geq7-ngspice-ref.txt")
        with open(path, "w") as fh:
            fh.write("# ngspice AC reference, GEQ-7 whole pedal (tools/geq7/geq7_ngspice.py --export), 4558 3 MHz / TL022 0.5 MHz single-pole op-amps,\n"
                     "# 2SC1815 follower. Columns: wiper100 200 400 800 1k6 3k2 6k4 wiperLevel f |H|dB\n")
            fh.writelines(rows)
        print("wrote", os.path.normpath(path), len(rows), "rows")
    if plot:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        fig, ax = plt.subplots(figsize=(11, 6))
        for name, f, db in curves[:9]:
            ax.semilogx(f, db, label=name)
        ax.set_xlabel("Hz"); ax.set_ylabel("dB"); ax.grid(True, which="both", alpha=0.3)
        ax.set_title("juicyboi GEQ-7 (GE-7-style circuit), ngspice AC")
        ax.legend(ncol=3, fontsize=8)
        fig.savefig(plot, dpi=110, bbox_inches="tight")
        print("wrote", plot)


if __name__ == "__main__":
    main()
