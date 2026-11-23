#!/usr/bin/env python3
"""Validate the Rev3 solved triode stages (Source/dsp/Triode.h) against ngspice.

usage: plot_triode.py <juicyboi_triode_dump> <outdir> [fs] [--write-cir] [--export Tests/triode-ngspice-ref.txt]

For each case it writes a netlist (same parts as Source/dsp/TriodeParts.h, same
Koren / Cohen-Helie laws as B-sources), runs ngspice (.tran from the DC
operating point, trapezoidal), runs the C++ stage at fs (default 192 kHz = 4x
of 48 kHz) and compares plate voltage, grid-cathode voltage (bias shift) and
harmonics. --write-cir stores the netlists in tools/triode/.
"""
import os, subprocess, sys, tempfile
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
ARGS = list(sys.argv[1:])
WRITE = "--write-cir" in ARGS
if WRITE: ARGS.remove("--write-cir")
EXPORT = None
if "--export" in ARGS:
    i = ARGS.index("--export"); EXPORT = ARGS[i + 1]; del ARGS[i:i + 2]
DUMP, OUT = ARGS[0], ARGS[1]
FS = float(ARGS[2]) if len(ARGS) > 2 else 192000.0

# name: (B, Ra, Rk, Ck, Cc, Rs, Rg, Rgs) -- keep in sync with TriodeParts.h
PARTS = {
    "rV1A": (395.0, 220e3, 1.8e3, 1e-6, 0.0, 10e3, 1e6, 1e3),
    "rV2A": (391.0, 100e3, 1.8e3, 1e-6, 20e-9, 48e3, 1.3e6, 2.9e6),
    "rV2B": (391.0, 100e3, 39e3, 0.0, 20e-9, 508e3, 1e6, 1e3),
    "rV3A": (409.0, 220e3, 1.8e3, 1e-6, 20e-9, 100e3, 330e3, 220e3),
    "fV1A": (290.0, 220e3, 1.8e3, 1e-6, 0.0, 10e3, 1e6, 68e3),
    "fV1B": (290.0, 100e3, 1.8e3, 1e-6, 22e-9, 518e3, 500e3, 720e3),
    "fV2A": (290.0, 100e3, 39e3, 0.0, 22e-9, 508e3, 1e6, 1e3),
    "fV2B": (320.0, 220e3, 1.8e3, 1e-6, 22e-9, 320e3, 330e3, 1e3),
    "fV5B": (320.0, 220e3, 2.2e3, 0.0, 22e-9, 1.05e6, 100e3, 1e3),
    "fV5A": (320.0, 100e3, 2.2e3, 0.0, 22e-9, 1.12e6, 1e6, 1e6),
}

# (stage, amp V, freq, seconds, burstEnd, amp2)
CASES = [
    ("rV1A", 0.5, 1000.0, 0.03, 0, 0), ("rV1A", 2.0, 1000.0, 0.03, 0, 0), ("rV1A", 5.0, 1000.0, 0.03, 0, 0),
    ("rV2B", 2.0, 1000.0, 0.03, 0, 0), ("rV2B", 8.0, 1000.0, 0.03, 0, 0), ("rV2B", 30.0, 1000.0, 0.03, 0, 0),
    ("rV3A", 1.0, 1000.0, 0.03, 0, 0), ("rV3A", 5.0, 1000.0, 0.03, 0, 0), ("rV3A", 30.0, 1000.0, 0.03, 0, 0),
    ("fV2A", 8.0, 1000.0, 0.03, 0, 0), ("fV5A", 30.0, 1000.0, 0.03, 0, 0), ("fV5A", 100.0, 1000.0, 0.03, 0, 0),
    ("rV2A", 5.0, 1000.0, 0.03, 0, 0), ("fV1B", 10.0, 1000.0, 0.03, 0, 0),
    # blocking / bias shift: a loud 100 Hz burst, then a quiet tone: watch Vgk recover
    ("rV3A", 40.0, 100.0, 0.40, 0.10, 0.5),
    ("fV5A", 150.0, 100.0, 0.40, 0.10, 2.0),
]
EXPORT_CASES = [("rV1A", 2.0), ("rV2B", 8.0), ("rV3A", 30.0), ("fV5A", 30.0)]


