#!/usr/bin/env python3
"""Rev3.1 vs Rev3 Boost comparison (OD-808 / SD-1 stage alone, 192 kHz = 4x of 48 kHz).

usage: compare_rev3.py <rev3.1 juicyboi_pedal_compare> <rev3 pedal_compare> <outdir>
Build the Rev3 binary from a Rev3 checkout (e.g. git worktree at 926ef71):
  g++ -O2 -std=c++17 -I<rev3>/Source Tests/PedalCompare.cpp <rev3>/Source/dsp/Od808Stage.cpp \
      <rev3>/Source/dsp/Sd1Stage.cpp -o pedal_compare_rev3
Measures: small-signal response at Tone 0/2.5/5/7.5/10 (Drive 2, Level 0 dB), THD/H2/H3
at 220 Hz 0.3 V for Drive 0/2/5/10 (Tone 5), level of the fixture guitar mix, CPU.
Writes pedal-tone-vs-rev3.png.
"""
import os, subprocess, sys, tempfile
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

NEW, OLD, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
FS = 192000.0
TONES = [0.0, 0.25, 0.5, 0.75, 1.0]
FREQS = np.geomspace(30, 16000, 40)
PEDALS = [("od", "OD-808"), ("sd", "SD-1")]


def render(binary, args, work):
    p = os.path.join(work, "y.f32")
    subprocess.run([binary] + [str(a) for a in args] + [p], check=True)
    return np.fromfile(p, dtype=np.float32).astype(float)


def gain_db(binary, ped, tone, f, work, drive=0.2, amp=1e-3):
    secs = max(0.25, 40.0 / f)
    y = render(binary, ["sine", ped, drive, tone, 1.0, FS, secs, amp, f], work)
    n = int(round(int(0.1 * FS) * f / FS)) or 1
    m = int(round(n * FS / f)); seg = y[-m:]; t = np.arange(len(y))[-m:] / FS
    re = np.sum(seg * np.sin(2 * np.pi * f * t)); im = np.sum(seg * np.cos(2 * np.pi * f * t))
    return 20 * np.log10(2 * np.hypot(re, im) / m / amp)


def thd(binary, ped, drive, work, f=220.0, amp=0.3):
    y = render(binary, ["sine", ped, drive, 0.5, 1.0, FS, 0.5, amp, f], work)
    per = int(round(FS / f)); seg = y[-20 * per:]; seg = seg - seg.mean()
    sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg))))
    h = np.array([sp[20 * k - 2:20 * k + 3].max() for k in range(1, 16)])
    return 100 * np.sqrt(np.sum(h[1:] ** 2)) / h[0], 20 * np.log10(h[1] / h[0]), 20 * np.log10(h[2] / h[0]), \
        20 * np.log10(np.sqrt(np.mean(seg ** 2)))


def gtr(binary, ped, drive, tone, work):
    y = render(binary, ["gtr", ped, drive, tone, 1.0, FS, 1.0], work)[int(FS / 2):]
    return 10 * np.log10(np.mean(y ** 2))


def cpu(binary, ped):
    return np.median([float(subprocess.run([binary, "cpu", ped, str(FS)], capture_output=True, text=True, check=True).stdout)
                      for _ in range(5)])


def main():
    os.makedirs(OUT, exist_ok=True)
    fig, axes = plt.subplots(2, 3, figsize=(19, 10))
    with tempfile.TemporaryDirectory() as work:
        for r, (ped, title) in enumerate(PEDALS):
            ax = axes[r][0]; axd = axes[r][1]; axt = axes[r][2]
            print(f"== {title}: small-signal response (Drive 2, Level 0 dB), dB re input")
            print("  Tone   " + " ".join(f"{f:7.0f}" for f in FREQS[::6]))
            for k, tone in enumerate(TONES):
                gn = np.array([gain_db(NEW, ped, tone, f, work) for f in FREQS])
                go = np.array([gain_db(OLD, ped, tone, f, work) for f in FREQS])
                ax.semilogx(FREQS, gn, f"C{k}", lw=2, label=f"Rev3.1 Tone {10*tone:g}")
                ax.semilogx(FREQS, go, f"C{k}--", lw=1, label=f"Rev3 Tone {10*tone:g}")
                axd.semilogx(FREQS, gn - go, f"C{k}", label=f"Tone {10*tone:g}")
                print(f"  {10*tone:4.1f} new " + " ".join(f"{v:7.1f}" for v in gn[::6]))
                print(f"  {10*tone:4.1f} old " + " ".join(f"{v:7.1f}" for v in go[::6]))
            ax.set_title(f"{title}: small-signal response, Drive 2, Level 0 dB (solid Rev3.1, dashed Rev3)")
            ax.set_ylabel("dB re input"); ax.grid(alpha=0.3, which="both"); ax.legend(fontsize=6, ncol=2)
            axd.set_title(f"{title}: Rev3.1 − Rev3 [dB]"); axd.grid(alpha=0.3, which="both"); axd.legend(fontsize=7)
            drives = [0.0, 0.2, 0.5, 1.0]
            tn = [thd(NEW, ped, d, work) for d in drives]; to = [thd(OLD, ped, d, work) for d in drives]
            print(f"== {title}: 220 Hz 0.3 V, Tone 5: THD% / H2 dBc / H3 dBc / out dBV  (Rev3 -> Rev3.1)")
            for d, a, b in zip(drives, to, tn):
                print(f"  Drive {10*d:4.0f}: THD {a[0]:5.1f} -> {b[0]:5.1f} %  H2 {a[1]:6.1f} -> {b[1]:6.1f}  H3 {a[2]:6.1f} -> {b[2]:6.1f}  out {a[3]:6.1f} -> {b[3]:6.1f}")
            x = np.arange(len(drives))
            axt.bar(x - 0.2, [a[0] for a in to], 0.4, label="Rev3", alpha=0.6)
            axt.bar(x + 0.2, [b[0] for b in tn], 0.4, label="Rev3.1", alpha=0.9)
            axt.set_xticks(x); axt.set_xticklabels([f"Drive {10*d:g}" for d in drives])
            axt.set_title(f"{title}: THD at 220 Hz, 0.3 V in, Tone 5"); axt.set_ylabel("THD %"); axt.legend(); axt.grid(alpha=0.3)
            print(f"== {title}: fixture guitar mix level, Rev3.1 − Rev3 [dB] (rows Drive 0/2/5/10, cols Tone 0/2.5/5/7.5/10)")
            for d in drives:
                print(f"  Drive {10*d:4.0f}: " + " ".join(f"{gtr(NEW, ped, d, t, work) - gtr(OLD, ped, d, t, work):+6.2f}" for t in TONES))
            cn, co = cpu(NEW, ped), cpu(OLD, ped)
            print(f"== {title}: CPU {co:.1f} -> {cn:.1f} ns per stereo frame at 192 kHz ({100*(cn/co-1):+.0f}%)")
    for a in axes.flat[:]:
        if a.get_xscale() == "log": a.set_xlabel("Hz")
    fig.suptitle("juicyboi Rev3.1 component-level Boost tone/level vs Rev3 (behavioural): stage alone, 192 kHz")
    fig.tight_layout(); fig.savefig(os.path.join(OUT, "pedal-tone-vs-rev3.png"), dpi=100)
    print("wrote", os.path.join(OUT, "pedal-tone-vs-rev3.png"))


if __name__ == "__main__":
    main()
