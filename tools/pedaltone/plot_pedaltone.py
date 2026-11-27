#!/usr/bin/env python3
"""Validate the Rev3.1 component-level Boost post-clip network against ngspice.

usage: plot_pedaltone.py <juicyboi_pedal_compare> <outdir> [fs] [--export Tests/pedal-tone-ngspice-ref.txt]

1) AC: {od808,sd1}-post.cir at Tone 0/2.5/5/7.5/10 (Level knob at 0 dB), tone op-amp ideal
   and as the 4558 single pole (3 MHz, 100 dB), vs the model's digital response at fs
   (default 192 kHz = 4x of 48 kHz). Tone stage (clip -> tone op-amp out) and whole post
   network (clip -> pedal output, real BJT follower, no makeup).
2) Transient: {od808,sd1}-pedal.cir (clipper + post network) vs the whole C++ Boost stage
   (divided by its fixed makeup) at Tone 5, Drive 0/5/10 x 0.05/0.3/1.0 V, 1 kHz from rest.
Writes pedal-tone-od.png, pedal-tone-sd.png, pedal-full-od.png, pedal-full-sd.png.
"""
import os, re, subprocess, sys, tempfile
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
ARGS = sys.argv[1:]
EXPORT = None
if "--export" in ARGS:
    i = ARGS.index("--export"); EXPORT = ARGS[i + 1]; del ARGS[i:i + 2]
DUMP, OUT = ARGS[0], ARGS[1]
FS = float(ARGS[2]) if len(ARGS) > 2 else 192000.0
TONES = [0.0, 0.25, 0.5, 0.75, 1.0]
LEV = 1.0 / 10 ** (12 / 20)  # Level knob 0 dB -> wiper ratio
PEDALS = [("od", "od808", "OD-808 (TS-808 values)", "Od808Stage.h"),
          ("sd", "sd1", "SD-1 topology (Boutique-mod values)", "Sd1Stage.h")]


def makeup(hdr):
    return float(re.search(r"makeup = ([0-9.]+)", open(os.path.join(ROOT, "Source", "dsp", hdr)).read()).group(1))


def taper(ped, x):
    if ped == "sd":
        return x
    half = lambda u: (10 ** (1.5 * u) - 1) / (10 ** 1.5 - 1)
    return 0.5 * half(2 * x) if x < 0.5 else 1 - 0.5 * half(2 * (1 - x))