def netlist(stage, amp, freq, secs, burst, amp2, outfile):
    B, Ra, Rk, Ck, Cc, Rs, Rg, Rgs = PARTS[stage]
    src = (f"BIN in 0 V = (time < {burst} ? {amp} : {amp2}) * sin(2*pi*{freq}*time)" if burst > 0
           else f"VIN in 0 SIN(0 {amp} {freq})")
    coup = (f"RS in a {Rs}\nCC a n {Cc}" if Cc > 0 else f"RS in n {Rs}")
    cath = f"RK k 0 {Rk}" + (f"\nCK k 0 {Ck}" if Ck > 0 else "")
    step = 1.0 / FS / 4.0
    return f"""* juicyboi Rev3 triode stage {stage} (Source/dsp/TriodeParts.h), {amp} V {freq} Hz
* 12AX7: Koren plate law (mu 100, ex 1.4, kg1 1060, kp 600, kvb 300),
*        Cohen-Helie grid current (Gg 6.062e-4, xi 1.354, Cg 13.9, Ig0 3.917e-8)
{src}
{coup}
RG n 0 {Rg}
RGS n g {Rgs}
VB vb 0 {B}
RA vb p {Ra}
{cath}
BP p k I = 2*pwr(max(V(p,k)/600*ln(1+exp(600*(0.01+V(g,k)/sqrt(300+V(p,k)*V(p,k))))),1e-12),1.4)/1060
BG g k I = 6.062e-4*pwr(ln(1+exp(13.9*V(g,k)))/13.9,1.354)+3.917e-8
.options method=trap reltol=1e-6 abstol=1e-13 vntol=1e-7
.tran {step} {secs} 0 {step}
.control
run
wrdata {outfile} v(in) v(p) v(g,k)
quit
.endc
.end
"""


def run_spice(stage, amp, freq, secs, burst, amp2, tmp):
    out = os.path.join(tmp, "sp.txt")
    cir = netlist(stage, amp, freq, secs, burst, amp2, out)
    path = os.path.join(tmp, "c.cir")
    open(path, "w").write(cir)
    subprocess.run(["ngspice", "-b", path], check=True, capture_output=True)
    d = np.loadtxt(out)
    return d[:, 0], d[:, 3], d[:, 5], cir   # t, vp, vgk


def run_cpp(stage, amp, freq, secs, burst, amp2, tmp):
    out = os.path.join(tmp, "cpp.txt")
    args = [DUMP, stage, str(amp), str(freq), str(FS), str(secs), out]
    if burst > 0: args += [str(burst), str(amp2)]
    r = subprocess.run(args, check=True, capture_output=True, text=True)
    d = np.loadtxt(out)
    it = r.stderr.strip()
    return d[:, 0], d[:, 2], d[:, 3], it


def harmonics(t, y, f0, n=9):
    # last 10 cycles
    T = 10.0 / f0
    m = t >= t[-1] - T
    tt, yy = t[m], y[m] - np.mean(y[m])
    out = []
    for h in range(1, n + 1):
        c = np.exp(-2j * np.pi * h * f0 * tt)
        out.append(abs(np.trapezoid(yy * c, tt)) * 2 / (tt[-1] - tt[0]))
    return np.array(out)


