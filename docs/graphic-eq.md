# GEQ-7 graphic EQ (Rev3.2, component level)

Rev3.2 replaces the RBJ-biquad graphic EQ with a solved circuit model of the
classic 7-band gyrator pedal EQ topology (GE-7 style). In the UI it is the
"GEQ-7"; it has no Boss branding and no affiliation. It stays where the
EQ was:
- one EQ per amp (`eq_*` for the Rectifier, `m5150_eq_*` for the 5150);
- post tone stack and pre cab;
- it now runs inside the 4x-oversampled region, after the preamp and before
  the 5150 Presence/Resonance.

## Circuit (published values: Halverson redraw / Swellamps bench notes)

**Input.**
- R30 10k, C22 47n to the input, R29 470k bias.
- U5a pre-emphasis: R31 4.7k, R28 470 + C20 15n. Modelled ideal: the gain is
  low and it sits well inside the op-amp's bandwidth.

**U4b Level stage.**
- R27/R26 10k, with the 10k Level pot between the op-amp inputs.
- Leg: R2 2.2k + C16 10µ.

**U4a band stage.**
- R24/R25 3.3k, with seven 10k slider pots between the op-amp's + and − inputs.
- Each wiper drives a leg to ground:

| Band | Series C | Gyrator C | R1 | R2 |
|---|---|---|---|---|
| 100 Hz | 1.5µ | 56n | 100k | 330 |
| 200 Hz | 680n | 33n | 82k | 330 |
| 400 Hz | 330n | 15n | 100k | 330 |
| 800 Hz | 150n | 8.2n | 100k | 330 |
| 1.6 kHz | 100n | 3.9n | 82k | 330 |
| 3.2 kHz | 39n | 2.2n | 82k | 330 |
| 6.4 kHz | C1 47n + R3 820 (no gyrator: a shelf-like top band) | | | |

Each gyrator is a TL022 follower with C2 to its input and R1 to ground.

**Output.**
- C19 1µ (R32 1M).
- De-emphasis: R33 4.7k / (R36 470 + C27 15n).
- C26 47n, R34 1M.
- Q1 emitter follower. Small-signal values from the ngspice `.op`: gm 11.45 mS,
  Ic 0.3 mA, so the follower gain is 0.990 and its input resistance 2.84 MΩ.
- R14 1k, C11 1µ, R12 100k load.

**Op-amps.** Each is a single-pole integrator: the 4558 (U4a/U4b) at 3 MHz GBW
and the TL022 followers at 0.5 MHz. This matters, because the GBW moves the top
bands by up to 2.8 dB.

## Implementation

`Source/dsp/GraphicEq.*`.

1. **Capacitors.** Every capacitor is a trapezoidal companion. The digital
   circuit is therefore exactly the bilinear transform of the analog circuit;
   matched to the analytic bilinear response to ≤ 3e-5 dB.
2. **Per sample.** Each wiper leg reduces to a Thevenin source, and each
   op-amp stage is a closed-form 2×2 solve that includes the op-amp
   integrator's state.
3. **Oversampling.** It runs at 4x, because at base rate the bilinear warping
   would cost up to 1.5 dB at the top.
4. **Sliders and switching.**
   - Wiper positions are smoothed per sample (10 ms), so slider moves do not
     zipper.
   - On/off is a 10 ms smoothstep crossfade.
   - Off is true bypass: the buffer is untouched once the fade-out has finished.
5. **CPU.** About 1.5 % of one core per channel at 4x 48 kHz (measured in
   `GraphicEqTest`).

## Slider law (mapping change)

The parameter IDs, ranges (±15 dB) and saved values are unchanged. The real
pedal uses linear 10k pots, and a linear pot is very non-linear in dB: +6 dB is
about 91 % of the travel. The plugin therefore keeps the slider as a
**dB-calibrated scale**:
- ±15 is the end of the wiper's travel;
- 0 is the centre;
- in between, the wiper is placed so that the band's peak gain is
  (value / 15) × that band's maximum.

The maxima in this circuit:

| Band | Peak at | Full boost | Full cut | Slider +6 → wiper |
|---|---|---|---|---|
| 100 | 93.7 Hz | +15.90 dB | −15.90 dB | 0.908 |
| 200 | 201.6 Hz | +14.60 | −14.61 | 0.911 |
| 400 | 391.3 Hz | +14.09 | −14.09 | 0.913 |
| 800 | 786.2 Hz | +13.73 | −13.73 | 0.913 |
| 1.6k | 1547 Hz | +14.00 | −13.99 | 0.913 |
| 3.2k | 3285 Hz | +14.53 | −14.51 | 0.909 |
| 6.4k | (12.8 kHz reference) | +12.18 | −12.11 | 0.854 |
| Level | 1 kHz | +14.88 | | |

So old sessions keep approximately their dB settings. Saved +15 now means
"slider at the top" (+13.7 to +15.9 dB, depending on the band). The two
remaining differences are real circuit behaviour and are intentional:
- the GE-7 bands are broader than the old Q 1.3 biquads, and adjacent bands
  interact the way the hardware does;
- the circuit loses about 0.4–0.7 dB with every slider at 0 (input divider,
  de-emphasis, follower, coupling caps): −0.5 dB at 1 kHz, −0.74 dB at 40 Hz.

**Effect on saved sessions** (fixtures in `Tests/`, EQ on):

| Session | Level vs Rev3.1 |
|---|---|
| 5150 lead + boost, 100 +4 / 800 −3 | −0.54 dB |
| Orange, 200 +5 | −0.03 dB |
| Rev2.3 state 0 | −0.76 dB |
| Rev2.3 state 2 | −0.26 dB |
| Rev2.3 state 3 (200 −2 into the Rectifier; the test input is mostly 41/82 Hz, where the broad 200 Hz cut and the flat loss reach about −2.2 dB) | −1.53 dB |

Every session with the EQ off still nulls against Rev3.1.

## Validation

**ngspice.** `tools/geq7/geq7_ngspice.py` builds the full netlist (4558 at
3 MHz / TL022 at 0.5 MHz, a real BJT, U5a ideal) for 16 wiper settings over
121 frequencies. `--export` writes `Tests/geq7-ngspice-ref.txt`, and `--plot`
writes `docs/img/geq7-ngspice.png`. `tools/geq7/ge7_model.py` is the
analytic Python model.

**`Tests/GraphicEqTest.cpp`** (smoketest):

| Check | Result |
|---|---|
| Analytic C++ model vs ngspice | 0.0038 dB worst |
| Digital at 4x 44.1 kHz vs ngspice | 0.095 dB to 10 kHz, 0.239 dB to 20 kHz |
| Digital at 4x 48 kHz vs ngspice | 0.080 dB to 10 kHz, 0.199 dB to 20 kHz |
| Digital at 4x 96 kHz vs ngspice | 0.021 dB to 10 kHz, 0.047 dB to 20 kHz |
| Slider law error | ≤ 0.074 dB (the 6.4k cut end; others ≤ 0.02 dB) |
| Monotonic | yes |
| Off | bit-identical |
| Enable fade and slider sweep | no curvature beyond a steady +12 dB tone |
| Plugin path at the 200 Hz peak | matches the circuit within 0.05 dB |

**`SmokeTest` EQ checks** at 4x 48 kHz, against the circuit, within 0.1 dB:
- flat at 1 kHz: −0.50 dB;
- 800 +15 at 800 Hz: +13.14 dB;
- 100 −15 at 100 Hz: −15.75 dB;
- 6.4k +15 at 12.8 kHz: +11.83 dB;
- Level +6 at 1 kHz: +5.45 dB.
