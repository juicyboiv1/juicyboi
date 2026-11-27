# Rev3: solved 12AX7 triode stages

Rev3 replaces the behavioural waveshapers in every preamp tube of the
Rectifier (Red v1/v2, Orange) and the 5150 (Rhythm/Crunch and Lead) with a
solved 12AX7 common-cathode stage. The stage has grid conduction and
coupling-cap bias shift. Rev3 is the new baseline. It is a new plugin,
`juicyboi Rev3` (code `Jcb3`, `com.juicyboi.juicyboi-rev3`,
LV2 `urn:juicyboi:juicyboi-rev3`, v3.0.0, app `juicyboi Rev3.app`). It
replaces Rev2, and Rev2 sessions do not load into it. Rev2.4 is the git tag
`rev2.4`.

Rev3 also carries two changes from the Rev2.4.x line:
- the shared **Input Cal** trim (taken off both faceplates);
- the **Preamp Out** toggle (see [per-amp-state.md](per-amp-state.md)).

## 1. Model (`Source/dsp/Triode.h`)

```
vs --[Rs]--||Cc||-- n --[Rgs]-- grid      B+ --[Ra]-- plate (output)
                    |                       cathode --[Rk || Ck]-- gnd
                   [Rg]
                    |
                   gnd
```

- **Plate current:** Koren 12AX7 (mu 100, ex 1.4, kg1 1060, kp 600, kvb 300).
- **Grid current:** Cohen/Hélie grid diode,
  `Ig = Gg·(ln(1+e^(Cg·Vgk))/Cg)^xi + Ig0`, with Gg 6.062e-4, xi 1.354,
  Cg 13.9 and Ig0 3.917e-8.
- **Bias shift:** grid current flows back through Rgs and charges Cc, so the
  grid's DC point walks negative under heavy drive. It recovers with the
  Cc·(Rs+Rg) time constant, which gives blocking distortion.
- **Discretisation:**
  - trapezoidal companions for Cc and Ck (the SPICE default rule);
  - per sample, the stage reduces exactly to two unknowns (Vgk, Vpk);
  - a damped, step-limited 2×2 Newton-Raphson solves them, predicted from
    the last sample;
  - everything runs at the existing 4× oversampled rate.
- **Output:** the plate voltage minus its quiescent value, inverted as on the
  tube.

## 2. Component values (`Source/dsp/TriodeParts.h`)

Each stage uses the plate/cathode parts from the schematics. The passive
network in front of it is collapsed to the equivalent Rs/Cc/Rg/Rgs. The gain
pots and the 5150 Pre Gain networks stay outside the stage and are modelled
separately, as in Rev2.4.

| Stage | B+ | Ra | Rk | Ck | Cc | Rs | Rg | Rgs |
|---|---|---|---|---|---|---|---|---|
| Recto V1A | 395 V | 220K | 1.8K | 1µ | DC | 10K | 1M | 1K |
| Recto V2A | 391 V | 100K | 1.8K | 1µ | 20n | 48K | 1.3M | 2.9M |
| Recto V2B (cold) | 391 V | 100K | 39K | none | 20n | 508K | 1M | 1K |
| Recto V3A | 409 V | 220K | 1.8K | 1µ | 20n | 100K | 330K | 220K |
| 5150 V1A | 290 V | 220K | 1.8K | 1µ | DC | 10K | 1M | 68K |
| 5150 V1B | 290 V | 100K | 1.8K | 1µ | 22n | 518K | 500K | 720K |
| 5150 V2A (cold) | 290 V | 100K | 39K | none | 22n | 508K | 1M | 1K |
| 5150 V2B | 320 V | 220K | 1.8K | 1µ | 22n | 320K | 330K | 1K |
| 5150 V5B | 320 V | 220K | 2.2K | none | 22n | 1.05M | 100K | 1K |
| 5150 V5A | 320 V | 100K | 2.2K | none | 22n | 1.12M | 1M | 1M |

The Rectifier B+ values are back-solved from the plate and cathode voltages
printed on sheet RF-1F. The model's operating points against that sheet:

| Stage | Plate, model (sheet) | Cathode, model (sheet) |
|---|---|---|
| V1A | 205.8 V (200) | 1.55 V (1.6) |
| V2A | 278.4 V (280) | 2.03 V (2.0) |
| V2B | 379.2 V (384) | 4.59 V (6.0) |
| V3A | 213.8 V (213) | 1.60 V (1.6) |

The 5150 supply voltages (290/320 V) are not printed on the sheets available,
so they are nominal assumptions.

## 3. Drive staging (`Source/dsp/TriodeCalibration.h`)

Each solved stage sits between the same normalised drive and makeup as the
Rev2.4 behavioural stage it replaces. `inScale` puts the solved stage's
half-gain point (the first side to clip, at quiescent bias) where the old
curve's slope had fallen to half. `outScale` keeps the small-signal gain.

Per-stage drive trims were then searched with `juicyboi_triode_tune` against
Rev2.4's levels and compression. The cathode-bypass shelf of each stage is 80%
compensated (ShelfEq), so 40/80/100 Hz stay within about 1 dB of Rev2.4
(section 5). The curve shape, asymmetry, grid conduction and bias shift all
come from the circuit.

## 4. ngspice validation (`tools/triode/*.cir`, `plot_triode.py`)

ngspice runs the same equations with `method=trap reltol=1e-6` at 192 kHz
(4× of 48k), using 1 kHz bursts plus 100 Hz blocking bursts. The worst-case
numbers from the full set (`triode-ngspice-192k.txt`):
- RMS error ≤ 0.028 % of swing;
- worst single sample ≤ 0.32 % of swing (fV5A 100 V and rV3A 30 V, hard
  grid conduction);
- Vgk worst 6.7 mV;
- harmonic magnitudes within 0.00 dB;
- Newton iterations: mean ≈ 2.0–2.4, max 8.

`TriodeTest` replays four of these transients on every smoketest
(`Tests/triode-ngspice-ref.txt`):

| Transient | RMS error | Worst sample (of swing) | Vgk worst |
|---|---|---|---|
| rV1A 2 V | 0.0010 % | 0.0019 % | 0.03 mV |
| rV2B 8 V | 0.0007 % | 0.0007 % | 0.02 mV |
| rV3A 30 V | 0.0095 % | 0.069 % | 1.62 mV |
| fV5A 30 V | 0.0045 % | 0.016 % | 0.29 mV |

TriodeTest also checks:
- **V3A bias shift:** a loud burst drives the grid to −2.58 V. It is back to
  −0.10 V 50 ms after the burst and −0.09 V after 300 ms.
- **Stability:** across 4× of 44.1/48/96/192k with hot, impulse, quiet and
  silent input, the worst case is 15 Newton iterations (limit 20).
- **Gated cascades:** they wake click-free and converge to the warm-running
  output.

Plots: [img/triode-ngspice.png](img/triode-ngspice.png) and
[img/triode-blocking.png](img/triode-blocking.png).

## 5. Measurements vs Rev2.4

Both builds were measured with the same probes (`juicyboi_bassprobe`,
`juicyboi_harmonics`, `juicyboi_dynamics`), in a Release build, full
processor, 48 kHz, Cab/EQ/Gate/Boost off, default knobs.
Plot: [img/rev3-vs-rev2.4.png](img/rev3-vs-rev2.4.png)
(`tools/triode/plot_rev3_vs_rev24.py`).

**Bass, driven 0.1 FS sine, Δ dB Rev3 − Rev2.4:**

| Config | 40 Hz | 80 Hz | 100 Hz | 1 kHz | 3 kHz |
|---|---|---|---|---|---|
| Red v2 B5 | +0.1 | +0.4 | +0.1 | +0.5 | +0.5 |
| Red v2 B10 | −0.2 | +0.2 | +0.1 | +0.5 | +0.5 |
| Red v1 B5 | +0.1 | +1.0 | +0.3 | −1.8 | −1.1 |
| Orange B5 | −0.8 | +0.7 | +0.4 | −0.8 | −0.5 |
| 5150 Lead B5 | −0.9 | +0.1 | +0.2 | 0.0 | −0.6 |
| 5150 Crunch B5 | −2.5 | −0.9 | −0.5 | −0.2 | −1.1 |