def main():
    os.makedirs(OUT, exist_ok=True)
    rows, export = [], []
    figs = {}
    with tempfile.TemporaryDirectory() as tmp:
        for (stage, amp, freq, secs, burst, amp2) in CASES:
            ts, vps, vgks, cir = run_spice(stage, amp, freq, secs, burst, amp2, tmp)
            tc, vpc, vgkc, it = run_cpp(stage, amp, freq, secs, burst, amp2, tmp)
            sp = np.interp(tc, ts, vps)
            sg = np.interp(tc, ts, vgks)
            vq = vpc[0]
            swing = max(np.max(np.abs(sp - vq)), 1e-9)
            err = vpc - sp
            rms = np.sqrt(np.mean(err ** 2)) / np.sqrt(np.mean((sp - np.mean(sp)) ** 2)) * 100
            worst = np.max(np.abs(err)) / swing * 100
            gerr = np.max(np.abs(vgkc - sg))
            hs, hc = harmonics(tc, sp, freq), harmonics(tc, vpc, freq)
            hd = [20 * np.log10(hc[i] / hc[0]) - 20 * np.log10(hs[i] / hs[0]) for i in range(1, 9)
                  if 20 * np.log10(hs[i] / hs[0]) > -60]
            hdw = max([abs(x) for x in hd], default=0.0)
            thd = 10 * np.log10(np.sum(hs[1:] ** 2) / hs[0] ** 2)
            rows.append((stage, amp, freq, burst, rms, worst, gerr, hdw, thd, it))
            print(f"{stage:5s} {amp:6.1f} V {freq:5.0f} Hz burst {burst:4.2f}: rms err {rms:6.3f} %  worst {worst:6.3f} % of swing  "
                  f"Vgk worst {gerr*1e3:7.2f} mV  harm worst {hdw:5.2f} dB  THD {thd:6.1f} dB  [{it}]", flush=True)
            figs.setdefault(stage, []).append((amp, freq, burst, tc, vpc, sp, vgkc, sg))
            if WRITE and (stage, amp) in [("rV1A", 2.0), ("rV2B", 8.0), ("rV3A", 40.0), ("fV5A", 30.0)]:
                name = f"{stage}-{'burst' if burst else int(amp)}.cir"
                open(os.path.join(HERE, name), "w").write(cir.replace(os.path.join(tmp, "sp.txt"), "out.txt"))
            if (stage, amp) in EXPORT_CASES and burst == 0:
                idx = np.arange(0, len(tc), 8)
                for i in idx:
                    export.append(f"{stage} {amp} {tc[i]:.9e} {sp[i]:.6f} {sg[i]:.6f}")
    if EXPORT:
        open(EXPORT, "w").write("# stage amp t vplate_ngspice vgk_ngspice (tools/triode/plot_triode.py --export)\n" + "\n".join(export) + "\n")

    # Plot 1: steady-state waveforms; Plot 2: blocking recovery
    stat = [(s, c) for s, cs in figs.items() for c in cs if c[2] == 0]
    n = len(stat)
    cols = 3
    fig, axs = plt.subplots((n + cols - 1) // cols, cols, figsize=(15, 3.0 * ((n + cols - 1) // cols)))
    for ax, (s, (amp, freq, burst, tc, vpc, sp, vgkc, sg)) in zip(axs.flat, stat):
        m = tc >= tc[-1] - 3.0 / freq
        ax.plot(tc[m] * 1e3, sp[m], "k", lw=2.5, alpha=0.4, label="ngspice")
        ax.plot(tc[m] * 1e3, vpc[m], "r", lw=0.9, label="juicyboi")
        ax.set_title(f"{s} {amp:g} V {freq:g} Hz: plate V", fontsize=9)
        ax.tick_params(labelsize=7)
    for ax in list(axs.flat)[n:]: ax.axis("off")
    axs.flat[0].legend(fontsize=7)
    fig.suptitle(f"Rev3 triode stages vs ngspice (C++ at {FS/1000:g} kHz)")
    fig.tight_layout(); fig.savefig(os.path.join(OUT, "triode-ngspice.png"), dpi=110)

    bur = [(s, c) for s, cs in figs.items() for c in cs if c[2] > 0]
    fig, axs = plt.subplots(len(bur), 2, figsize=(14, 3.4 * len(bur)))
    for row, (s, (amp, freq, burst, tc, vpc, sp, vgkc, sg)) in zip(np.atleast_2d(axs), bur):
        row[0].plot(tc * 1e3, sp, "k", lw=2.5, alpha=0.4, label="ngspice"); row[0].plot(tc * 1e3, vpc, "r", lw=0.6, label="juicyboi")
        row[0].set_title(f"{s}: {amp:g} V 100 Hz burst until {burst*1e3:g} ms, then quiet: plate V", fontsize=9)
        row[1].plot(tc * 1e3, sg, "k", lw=2.5, alpha=0.4, label="ngspice"); row[1].plot(tc * 1e3, vgkc, "r", lw=0.6, label="juicyboi")
        row[1].set_title(f"{s}: grid-cathode voltage (bias shift and recovery)", fontsize=9)
        row[0].legend(fontsize=7)
    fig.tight_layout(); fig.savefig(os.path.join(OUT, "triode-blocking.png"), dpi=110)


if __name__ == "__main__":
    main()
