# Rev2.2 — component-level diode clippers and post-TMB gain recovery

Rev2.2 replaces the behavioural `in + clip(gain · hp(in))` Boost waveshapers with
the op-amp clipping stage **solved as a circuit** every sample, and adds a
post-tone-stack recovery stage to the Rectifier Red and 5150 Lead paths. This is
original modelling of published circuit topologies; no product is cloned and no
commercial branding appears in the UI.

## 1. Sources

| Ref | Document | Used for |
|-----|----------|----------|
| [C] | Steve Cerutti, *Tube Screamer TS-808* schematic ("for reference only"), https://www.pedalboard.org/images/archiv/vintage_effects/Ibanez_TS808_ORIGINAL.pdf | TS-808 clipper values (read off the drawing) |
| [E] | ElectroSmash, *Tube Screamer analysis* + BOM, https://www.electrosmash.com/tube-screamer-analysis | cross-check of every TS-808 value, Drive taper |
| [S] | schematicheaven, *Boss SD-1 Super Overdrive*, https://schematicheaven.net/effects/boss_sd1_super_overdrive.pdf | stock SD-1 clipper values, diode orientation |
| [W] | Wampler DIY, *A few easy Boss SD-1 Super Overdrive Mods* (2017), https://wamplerdiy.com/blogs/news/a-few-easy-boss-sd-1-super-overdrive-mods | "Boutique Overdrive" mod values (the SD-1 voicing juicyboi has always used) |
| [H] | hobby-hour.com SD-1 notes; GGG SD-1 clone BOM | D4–D6 = 1S2473, 2 + 1 asymmetric; Drive = 1M audio (stock) |
| [N] | NJM4558 datasheet (Nisshinbo/NJR): GBW 3 MHz typ., A_V 100 dB typ., SR 1 V/µs | op-amp model |
| [D] | standard SPICE 1N914 / 1N4148 model: IS = 2.52 nA, N = 1.752, RS = 0.568 Ω | diode law |

## 2. Component values

### OD-808 Boost (TS-808 values)

| Part | Value | Role | Source / note |
|------|-------|------|---------------|
| C2 | 1 µF (BP) | input coupling | [C], [E] |
| R5 | 10 kΩ → Vr | (+) bias, with C2 = 15.9 Hz HP | [C], [E] |
| R4 | 4.7 kΩ | gain leg | [C], [E] |
| C3 | 0.047 µF | gain leg (720 Hz with R4) | [C], [E]. Rev2.1 used 50 nF from an OD-808 rev.1 stripboard BOM |
| R6 | 51 kΩ | fixed feedback | [C], [E] |
| Drive | 500 kΩ **A** (audio), rheostat | feedback | [C] prints "500KA", [E] says log. One clone site lists it linear: we follow [C]/[E] |
| C4 | 51 pF | across feedback | [C], [E]. Rev2.1 used 50 pF (rev.1 BOM) |
| D1, D2 | 1N914, antiparallel | across feedback | [C]; [E] lists MA150/1N914 equivalents |
| IC | JRC4558D | op-amp | [C] |
| R7 / C5 | 1 kΩ / 0.22 µF | post-clip LP (723 Hz) into the tone stage | [E]; [C] prints "22uf tant" for this cap, read as .22 µF (matches [E] and the .22 µF tone cap) |

### SD-1 Boost (stock SD-1 topology + Boutique-mod values)

juicyboi's SD-1 voice has always been the "Boutique Overdrive" mod from [W].
Rev2.2 keeps that voicing and uses the mod's values where [W] changes a part,
and the stock values from [S] for everything [W] leaves alone. [W] uses the
same reference designators as the Boss board, so both columns line up.

| Part | Stock [S] | Used (Boutique mod [W]) | Role |
|------|-----------|-------------------------|------|
| C2 | 0.018 µF | **0.055 µF** | input coupling |
| R4 | 100 kΩ → Vr | 100 kΩ (unchanged) | (+) bias; with C2: 28.9 Hz HP (stock: 88 Hz) |
| R6 | 4.7 kΩ | **1 kΩ** | gain leg |
| C3 | 0.047 µF | **0.22 µF** | gain leg (723 Hz) |
| R5 | 33 kΩ | **10 kΩ** | fixed feedback |
| Drive | 1 MΩ (audio per [H]; taper not printed on [S]) | **500 kΩ audio** | feedback rheostat |
| feedback cap | none fitted | none | — |
| D4, D5, D6 | 1N916 [S] / 1S2473 [H] | unchanged | two in series conduct for out > (−), one for out < (−) |
| R7 / C4 | 10 kΩ / 0.018 µF | **1 kΩ / 0.15 µF** | post-clip LP (1.06 kHz) |
| C5 / Tone | 0.027 µF + 470 Ω / 22 kΩ | **0.15 µF / 5 kΩ** | tone LP |
| C6 | 0.01 µF | **removed** | recovery-stage HF cap |