def run_spice(path, params, work):
    src = open(path).read()
    src = re.sub(r"^\.param .*$", ".param " + " ".join(f"{k}={v}" for k, v in params.items()), src, count=1, flags=re.M)
    src = re.sub(r"wrdata \S+", "wrdata spice.txt", src)
    if "drive" in params:  # keep the rdrive line (second .param) intact
        pass
    p = os.path.join(work, "run.cir"); open(p, "w").write(src)
    subprocess.run(["ngspice", "-b", p], cwd=work, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return np.loadtxt(os.path.join(work, "spice.txt"))


def ac(ped, net, tone, gbw, a0, work):
    d = run_spice(os.path.join(HERE, f"{net}-post.cir"), dict(tau=taper(ped, tone), lev=LEV, gbw=gbw, a0=a0), work)
    return d[:, 0], d[:, 1], d[:, 3]


def model_resp(ped, tone):
    out = subprocess.run([DUMP, "resp", ped, str(tone), "1.0", str(FS)], check=True, capture_output=True, text=True).stdout
    d = np.array([[float(v) for v in l.split()] for l in out.strip().splitlines()])
    return d[:, 0], 20 * np.log10(d[:, 1]), 20 * np.log10(d[:, 3])


def main():
    os.makedirs(OUT, exist_ok=True)
    rows, summary = [], []
    with tempfile.TemporaryDirectory() as work:
        for ped, net, title, hdr in PEDALS:
            fig, (a1, a2, a3) = plt.subplots(1, 3, figsize=(17, 5))
            wT = wP = wOp = 0.0
            for k, tone in enumerate(TONES):
                f, st, sp = ac(ped, net, tone, 1e13, 1e9, work)
                _, st4, sp4 = ac(ped, net, tone, 3e6, 1e5, work)
                fm, mt, mp = model_resp(ped, tone)
                mti, mpi = np.interp(np.log(f), np.log(fm), mt), np.interp(np.log(f), np.log(fm), mp)
                band = (f >= 20) & (f <= 20000)
                eT = np.max(np.abs(mti - st)[band]); eP = np.max(np.abs(mpi - sp)[band])
                eOp = np.max(np.abs(st4 - st)[band])
                wT, wP, wOp = max(wT, eT), max(wP, eP), max(wOp, eOp)
                summary.append((ped, tone, eT, eP, eOp))
                for i in range(len(f)):
                    rows.append((ped, tone, f[i], st[i], sp[i]))
                c = f"C{k}"
                a1.semilogx(f, st, c, lw=2.4, alpha=0.4, label=f"ngspice Tone {10*tone:g}")
                a1.semilogx(fm, mt, c + "--", lw=1, label=f"model (Δ≤{eT:.3f} dB)")
                a2.semilogx(f, sp, c, lw=2.4, alpha=0.4)
                a2.semilogx(fm, mp, c + "--", lw=1, label=f"Tone {10*tone:g}: Δ≤{eP:.3f} dB")
                a3.semilogx(f[band], (mti - st)[band], c, label=f"tone stage, Tone {10*tone:g}")
                a3.semilogx(f[band], (mpi - sp)[band], c + ":", label=f"post network")
            a1.set_title(f"{title}: tone stage (clip out → tone op-amp out)"); a1.set_ylim(-45, 10)
            a2.set_title("whole post network → pedal out (Level knob 0 dB, no makeup)"); a2.set_ylim(-60, -5)
            a3.set_title(f"model − ngspice [dB] @ fs={FS/1000:g} kHz"); a3.set_ylim(-0.6, 0.6)
            for a in (a1, a2, a3):
                a.set_xlim(10, 40000); a.grid(alpha=0.3, which="both"); a.set_xlabel("Hz"); a.legend(fontsize=7)
            fig.suptitle(f"{title} — Rev3.1 post-clip network vs ngspice: tone worst {wT:.3f} dB, post worst {wP:.3f} dB "
                         f"(20 Hz–20 kHz); 4558 GBW vs ideal op-amp: ≤{wOp:.3f} dB")
            fig.tight_layout(); fig.savefig(os.path.join(OUT, f"pedal-tone-{ped}.png"), dpi=100); plt.close(fig)

        # transient: whole pedal
        tsum = []
        for ped, net, title, hdr in PEDALS:
            mk = makeup(hdr)
            fig, axes = plt.subplots(3, 1, figsize=(12, 10))
            for r, drive in enumerate([0.0, 0.5, 1.0]):
                for c, amp in enumerate([0.05, 0.3, 1.0]):
                    d = run_spice(os.path.join(HERE, f"{net}-pedal.cir"),
                                  dict(drive=drive, amp=amp, freq="1k", tstop="30m", tmax=f"{1/(FS*8):.6e}",
                                       tau=taper(ped, 0.5), lev=LEV, gbw=3e6, a0=1e5), work)
                    ts, vs = d[:, 0], d[:, 3]
                    path = os.path.join(work, "m.f32")
                    subprocess.run([DUMP, "sine", ped, str(drive), "0.5", "1.0", str(FS), "0.03", str(amp), "1000", path], check=True)
                    vm = np.fromfile(path, dtype=np.float32).astype(float) / mk
                    tm = np.arange(len(vm)) / FS
                    vsi = np.interp(tm, ts, vs)
                    peak = np.max(np.abs(vsi)); per = int(FS / 1000)
                    relMax = np.max(np.abs(vm - vsi)) / peak
                    relRms = np.sqrt(np.mean((vm - vsi) ** 2)) / np.sqrt(np.mean(vsi ** 2))
                    hs = np.abs(np.fft.rfft(vsi[-10 * per:]))[10:100:10]; hm = np.abs(np.fft.rfft(vm[-10 * per:]))[10:100:10]
                    mask = hs > hs[0] * 1e-3
                    hErr = np.max(np.abs(20 * np.log10(hm[mask] / hs[mask])))
                    thd = 20 * np.log10(np.sqrt(np.sum(hs[1:] ** 2)) / hs[0])
                    tsum.append((ped, drive, amp, peak, relMax, relRms, hErr, thd))
                    sel = tm >= 0.028
                    axes[r].plot((tm[sel] - 0.028) * 1e3, vsi[sel], f"C{c}", lw=2.4, alpha=0.4, label=f"ngspice {amp:g} V")
                    axes[r].plot((tm[sel] - 0.028) * 1e3, vm[sel], f"C{c}--", lw=1, label=f"model (rms err {100*relRms:.2f}%)")
                axes[r].set_title(f"{title}: Drive {10*drive:g}, Tone 5, Level 0 dB — last 2 ms of a 30 ms 1 kHz burst (pedal out, no makeup)")
                axes[r].grid(alpha=0.3); axes[r].legend(fontsize=7, ncol=3); axes[r].set_ylabel("V")
            fig.tight_layout(); fig.savefig(os.path.join(OUT, f"pedal-full-{ped}.png"), dpi=100); plt.close(fig)

    if EXPORT:
        with open(EXPORT, "w") as fo:
            fo.write("# ngspice AC reference (tools/pedaltone/*-post.cir, ideal tone op-amp, real BJT follower).\n")
            fo.write("# Level knob 0 dB (wiper 10^(-12/20)). Columns: pedal tone01 f |Htone|dB |Hpost|dB (no makeup)\n")
            for r in rows:
                fo.write(f"{r[0]} {r[1]:g} {r[2]:.6f} {r[3]:.6f} {r[4]:.6f}\n")
        print("wrote", EXPORT)
    print(f"AC (fs {FS:g}): pedal tone  toneErr[dB] postErr[dB] 4558-vs-ideal[dB]")
    for s in summary:
        print(f"  {s[0]} {10*s[1]:4.1f}  {s[2]:.4f}  {s[3]:.4f}  {s[4]:.4f}")
    print("transient: pedal drive amp peak[V] maxErr% rmsErr% harmΔdB THD[dB]")
    for s in tsum:
        print(f"  {s[0]} {s[1]:.1f} {s[2]:.2f} {s[3]:.3f} {100*s[4]:.2f} {100*s[5]:.2f} {s[6]:.2f} {s[7]:.1f}")


if __name__ == "__main__":
    main()
