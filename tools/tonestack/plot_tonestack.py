#!/usr/bin/env python3
"""Plot juicyboi's digital passive-TMB response against the analog formula (and ngspice).

Usage: python3 tools/tonestack/plot_tonestack.py [build-dir] [out-dir]
  build-dir defaults to ./build (needs the juicyboi_tonestack_dump target built)
  out-dir   defaults to ../juicyboi-shots (PNG files tonestack-*.png)
Needs matplotlib. ngspice is optional: when it is on PATH, the SPICE netlists in this
folder (same component values) are run at default knobs and overlaid as dots.
"""
import csv, io, math, os, shutil, subprocess, sys, tempfile
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

here = os.path.dirname(os.path.abspath(__file__))
build = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.getcwd(), "build")
out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.getcwd()), "juicyboi-shots")
os.makedirs(out, exist_ok=True)
dump = None
for cand in ("juicyboi_tonestack_dump", "Release/juicyboi_tonestack_dump", "Debug/juicyboi_tonestack_dump"):
    if os.path.exists(os.path.join(build, cand)):
        dump = os.path.join(build, cand)
if dump is None:
    sys.exit("build the juicyboi_tonestack_dump target first (cmake --build build --target juicyboi_tonestack_dump)")

TITLES = {"red": "Rectifier Red (RF-1F parts)", "orange": "Rectifier Orange (RF-1F parts)",
          "lead": "5150 Lead (EVH 5150 sheet 3 parts)"}
SETTINGS = [((5, 4, 5.5), "defaults B5 M4 T5.5"), ((10, 10, 10), "all 10"), ((0, 0, 0), "all 0"),
            ((10, 0, 10), "scoop B10 M0 T10"), ((5, 10, 5.5), "Mid 10"), ((10, 4, 5.5), "Bass 10")]

def run_dump(which, fs, b, m, t):
    txt = subprocess.check_output([dump, which, str(fs), str(b), str(m), str(t)], text=True)
    rows = list(csv.DictReader(io.StringIO(txt)))
    return [{k: float(v) for k, v in r.items()} for r in rows]

def audio_taper(p, mid=0.15):
    base = ((1 - mid) / mid) ** 2
    return (base ** p - 1) / (base - 1)

def run_spice(which, b, m, t):
    if shutil.which("ngspice") is None:
        return None
    with tempfile.TemporaryDirectory() as d:
        res = os.path.join(d, "out.txt")
        deck = os.path.join(d, "run.cir")
        src = open(os.path.join(here, f"tmb-{which}.cir")).read()
        l = min(max(audio_taper(b / 10), 0.001), 0.999)
        src = src.replace(".param t=0.55 m=0.4 l=0.150",
                          f".param t={min(max(t/10,0.001),0.999)} m={min(max(m/10,0.001),0.999)} l={l}")
        src = src.replace(".end", f".control\nrun\nwrdata {res} vdb(out)\n.endc\n.end")
        open(deck, "w").write(src)
        subprocess.run(["ngspice", "-b", deck], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        pts = [tuple(map(float, line.split()[:2])) for line in open(res) if line.strip()]
        return pts

summary = []
for which in ("red", "orange", "lead"):
    makeup = None
    fig, axes = plt.subplots(1, 2, figsize=(15, 6))
    for (b, m, t), label in SETTINGS:
        rows = run_dump(which, 48000, b, m, t)
        hz = [r["hz"] for r in rows]
        line, = axes[0].semilogx(hz, [r["analog_db"] for r in rows], lw=2.2, alpha=0.45, label=f"{label} analog")
        axes[0].semilogx(hz, [r["measured_db"] for r in rows], "--", color=line.get_color(), lw=1.1,
                         label=f"{label} digital (measured, 48 kHz)")
        worst = max(abs(r["measured_db"] - r["analog_db"]) for r in rows if r["hz"] <= 8000)
        summary.append(f"{which:6s} {label:22s} 48k worst |digital-analog| <= 8 kHz: {worst:.3f} dB")
        if (b, m, t) == (5, 4, 5.5):
            sp = run_spice(which, b, m, t)
            if sp:
                # SPICE has no makeup: align both at exactly 1 kHz (log-interpolated)
                import numpy as np
                lf = np.log([r["hz"] for r in rows]); an = np.array([r["analog_db"] for r in rows])
                off = float(np.interp(math.log(1000.0), lf, an))
                base_sp = float(np.interp(math.log(1000.0), np.log([p[0] for p in sp]), [p[1] for p in sp]))
                axes[0].semilogx([p[0] for p in sp[::3]], [p[1] - base_sp + off for p in sp[::3]], "k.", ms=3,
                                 label="defaults ngspice (same netlist values)")
                worst_sp = max(abs((p[1] - base_sp + off) - np.interp(math.log(p[0]), lf, an))
                               for p in sp if rows[0]["hz"] <= p[0] <= rows[-1]["hz"])
                summary.append(f"{which:6s} defaults: ngspice vs closed form worst {worst_sp:.4f} dB")
    axes[0].set_title(f"juicyboi Rev2 TMB: {TITLES[which]} (incl. makeup)")
    axes[0].set_xlabel("Hz"); axes[0].set_ylabel("dB"); axes[0].grid(True, which="both", alpha=0.3)
    axes[0].legend(fontsize=6.5, ncol=2); axes[0].set_xlim(20, 20000)
    # right: digital - analog error per sample rate at defaults and extremes
    for fs in (44100, 48000, 96000, 192000):
        for (b, m, t), label in (SETTINGS[0], SETTINGS[3]):
            rows = run_dump(which, fs, b, m, t)
            axes[1].semilogx([r["hz"] for r in rows], [r["measured_db"] - r["analog_db"] for r in rows],
                             label=f"{fs/1000:g} kHz, {label}")
    axes[1].axhline(0, color="k", lw=0.6)
    axes[1].set_title("digital (measured sines) minus analog H(jw); prewarp 3 kHz")
    axes[1].set_xlabel("Hz"); axes[1].set_ylabel("dB error"); axes[1].grid(True, which="both", alpha=0.3)
    axes[1].legend(fontsize=7); axes[1].set_xlim(20, 20000); axes[1].set_ylim(-1, 1)
    fig.tight_layout()
    path = os.path.join(out, f"tonestack-{which}.png")
    fig.savefig(path, dpi=110); plt.close(fig)
    summary.append(f"wrote {path}")

# Interaction plot: Mid sweep changes the bass response (Red)
fig, ax = plt.subplots(figsize=(9, 5.5))
for mid in (0, 2.5, 5, 7.5, 10):
    for bass, ls in ((0, ":"), (10, "-")):
        rows = run_dump("red", 48000, bass, mid, 5.5)
        ax.semilogx([r["hz"] for r in rows], [r["measured_db"] for r in rows], ls, label=f"Mid {mid}, Bass {bass}")
ax.set_title("Red: knobs interact. Bass 0 (dotted) vs 10 (solid) at five Mid settings, Treble 5.5")
ax.set_xlabel("Hz"); ax.set_ylabel("dB (incl. makeup)"); ax.grid(True, which="both", alpha=0.3)
ax.legend(fontsize=7, ncol=2); ax.set_xlim(20, 20000)
fig.tight_layout(); path = os.path.join(out, "tonestack-interaction.png"); fig.savefig(path, dpi=110)
summary.append(f"wrote {path}")
print("\n".join(summary))
