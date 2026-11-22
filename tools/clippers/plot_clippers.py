#!/usr/bin/env python3
"""Validate the Rev2.2 solved diode clippers against ngspice.

usage: plot_clippers.py <juicyboi_clipper_dump> <outdir> [fs] [--export Tests/clipper-ngspice-ref.txt]

For each stage (OD-808, SD-1) and each (Drive, input level) case it
  * runs the ngspice netlist in this folder (ideal op-amp, same components),
  * runs the C++ model (input coupling + solved clipper) at fs (default 192 kHz,
    the 4x-oversampled rate at a 48 kHz host),
  * compares the transient waveforms (whole 30 ms run, from rest) and the
    harmonic content (last 10 cycles),
and writes clipper-od.png / clipper-sd.png plus a summary on stdout.
"""
import os, re, subprocess, sys, tempfile
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
ARGS = [a for a in sys.argv[1:]]
EXPORT = None
if "--export" in ARGS:
    i = ARGS.index("--export"); EXPORT = ARGS[i + 1]; del ARGS[i:i + 2]
DUMP = ARGS[0]
OUT = ARGS[1]
FS = float(ARGS[2]) if len(ARGS) > 2 else 192000.0
# ngspice references stored for Tests/ClipperTest.cpp (stage, drive, amp)
EXPORT_CASES = [("od", 0.5, 0.3), ("od", 1.0, 1.0), ("sd", 0.5, 0.3), ("sd", 1.0, 1.0)]
FREQ = 1000.0
TSTOP = 0.030
DRIVES = [0.0, 0.5, 1.0]
AMPS = [0.05, 0.3, 1.0]
NH = 9

STAGES = [("od", "od808-clipper.cir", "OD-808 (TS-808 values)", "clipper-od.png"),
          ("sd", "sd1-clipper.cir", "SD-1 topology (Boutique-mod values)", "clipper-sd.png")]


