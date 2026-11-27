# 5150 Resonance: declared small-signal model

> **Rev2.4 update.** The 5150 now has a 4x12 load model (`SpeakerLoad5150`, see
> `docs/5150.md`). Its output impedance comes from this same NFB loop, so
> Resonance now also raises the cabinet peak near 100 Hz. That is the
> real-amp behaviour that assumption 2 below says was missing: +5.4 dB at
> Resonance 0, rising to +8.5 dB at 10 re 1 kHz. Position 0 is no longer a
> bit-identical reference of the old Lead, because the whole 5150 engine
> changed in Rev2.4.

Like Output Presence, this is a **relative closed-loop response approximation**,
not measured hardware or a nonlinear power-amp/speaker simulation. Position
0/default is the existing Lead sound **bit-for-bit**; turning up adds the
calculated change from that reference. Rectifier DSP is untouched.

## Circuit provenance and readable values

- Source: [EVH 5150, sheet 3, Tone Stack/PI](https://www.prowessamplifiers.com/schematics/post/peavey_evh5150.pdf),
  cross-checked with the clearer Peavey drawing 81501510 (EVH 120 PREAMP)
  [scan](https://el34world.com/charts/Schematics/Files/Peavey/5150_evh.pdf).
  No specific production revision is claimed.
- **VR8** is labelled `POT0752` / `1M5%A`: read as a **1 MΩ audio-taper** pot,
  wired as a rheostat (wiper strapped to one end).
- **C12 = .0068 µF**, in **parallel** with VR8.
- The VR8 || C12 pair sits in **series with R56 (39 kΩ)** between the speaker
  feedback tap and the feedback/PI-tail node (where VR9 Presence + C8||C62 and
  the nominal R57 4.7 kΩ load sit). R85 220k / R81 2.2M are on the PI signal
  grid branch and are not part of this divider (same as the Presence model).
- VR8 = 0 Ω shorts the network → feedback resistor is plain R56 → this is the
  reference that `LeadPresence` already assumed ("resonance bypassed").
  The pot law and CW direction are not verified against a real chassis; we
  take minimum resistance as fully CCW (knob 0), matching the reference.

## Assumptions (stated explicitly)

1. **Forward gain A = 20 V/V**, real and frequency-independent (same declared
   calibration as Presence; not measured or fully derivable from the sheets).
2. **Speaker load is resistive** (nominal). No speaker impedance peak, output
   transformer LF roll-off, output impedance/damping change, PI/tube
   nonlinearity or sag is modeled. In a real 5150 the Resonance control acts
   mostly at the cabinet's impedance peak (roughly 80–120 Hz), because lowering
   LF feedback raises output impedance there. With a resistive load the same
   network instead gives a **broad low-frequency shelf** (below ~1/(2π·VR8·C12)).
   That is the honest result of the declared model, not a speaker peak.
3. Feedback-node load `RL = R57 = 4.7 kΩ` via the 22 µF coupling (AC short),
   as in `docs/5150-presence.md`.
4. Knob `p = knob/10` maps to `VR8(p) = 1 MΩ · (10^(2p) − 1)/99` (nominal
   two-decade audio law, 0 Ω at 0, 90.9 kΩ at 5, 1 MΩ at 10). Resistance is
   slewed linearly over 30 ms once per sample frame.

## Nodal model and realizable filter

```
Rf(s)   = R56 + Rr/(1 + s·Rr·C12)              Rr = VR8, C12 = 6.8 nF
Z(s)    = RL || (Rp + 1/(s·Cp))                Rp = VR9, Cp = .133 µF
T(s)    = A / (1 + A·Z/(Rf + Z)) = A·(Rf+Z)/(Rf + (1+A)·Z)
H_res   = T(s; Rp, Rr) / T(s; Rp, 0)
```

`H_res` is an exact factor of the loop, so `LeadPresence · LeadResonance =
T(Rp, Rr)/T(10k, 0)`: Presence and Resonance interact as in a single NFB loop
(the Resonance stage receives the current, slewed VR9 resistance each frame).

Over the common denominator `(1+s·Rr·C12)(1+s·(RL+Rp)·Cp)`, `Rf + k·Z` has the
numerator `c0 + c1·s + c2·s²` with

```
c0 = R56 + Rr + k·RL
c1 = (R56 + Rr)·(RL+Rp)·Cp + R56·Rr·C12 + k·RL·(Rp·Cp + Rr·C12)
c2 = R56·Rr·C12·(RL+Rp)·Cp + k·RL·Rp·Cp·Rr·C12
```

`N = poly(k=1)`, `D = poly(k=1+A)`, and `N0`, `D0` the same with `Rr = 0`
(first order). Then `H_res = N·D0 / (D·N0)`: third order over third order.
All roots are real and negative (sums of RC impedances), so `H_res` is
realized as a DC gain `G = N(0)·D0(0)/(D(0)·N0(0))` times three first-order
sections `(1+s·τz)/(1+s·τp) = τz/τp + (1 − τz/τp)/(1+s·τp)`, each a stable
trapezoidal (TPT) one-pole in double precision — the same unwarped bilinear
mapping as `LeadPresence`. No audio feedback loop is evaluated. At `Rr = 0`
the zeros and poles cancel exactly; once the slew settles on 0 the stage
returns the input sample unchanged (bit-identical), while its states keep
running so leaving 0 is continuous.

## Low-end response (model, Presence 0, dB relative to 1 kHz)

Resonance stage alone (`H_res`), from `Tests/ResonanceTest.cpp` (analog nodal
reference; the 48 kHz digital stage matches within 0.006 dB):

| Resonance | VR8 | 40 Hz | 60 Hz | 80 Hz | 100 Hz | 150 Hz | 250 Hz | 1 kHz level vs 0 |
|---|---|---|---|---|---|---|---|---|
| 0 (default) | 0 Ω | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | +0.00 dB |
| 5 | 90.9 kΩ | +3.94 | +3.80 | +3.64 | +3.50 | +3.17 | +2.64 | +1.28 dB |
| 10 | 1 MΩ | +7.72 | +7.24 | +6.76 | +6.32 | +5.39 | +3.96 | +0.97 dB |

Absolute lift vs position 0 at 48 kHz: 20/100/1000 Hz = +5.33/+4.78/+1.28 dB
at 5, +9.07/+7.29/+0.97 dB at 10. HF converges to the reference.

Whole Lead path at default knobs (Gain 6, B5 M4 T5.5, Presence 0), small
signal, dB relative to 1 kHz (offline harness, preamp at 192 kHz):

| Resonance | 40 | 60 | 80 | 100 | 150 | 250 |
|---|---|---|---|---|---|---|
| 0 | −29.1 | −16.3 | −9.7 | −5.8 | −1.3 | +0.8 |
| 5 | −25.1 | −12.5 | −6.0 | −2.3 | +1.9 | +3.5 |
| 10 | −21.4 | −9.1 | −2.9 | +0.5 | +4.1 | +4.8 |

Resonance sits after the distortion, so it adds weight without feeding more
bass into the clipping stages (palm mutes stay as tight as before).

## Placement, parameter and state

```
Lead → Lead TMB → Lead Graphic EQ → Lead Presence → Lead Resonance ─┐
                                                              30 ms model mix → Cab → Gate → Master
Rectifier → TMB/Presence → Rectifier Graphic EQ ────────────────────┘
```

`lead_resonance` (0..10, default **0 = exact unchanged Lead tone**) is appended
after `lead_presence`; all earlier IDs, indices and defaults are preserved.
States without it restore 0 even when loaded over a processor set higher. The
Output-section knob is enabled only in 5150 mode (dimmed for Rectifier).

## Tests

`Tests/ResonanceTest.cpp`: 90 sine magnitude/phase points (3 rates × 0/5/10 ×
6 frequencies, plus Presence 5/10 interaction) against the independent nodal
equations at the bilinear-warped frequency and the unwarped analog curve;
bit-identical reference at 0 (also after moving away and back, and with
Presence engaged); rapid Presence+Resonance sweeps; silence/DC; state
migration; model crossfade endpoints and zero Rectifier leakage; and a
whole-processor null of three 5150 configurations against output captured
from unmodified commit `62a1239` (`Tests/lead-62a1239.f32`).

These are **model agreement tests, not amp measurements**. Mac/Scarlett
listening has not been verified.