Above 4 kHz the Lead rolls off about 1.5 dB more at 4 kHz and 4.8 dB more at
6 kHz.

**Harmonics, 110 Hz at 0.25 FS, Gain 7:**
- Red v2: H2 −14.8 → −17.1 dBc, H3 −18.1 → −17.6, THD 25.4 → 22.6 %.
- 5150 Lead: H2 −14.9 → −28.0 dBc, H3 −18.8 → −16.9, THD 25.6 → 16.9 %.

The solved stages are more odd-dominant and less fizzy-even, with lower THD
at the same output level. Two-tone IMD (E2+B2) drops on Red v2: −11.7 → −16.5
dB at B5 and −11.8 → −15.0 at B10.

**Dynamics** (a 300 ms power chord over a quiet note, then the quiet note
alone):
- The chord's attack peak is about the same: Red v2 +2.85 → +2.67 dB,
  Lead +1.66 → +1.33 dB.
- The quiet note's dip after the chord (the bias-shift "breath") gets
  deeper on the Rectifier: Red v2 −1.2 → −2.3 dB, Red v1 −0.7 → −2.4,
  Orange −1.0 → −3.3. It is back within 1 dB in 20 ms.
- On the 5150 the dip is Lead −3.7 → −1.7 dB (Gain 10: −2.4 → −3.2) and
  Crunch −5.0 → −6.5 dB.

**CPU** per 256-sample block (mono in, one amp active, this Linux box):

| Amp | Rev2.4 | Rev3 | Change |
|---|---|---|---|
| Red v2 | 829 µs (15.6 % of a core) | 941 µs (17.7 %) | +13 % |
| 5150 Lead | 818 µs (15.3 %) | 1323 µs (24.8 %) | +62 % (six solved tubes) |
| 5150 Crunch | 812 µs | 1188 µs (22.3 %) | +46 % |

A modern Mac core is much faster than this box, so expect lower numbers there.

**Levels:** every Rectifier voice and boost is within 1.5 dB of Rev2.4
(Red v1 OD-808 −1.48 dB is the largest gap). The 5150 fixture configs are
within ±0.6 dB.

## 6. Tests and fixtures

- `rect-rev3.f32`, `lead-rev3.f32` and `rev3-migration-rect.f32` were captured
  once from commit 93ff554 and null at 0.
- The Rev2.4 renders are kept as level references with a 1.5 dB gate.
- The old Rev1-level sanity check vs 348fc68 is widened to 3 dB (Red v2
  −3..+4 dB), because the triodes compress the boosted voices slightly more
  than Rev1 did.

## 7. Limitations

- **Supply:** the B+ is ideal (stiff), with no sag or recovery. That is the
  next step (**supply sag**: rectifier/choke/filter-cap model with a shared
  node per supply, for compression and bloom).
- **Calibration:** the stages are calibrated onto Rev2.4's tuned gain
  staging rather than driven purely at schematic voltages. The circuit sets
  curve shape, asymmetry, grid current and bias shift. The drive into each
  stage is still matched to Rev2.4's feel.
- **Tube variance:** one Koren/Cohen-Hélie 12AX7 fit for every tube, with no
  tube-to-tube spread and no heater or microphonic effects. Miller capacitance
  stays as the Rev2.4 linear low-pass around each stage.
- **5150 supply:** 290/320 V are nominal assumptions (not on the sheets).
  Crunch loses about 2.5 dB at 40 Hz and the Lead is darker above 4 kHz than
  Rev2.4.
- **CPU:** the 5150 costs about 60 % more than in Rev2.4.
- **Unverified builds:** AU and AAX were not built here (Linux). Check that
  they load on macOS. AAX still needs PACE signing for retail Pro Tools.
