# Passive TMB tone stacks (Rev2)

Source: `Source/dsp/ToneStack.h/.cpp` (solver), part sets in
`Source/dsp/ChannelVoice.h` (`redTmb`, `orangeTmb`) and `Source/dsp/Lead5150.h`
(`leadTmb`). Tests: `Tests/ToneStackTest.cpp`. Cross-check: `tools/tonestack/`.

## Network

```
 in ──Ct──────────── A (Treble pot top)
  │                   │ (1-t)·Rt
  │                   ├──── out (Treble wiper, unloaded)
  │                   │ t·Rt
  │        ┌──Cb───── B (Treble pot bottom)
  └──Rs─── X          │ l·Rb   (Bass, rheostat)
           │          M (Mid pot top)
           │          │ (1-m)·Rm
           └──Cm───── W (Mid wiper)
                      │ m·Rm
                     GND
```

Ideal voltage source at `in`, no load on `out`. Pot fractions: `t` Treble
(linear), `m` Mid (linear), `l` Bass (audio law, 15 % at mid-rotation), each
clamped to 0.1 %…99.9 % (pot end resistance).

H(s) = (b1 s + b2 s² + b3 s³) / (1 + a1 s + a2 s² + a3 s³), coefficients in
`tmb::analog` (standard Yeh/Smith TMB form, written for R1 = Rt, R2 = Rb, R3 = Rm,
R4 = Rs, C1 = Ct, C2 = Cb, C3 = Cm). `ToneStackTest` checks it against an
independent nodal solve of the drawing above (worst relative error 5e-7), and
`plot_tonestack.py` against ngspice on the netlists (≤ 0.007 dB at defaults,
grid-interpolation limited).

## Part values and sources

| Part | Rectifier Red | Rectifier Orange | 5150 Lead |
|------|---------------|------------------|-----------|
| Sheet | Mesa/Boogie Dual Rectifier preamp **RF-1F** (6-93), Red stack | RF-1F, Orange stack | Peavey **EVH 5150**, sheet 3 (Tone Stack/PI) |
| Rt | TRBL 250K | TRBL 250K | VR5 HIGH 250K, "B" (linear) |
| Ct | C4 680 pF | C5 500 pF | C14 470 pF ∥ C60 100 pF = 570 pF |
| Rs | R273 47K | R274 47K | R28 47K |
| Cb | C23 .02 µF | C25 .02 µF | C18 .022 µF |
| Cm | C24 .02 µF | C26 .02 µF | C21 .022 µF |
| Rb | BASS 1M | BASS 1M | VR3 LOW 1M, "A" (audio) |
| Rm | MID 25K | MID 25K | VR4 MID 50K, "B" (linear) |
| Makeup | +5.0 dB | +6.0 dB | +2.8 dB |

5150 link: https://www.prowessamplifiers.com/schematics/post/peavey_evh5150.pdf
(same reference as docs/5150-presence.md and docs/5150-resonance.md).

**Assumptions (not on the sheets):**
- RF-1F prints no pot tapers. Rectifier uses the 5150 sheet's laws: Bass audio,
  Treble and Mid linear.
- The audio law shape (15 % at mid-rotation) is a generic A-taper, not measured.
- RF-1F prints the bass/mid caps as ".02"; used as 20 nF exactly as printed.
- C14 and C60 are read as parallel (both drawn between the drive node and the
  top of VR5).

**Not modelled:** the driving stage's source impedance (RF-1F: V3B cathode
follower via LDR8/LDR9 and the R215 1M bridge; 5150: the V5A drive node with R24
33K and the C59 .01 µF + R94 47K network), the load on the treble wiper (RF-1F:
1M master), and the 5150's R65 47K branch from the Low/Mid junction.

## Level

The makeup is fixed per part set and placed right after the network; it does not
change the network. It was set from the whole-plugin RMS of the regression signal
at default knobs (Bass 5, Mid 4, Treble 5.5) against the pre-tone-stack renders:

| Voice | Boost off | OD-808 | SD-1 |
|-------|-----------|--------|------|
| Red v1 | −1.00 dB | −1.44 dB | −1.37 dB |
| Red v2 | +1.33 dB | −1.01 dB | −0.96 dB |
| Orange | +1.67 dB | −1.68 dB | −1.53 dB |
| 5150 Lead | −0.01 dB | | |

Filter-only difference at default knobs vs the Rev1 biquads (70 Hz–6 kHz):
Red RMS 1.2 dB (worst 2.1 dB, −2.1 dB at 630 Hz), Orange RMS 1.7 dB (worst
2.8 dB), Lead RMS 1.0 dB (worst 2.0 dB). The real stacks scoop the 400–800 Hz
region a little more and keep more low end at the defaults.

## Discretisation and runtime

- Bilinear transform prewarped at 3 kHz (min(3 kHz, fs/4)). Worst digital vs
  analog magnitude to 8 kHz over a 5×5×5 knob grid: 0.54 dB at 44.1 kHz, 0.45 dB
  at 48 kHz, 0.11 dB at 96 kHz, 0.03 dB at 192 kHz.
- Runtime structure: nodal state-space; each capacitor is a trapezoidal companion
  (Gc = C·K) and the three states are its history sources, so capacitor voltages
  stay continuous when a pot moves or Ct changes (history rescaled with Gc).
- Knob values, parts and makeup are smoothed (one-pole, 20 ms). While moving, the
  6-node system is re-solved every 8 samples and the state-space matrices ramp
  linearly per sample. A 0→10 jump on all three knobs stays at the click-free
  bound of the 2nd-difference detector (unsmoothed would be 400× larger).
- Tails are flushed to exact zero below 1e-30 (no denormals).
- The stacks run at the base rate after the 4× section (same place as before).