def spice(netlist, drive, amp, work):
    src = open(os.path.join(HERE, netlist)).read()
    src = re.sub(r"^\.param drive=.*$",
                 f".param drive={drive} amp={amp} freq={FREQ} tstop={TSTOP} tmax={1.0/(FS*8):.6e}",
                 src, count=1, flags=re.M)
    src = re.sub(r"wrdata \S+", "wrdata spice.txt", src)
    path = os.path.join(work, "run.cir")
    open(path, "w").write(src)
    subprocess.run(["ngspice", "-b", path], cwd=work, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    d = np.loadtxt(os.path.join(work, "spice.txt"))
    return d[:, 0], d[:, 3]


def model(stage, drive, amp, work):
    path = os.path.join(work, "model.txt")
    subprocess.run([DUMP, stage, str(drive), str(amp), str(FREQ), str(FS), str(TSTOP), path, "core"],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    d = np.loadtxt(path)
    return d[:, 0], d[:, 2]


def harmonics(y, fs):
    per = int(round(fs / FREQ))
    seg = y[-10 * per:]
    seg = seg - seg.mean()
    spec = np.abs(np.fft.rfft(seg)) * 2 / len(seg)
    return np.array([spec[10 * k] for k in range(1, NH + 1)])


def main():
    os.makedirs(OUT, exist_ok=True)
    summary = []
    exported = {}
    with tempfile.TemporaryDirectory() as work:
        for stage, net, title, png in STAGES:
            fig, axes = plt.subplots(len(DRIVES), 2, figsize=(13, 3.2 * len(DRIVES)))
            worstWave, worstH = 0.0, 0.0
            for r, drive in enumerate(DRIVES):
                axw, axh = axes[r]
                width = 0.8 / len(AMPS)
                for c, amp in enumerate(AMPS):
                    ts, vs = spice(net, drive, amp, work)
                    tm, vm = model(stage, drive, amp, work)
                    vsi = np.interp(tm, ts, vs)
                    if (stage, drive, amp) in EXPORT_CASES:
                        exported[(stage, drive, amp)] = vsi
                    err = np.abs(vm - vsi)
                    peak = np.max(np.abs(vsi))
                    relMax = err.max() / peak
                    relRms = np.sqrt(np.mean((vm - vsi) ** 2)) / np.sqrt(np.mean(vsi ** 2))
                    hs, hm = harmonics(vsi, FS), harmonics(vm, FS)
                    ref = hs[0]
                    # harmonic error (dB) for harmonics above -60 dBc
                    mask = hs > ref * 1e-3
                    hErr = np.max(np.abs(20 * np.log10(hm[mask] / hs[mask])))
                    worstWave = max(worstWave, relMax)
                    worstH = max(worstH, hErr)
                    summary.append((stage, drive, amp, peak, relMax, relRms, hErr,
                                    20 * np.log10(np.sqrt(np.sum(hs[1:] ** 2)) / ref)))
                    sel = tm >= TSTOP - 2.0 / FREQ
                    col = f"C{c}"
                    axw.plot((tm[sel] - tm[sel][0]) * 1e3, vsi[sel], col, lw=2.2, alpha=0.45,
                             label=f"ngspice {amp:g} V")
                    axw.plot((tm[sel] - tm[sel][0]) * 1e3, vm[sel], col + "--", lw=1.0,
                             label=f"model (max err {100*relMax:.2f}%)")
                    k = np.arange(1, NH + 1)
                    axh.bar(k + (c - 1) * width, 20 * np.log10(np.maximum(hs / ref, 1e-7)), width * 0.5,
                            color=col, alpha=0.45, label=f"ngspice {amp:g} V")
                    axh.plot(k + (c - 1) * width, 20 * np.log10(np.maximum(hm / ref, 1e-7)), col + "x",
                             label=f"model (Δ ≤ {hErr:.2f} dB)")
                axw.set_title(f"{title}: Drive {10*drive:g}/10 — last 2 ms of a 30 ms 1 kHz burst")
                axw.set_xlabel("ms"); axw.set_ylabel("op-amp out [V]"); axw.grid(alpha=0.3)
                axw.legend(fontsize=7, ncol=2)
                axh.set_title("harmonics re H1 (last 10 cycles)")
                axh.set_xlabel("harmonic"); axh.set_ylabel("dBc"); axh.set_ylim(-100, 5)
                axh.grid(alpha=0.3); axh.legend(fontsize=7, ncol=2)
            fig.suptitle(f"{title} — solved clipper vs ngspice @ fs={FS/1000:g} kHz: "
                         f"worst waveform err {100*worstWave:.2f}% of peak, worst harmonic Δ {worstH:.2f} dB",
                         fontsize=11)
            fig.tight_layout()
            fig.savefig(os.path.join(OUT, png), dpi=110)
            plt.close(fig)

    if EXPORT:
        with open(EXPORT, "w") as f:
            f.write(f"# ngspice reference (tools/clippers/*.cir), op-amp output [V] on a {FS:g} Hz grid,\n")
            f.write(f"# {FREQ:g} Hz sine from rest, {TSTOP*1e3:g} ms. Columns: one per case. Cases:\n")
            f.write("# " + " ".join(f"{s}:{d:g}:{a:g}" for s, d, a in EXPORT_CASES) + "\n")
            cols = np.column_stack([exported[c] for c in EXPORT_CASES])
            for row in cols:
                f.write(" ".join(f"{v:.7e}" for v in row) + "\n")
        print("wrote", EXPORT)
    print(f"fs = {FS:g} Hz, {FREQ:g} Hz sine from rest, {TSTOP*1e3:g} ms")
    print("stage drive  amp[V]  peak[V]  maxErr%  rmsErr%  harmΔdB  THD[dB]")
    for s in summary:
        print(f"{s[0]:5s} {s[1]:5.2f} {s[2]:7.2f} {s[3]:8.3f} {100*s[4]:8.3f} {100*s[5]:8.3f} {s[6]:8.3f} {s[7]:8.1f}")
    for st in ("od", "sd"):
        rows = [s for s in summary if s[0] == st]
        print(f"{st}: worst max err {100*max(r[4] for r in rows):.3f}% of peak, "
              f"worst rms err {100*max(r[5] for r in rows):.3f}%, worst harmonic Δ {max(r[6] for r in rows):.3f} dB")


if __name__ == "__main__":
    main()