[W] prints C2 as ".055UF", an unusual value (questioned in the page's comments);
it is used as printed.

## 3. Model

Non-inverting op-amp, everything referenced to Vr (AC ground):

```
          +-- Rf = Rfixed + Rdrive·taper --+
          +-- Cfb ------------------------ +
          +-- diode strings -------------- +
          |                                |
vin ->(+) |>-------------------------------+-- vo
      (-)-+
          Rleg — Cleg — Vr
```

* Op-amp: single pole, `dvo/dt = ωt(v+ − v−) − (ωt/A0)·vo`, GBW 3 MHz, A0 = 100 dB [N].
* Diodes: Shockley strings `Id(v) = Is(e^{v/(Np·n·Vt)} − 1) − Is(e^{−v/(Nn·n·Vt)} − 1)`,
  Is = 2.52 nA, n = 1.752, Vt = 25.85 mV (27 °C). OD: Np = Nn = 1; SD-1: Np = 2, Nn = 1.
* Discretisation at the 4× rate: trapezoidal companions for Cleg and Cfb (SPICE's
  default rule), BDF2 for the op-amp integrator (2nd order and L-stable: with the
  diodes on, the loop pole sits far above Nyquist where trapezoidal rings at fs/2).
* v− is eliminated in closed form; the scalar implicit equation in the
  feedback-network voltage is solved with a bracketed Newton–Raphson: predictor
  from the last sample, one `exp` per iteration, SPICE-style junction limiting,
  an inverse-diode step when deep in conduction, bisection fallback. The function
  is strictly monotonic, so the root is unique and convergence is guaranteed.
  Mean 2.0 iterations, worst 10 under 8 V / 5 kHz abuse at every rate.
* Drive is smoothed as a resistance (20 ms); Tone and Level are smoothed per sample
  (this was the old "smooth OD level" TODO). Cap states are physical, so knob moves
  act on charged caps like the real pedal.
* The OD-808 / SD-1 stages only run while audible: both during a type or on/off
  crossfade, one while the Boost is on, none when it is off. A stage restarts from rest.

**Assumptions / not modelled.** Op-amp rails (9 V supply; the outputs here stay
under about 2.2 V for a 1 V input) and slew limit (1 V/µs; GBW limits the slopes
first). Diode RS (0.57 Ω; under 1 mV at these currents). The 1N916/1S2473 use
the 1N914 model. The input buffer's source impedance and the tone-stage load on
the clipper are not modelled. 1.0 digital = 1 V at the pedal input. The stages
after the clipper (Tone blend, Level, output HP) are the Rev2.1 behavioural ones,
now smoothed. **Rev3.1:** the post-clip network (post LP, active tone stage, Level pot,
output buffer) is now solved too, see [pedal-tone.md](pedal-tone.md).

## 4. Validation against ngspice

`tools/clippers/od808-clipper.cir` and `sd1-clipper.cir` hold the same circuit:
a G–C–R single-pole op-amp, the same diode model, trap, 0.65 µs max step.
`tools/clippers/plot_clippers.py build/juicyboi_clipper_dump <outdir> [fs]` runs a
1 kHz sine from rest for 30 ms. It covers Drive 0/5/10 × input 0.05/0.3/1.0 V and
compares the C++ clipper (input coupling + solved stage) with ngspice. Plots:
`clipper-od.png`, `clipper-sd.png`.

| fs (4× of) | OD-808 worst rms err | OD-808 worst sample err | SD-1 worst rms err | SD-1 worst sample err | worst harmonic Δ (H2–H9 above −60 dBc) |
|------------|---------------------|-------------------------|--------------------|-----------------------|-------------|
| 192 kHz (48 k) | 0.33 % | 2.4 % of peak | 1.1 % | 10.1 % of peak | OD 0.06 dB, SD 0.58 dB |
| 176.4 kHz (44.1 k) | 0.34 % | 3.0 % of peak | 1.8 % | 18.7 % of peak | OD 0.07 dB, SD 0.23 dB |

The large sample errors are single samples on the SD-1's near-vertical edges at
Drive 10. With no feedback cap, the closed-loop gain is 511 and the edge is
shorter than one sample, so a sub-sample timing difference shows up as a large
error. Every other sample agrees to within about 1 %. The error halves each time
fs doubles (SD-1 worst rms 0.54 % at 384 kHz, 0.17 % at 768 kHz), which
confirms it is discretisation and not a model difference. Small-signal gain matches
the analytic linear network to within 0.036 dB (Drive 0/5/10, 100 Hz–3 kHz). The
symmetric pair clips at ±0.53 V, and the SD-1 at +1.05 / −0.56 V (ratio 1.87).
Four of the ngspice runs are stored in `Tests/clipper-ngspice-ref.txt` and checked by
`ClipperTest`.

**Aliasing (4×, worst case: 3 kHz sine at 1 V, whole stage, ideal decimation):**
alias energy re harmonics is −46 dB for OD Drive 10, −66 dB for OD Drive 5,
−43 dB for SD-1 Drive 10 and −50 dB for SD-1 Drive 5. Without oversampling it would
be about −28 dB. 4× is kept; an oversampling quality setting is on the roadmap.

**CPU (this Linux box, -O2):** about 55–65 ns per stage-channel-sample at the 4×
rate. One active stage in stereo at a 48 kHz host costs about 2.5 % of one core
(about 5 % for the ~20 ms of a type crossfade, 0 % with the Boost off).

## 5. Level at default knobs

Measured as the whole-plugin RMS of the regression render vs the Rev2.1 render
(`rect-rev2-tonestack.f32` / `lead-rev2-tonestack.f32`, kept as references):

| | Boost off | OD-808 | SD-1 |
|---|---|---|---|
| Orange (no recovery: isolates the clipper change) | −0.08 dB | −0.18 dB | −0.08 dB |
| Red v2 (default voice) | −0.08 dB | +0.00 dB | +0.01 dB |
| Red v1 | +0.68 dB | −0.05 dB | −0.06 dB |
| 5150 Lead defaults / +SD-1 Gain 8 / +OD-808 Presence 10 | −0.09 dB | −1.51 dB (cfg 2) | −1.36 dB (cfg 1) |

No makeup was added to the clipper itself. The boosted 5150 configurations lose
up to 1.5 dB because the louder boosted signal is compressed by the new recovery
stage.

## 6. Rev2.2 gain recovery (Red and 5150 Lead)

**Finding.** In both Rev1 and Rev2 all of the preamp distortion happens *before* the tone
stack, and the cascades are identical. Rev2.1's real passive TMB passes relatively
more fundamental than Rev1's behavioural EQ did, so it lowered the
harmonic-to-fundamental ratio at the output. The probe
(`juicyboi_harmonics`, 110 Hz sine, Cab/EQ/Gate/Boost off, default tone knobs)
showed 1.3–3.5 dB less of every harmonic and 2–2.5 dB lower THD at every Gain
setting. Driving the cascade harder (×1.4…×2.8 input) did not close the gap at
high Gain: the cascade is already saturated and the stack's weighting sets the
output ratio.

**Change.** Both reference amps follow the passive stack with more tube gain
before the power section: the Dual Rectifier's FX-loop recovery triodes and
phase inverter, and the 5150's loop/PI triodes. Rev2.1 had only clean makeup there.
Rev2.2 adds `RecoveryStage` after the TMB. It is a behavioural stand-in, not a
solved tube: coupling HP, then the asymmetric triode curve normalised to unity
small-signal gain. Its drive is fixed per channel and stands in for a typical
Master; the real stage would track the Master. The TMB itself is unchanged. To keep
this stage alias-free, the tone stacks and the recovery stage now run inside the
4× region. The TMB is therefore discretised at 4× fs, which makes it more accurate.

| Path | drive | bias | curve | out trim |
|------|-------|------|-------|----------|
| Rectifier Red (v1 and v2) | 2.0 | 0.40 | positive half flattens first | +0.6 dB |
| 5150 Lead | 2.0 | 0.50 | mirrored, so it adds to (not cancels) the cascade's dominant H2 | −0.6 dB |
| Orange | 0 (exact bypass) | — | — | — |

**Before / after vs Rev1** (THD, number of harmonics within 40 dB of H1,
harmonic energy at or above 2 kHz re H1, output RMS):

| cfg | Gain | in (FS) | THD Rev1 / Rev2.1 / Rev2.2 | n(−40 dBc) | ≥2 kHz [dBc] | level [dBFS] |
|-----|------|---------|-----------------------------|------------|--------------|--------------|
| Red v2 | 3 | 0.05 | 12.9 % / 11.0 % / 22.6 % | 3 / 3 / 3 | −107.9 / −109.3 / −106.5 | −21.6 / −19.9 / −19.3 |
| Red v2 | 3 | 0.25 | 22.0 % / 16.6 % / 29.0 % | 9 / 7 / 10 | −55.6 / −57.0 / −49.4 | −15.8 / −14.2 / −14.3 |
| Red v2 | 5 | 0.05 | 21.8 % / 17.6 % / 30.7 % | 7 / 5 / 8 | −69.2 / −70.6 / −63.9 | −17.4 / −15.7 / −15.4 |
| Red v2 | 5 | 0.25 | 26.0 % / 19.2 % / 31.9 % | 16 / 14 / 18 | −36.5 / −37.8 / −33.0 | −15.2 / −13.6 / −13.7 |
| Red v2 | 7 | 0.05 | 28.1 % / 21.8 % / 35.2 % | 14 / 13 / 16 | −39.6 / −41.0 / −36.2 | −15.7 / −14.1 / −14.1 |
| Red v2 | 7 | 0.25 | 26.1 % / 19.3 % / 32.5 % | 16 / 15 / 18 | −35.7 / −37.0 / −32.3 | −15.2 / −13.6 / −13.7 |
| Red v2 | 10 | 0.05 | 28.8 % / 21.7 % / 35.7 % | 20 / 19 / 23 | −30.3 / −31.6 / −28.0 | −15.3 / −13.7 / −13.8 |
| Red v2 | 10 | 0.25 | 30.4 % / 23.7 % / 37.5 % | 22 / 21 / 25 | −28.0 / −29.3 / −26.7 | −15.6 / −14.0 / −13.9 |
| Lead | 3 | 0.05 | 9.9 % / 8.5 % / 15.6 % | 1 / 1 / 2 | −130.6 / −131.1 / −130.9 | −28.5 / −28.8 / −29.3 |
| Lead | 3 | 0.25 | 37.2 % / 31.7 % / 45.8 % | 5 / 5 / 7 | −93.9 / −94.3 / −73.2 | −16.2 / −16.6 / −16.2 |
| Lead | 5 | 0.05 | 22.2 % / 18.9 % / 32.6 % | 2 / 2 / 3 | −118.3 / −118.8 / −113.8 | −20.8 / −21.1 / −21.0 |
| Lead | 5 | 0.25 | 36.8 % / 31.6 % / 47.2 % | 8 / 9 / 11 | −56.9 / −57.2 / −48.9 | −12.8 / −13.2 / −13.5 |
| Lead | 7 | 0.05 | 35.3 % / 30.2 % / 45.2 % | 5 / 5 / 7 | −89.1 / −89.5 / −74.2 | −14.4 / −14.8 / −14.7 |
| Lead | 7 | 0.25 | 28.8 % / 24.9 % / 43.0 % | 10 / 11 / 14 | −46.7 / −47.1 / −41.6 | −12.0 / −12.3 / −12.9 |
| Lead | 10 | 0.05 | 28.4 % / 24.5 % / 42.4 % | 8 / 10 / 12 | −52.8 / −53.2 / −47.1 | −12.0 / −12.4 / −13.0 |
| Lead | 10 | 0.25 | 23.6 % / 20.4 % / 37.8 % | 11 / 11 / 14 | −46.4 / −46.8 / −40.5 | −11.6 / −11.9 / −12.7 |

At every grid point Red v2 and the Lead now meet or beat Rev1's THD (worst
+1.8 dB, mean +2.5 dB Red v2 / +3.1 dB Lead) and harmonic count. The ≥2 kHz
energy also meets or beats Rev1, except the Lead Gain 3 / 0.05 point, which is
0.3 dB under at −131 dBc (numerical floor). The output level stays within
−0.8…+0.6 dB of Rev2.1. Red v1 shares the Red recovery stage (THD worst +2.4 dB
vs Rev1). Orange is unchanged and stays about 2 dB under Rev1's THD: no request
covered it. Plot: `gain-recovery-harmonics.png`.

Reproduce: build `juicyboi_harmonics` on any revision (parameter IDs are stable)
and run `juicyboi_harmonics [spectrum-dir|-] [input-scale] [cfg]`.
