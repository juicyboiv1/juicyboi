# Rev3.1 — component-level Boost tone / level (OD-808, SD-1)

Rev2.2 solved the op-amp clipping stage of both Boost pedals as a circuit
([diode-clippers.md](diode-clippers.md)). Rev3.1 (v3.1.0) replaces the last
behavioural parts, the Tone blend, linear Level gain and fixed output HP, with
the pedals' real post-clip networks: the passive post-clip LP, the active tone
stage, the Level pot and the emitter-follower output buffer. With this, both
Boost pedals are fully component-level. The amp paths are unchanged: with the
Boost off, every Rectifier / 5150 render is **bit-identical** to Rev3.

Rev3.1 keeps Rev3's identity: plugin name `juicyboi Rev3`, code `Jcb3`,
bundle ID `com.juicyboi.juicyboi-rev3`, app `juicyboi Rev3.app`. It installs over
Rev3 and saved state carries over. Parameter IDs and knob ranges are unchanged.
Original modelling of published topologies; no product is cloned.

## 1. Circuits and values

### OD-808 (TS-808)

Sources: Steve Cerutti's TS-808 drawing [C], ElectroSmash TS808 analysis [E],
R.G. Keen *The Technology of the Tube Screamer* [K] (geofex.com), Swell Amps
TS808 parts list [SA] (see diode-clippers.md for [C]/[E] links).

| Part | Value | Role | Source |
|------|-------|------|--------|
| R7 / C5 | 1 kΩ / 0.22 µF to ground | post-clip LP (723 Hz) | [C] [E] [K] |
| R9 | 10 kΩ to Vr | tone op-amp (+) bias (also loads the LP: −0.83 dB) | [C] [SA] |
| Tone | 20 kΩ "G" (W taper), (+) end … (−) end | active tone control | [C] "20KG", [E] |
| R8 / C6 | 220 Ω + 0.22 µF wiper to ground | tone network (3.3 kHz) | [C] [E] [K] |
| R10 | 1 kΩ output → (−) | tone op-amp feedback | [C] [SA] |
| R11 / C7 | 1 kΩ / 1 µF | into the Level pot | [C] [SA] |
| Level | 100 kΩ, cold lug to Vr | output level | [C] prints B100K, [SA] audio: see §3 |
| JFET bias | 510 kΩ to Vr at the wiper | bypass switch bias (load) | [C] |
| C8 / R13 | 0.1 µF / 510 kΩ to Vr | into the output buffer | [C] [SA] |
| Q2 / R14 | 2SC1815, 10 kΩ emitter | emitter follower | [C] [K] |
| R15 / C9 / R16 | **100 Ω / 10 µF / 10 kΩ** | output network (808 values; the TS9 has 470 Ω / 100 kΩ) | [C] [K] |

The 808 output network is used (100 Ω series, 10 kΩ to ground). The "TS9 → 808"
mod (470 Ω → 100 Ω, 100 kΩ → 10 kΩ, JRC4558) is therefore already what juicyboi
models. Into a 1 MΩ amp input the two networks differ by 0.04 dB in level and a
subsonic corner ([K]: divider 0.990 vs 0.995). The op-amp in both the clipper and
the validation netlists is the JRC/NJM4558 (3 MHz GBW, 100 dB).

### SD-1 (stock topology, "Boutique Overdrive" mod values)

Sources: schematicheaven SD-1 drawing [S], Wampler DIY "Boutique Overdrive" mod [W].

| Part | Stock [S] | Used [W] | Role |
|------|-----------|----------|------|
| R7 / C4 | 10 kΩ / 0.018 µF | **1 kΩ / 0.15 µF** to Vr | post-clip LP (1.06 kHz) |
| (+) bias | none | none | (the SD-1 has no R9 at the tone op-amp input) |
| Tone | 22 kΩ | **5 kΩ linear** | (+) end … (−) end |
| C5 / R8 | 0.027 µF + 470 Ω | **0.15 µF** + 470 Ω | wiper to ground |
| R9 | 10 kΩ | **1 kΩ** | tone op-amp feedback |
| C6 | 0.01 µF across R9 | **removed** | — |
| C7 / R10 | 1 µF / 4.7 kΩ | **22 µF / jumper** | into the Level pot |
| Level | 10 kΩ | **100 kΩ audio** | cold lug to Vr |
| JFET bias | 1 MΩ | 1 MΩ | at the wiper |
| R14 / C8 | 22 kΩ / 0.047 µF | **1 kΩ / 22 µF** | into the output buffer (1 MΩ base bias) |
| Q / RE | 2SC732, 10 kΩ | unchanged | emitter follower |
| R16 / C10 / pulldown | 1 kΩ / 1 µF / 100 kΩ | 1 kΩ / **22 µF** / 100 kΩ | output network |

## 2. Model

Everything after the clipper is linear, so each part is solved exactly.

**Tone stage** (ideal op-amp: v− = v+; the tone pot splits into rA = τ·Rpot from
the (+) end to the wiper and rN = (1−τ)·Rpot from the wiper to the (−) end; g0 =
1/Rpost + 1/Rbias, P = rA·rN + Rw·Rpot). Nodal analysis gives the biquad

```
           (1/Rpost) [ Rpot + s Cw (P + rA Rf) ]
H(s) = ---------------------------------------------------------------
       g0 Rpot + s (g0 Cw P + Cpost Rpot + Cw rN) + s^2 Cpost Cw P
```

Tone 10 (τ = 1) factors into the post LP times a treble shelf (1 + sCw(Rw+Rf)) /
(1 + sCwRw). Tone 0 (τ = 0) is a second-order LP.

**Level + output buffer.** With the Level pot split Ru = (1−v)·R, Rl = v·R and the
buffer as a small-signal unity follower (Af = Re_ac/(Re_ac + re), re = Vt/Ie,
input Rin = Rbase ‖ β·Re_ac, β = 300), the coupling caps C1 (C7) and C2 (C8) give
an exact biquad, and the output cap C3 (C9/C10) a first-order HP into Rpd ‖ 1 MΩ
(formulas in `Source/dsp/PedalTone.h`).

**Discretisation.** The bilinear transform (trapezoidal, SPICE's default "trap")
runs at the 4× rate. The pot-dependent sections run as a trapezoidal
state-variable filter (Simper TPT SVF). Its states are the integrator "capacitor"
currents, so coefficients can change every sample without zipper noise or
instability (a plain direct-form biquad stepped on knob jumps). Tone and Level
pot positions are smoothed per sample (20 ms), and coefficients are recomputed
only while a pot moves.

**Assumptions / not modelled.** Ideal tone op-amp: the 4558's 3 MHz GBW changes
the tone stage by ≤ 0.04 dB (ngspice, below). The emitter follower is treated as
small-signal (its 0.4 mA bias handles these levels). The bypass JFET's on-resistance
(~hundreds of Ω against 100 kΩ+) is not modelled. The amp input is a 1 MΩ load.
The clipper isn't loaded by the post LP; its output is the op-amp output.

## 3. Pot tapers

| Control | Taper used | Why |
|---------|-----------|-----|
| OD-808 Tone | "G" = W/S taper: two mirrored audio halves, 7.5 % rotation-resistance at 25 %, 50 % at 50 % | [C] prints 20KG; [E] calls it G (more control in the middle) |
| SD-1 Tone | linear | [W] specifies 5 k linear |
| Drive (both) | audio, unchanged from Rev2.2: R = Rpot·(10^(2x) − 1)/99 | diode-clippers.md |
| Level (both) | **knob law unchanged** (−24…+12 dB, default 0 dB). The wiper is set to the unloaded divider ratio v = 10^((dB − 12)/20), i.e. an ideal log taper over the knob's 36 dB. A fixed makeup gain, which isn't part of the circuit, restores the top 12 dB plus the pedal's in-band loss: OD-808 ×4.7643 (+13.56 dB), SD-1 ×3.8637 (+11.74 dB). | A real Level pot can only attenuate. Keeping the dB law keeps sessions and Rev3 levels. Buffer loading is modelled, so the law bends by a fraction of a dB. |

So **Level 0 dB (default) is the real pedal with its Level pot fully up** (+0.6 dB
on the OD-808). The makeups were set so the fixture guitar mix at Drive 2 / Tone 5
matches Rev3 exactly.

## 4. Validation against ngspice

`tools/pedaltone/od808-post.cir`, `sd1-post.cir`: the post networks (AC, real BJT
follower with a 2SC1815-class model and junction caps, Vr as an ideal source).
`od808-pedal.cir`, `sd1-pedal.cir`: the whole pedals (the clipper netlist
plus the post network, transient from the DC operating point).
`tools/pedaltone/plot_pedaltone.py build/juicyboi_pedal_compare <outdir>
[--export Tests/pedal-tone-ngspice-ref.txt]`. Model at 192 kHz (4× of 48 kHz).

| Check | OD-808 | SD-1 |
|-------|--------|------|
| Tone stage vs ngspice, Tone 0/2.5/5/7.5/10, 20 Hz–10 kHz | ≤ 0.076 dB | ≤ 0.075 dB |
| Whole post network (incl. BJT follower), 20 Hz–10 kHz | ≤ 0.061 dB | ≤ 0.056 dB |
| Same, 10–20 kHz (bilinear frequency warping) | ≤ 0.30 dB | ≤ 0.30 dB |
| 4558 (3 MHz) vs ideal tone op-amp | ≤ 0.04 dB (Tone 10) | ≤ 0.011 dB |
| Whole pedal transient, Drive 0/5/10 × 0.05/0.3/1.0 V, 1 kHz | rms err ≤ 0.19 %, worst sample 0.27 % of peak, harmonics ≤ 0.07 dB | rms err ≤ 0.25 %, worst sample 0.54 %, harmonics ≤ 0.23 dB |

Plots: `pedal-tone-od.png`, `pedal-tone-sd.png` (AC), `pedal-full-od.png`,
`pedal-full-sd.png` (transient). `Tests/PedalToneTest.cpp` re-checks the exact
derivation, the ngspice AC reference, that process() matches its coefficients
(0.0000 dB), Tone/Level jumps zipper-free (0.92× the steady step), and the
level against Rev3.

## 5. Rev3.1 vs Rev3 (what changes)

`tools/pedaltone/compare_rev3.py` (stage alone, 192 kHz) → `pedal-tone-vs-rev3.png`.

* **Level**: fixture guitar mix at Tone 5 is within 0.02 dB (OD) / 0.51 dB (SD-1)
  of Rev3 for Drive 0–10. Across Tone 2.5–7.5, the difference is within ±1.2 dB. At Tone 0 the real
  circuits are 1.3–1.8 dB quieter than Rev3's blend (intentional: the real dark
  setting). In the plugin, every boosted fixture stays within 0.25 dB of Rev3.
* **OD-808 tone**: identical below ~1 kHz at Tone 5. Above that it rolls off
  like the real circuit (Rev3's blend left a −12 dB treble floor): −3 dB at 3.7 kHz,
  −10 dB at 10 kHz. Rev3.1 Tone ≈ 7 sounds like Rev3 Tone 5 up to ~4 kHz. Tone 10 is
  the real treble shelf. THD at 220 Hz / 0.3 V: 13.4 → 13.9 % (Drive 2), 18.8 →
  19.4 % (Drive 10). Still purely odd.
* **SD-1 tone**: brighter. The Boutique-mod tone stage (5 k pot, 1 k feedback,
  C6 removed) never gets as dark as Rev3's invented double LP. At Tone 5: +9 dB
  at 3.7 kHz and +11 dB at 10 kHz. Even Tone 0 is brighter than Rev3's Tone 5
  above 3 kHz. THD rises 15.9 → 17.8 % (Drive 2), 19.9 → 23.8 % (Drive 5).
* **Lows**: the real output coupling caps reach well below Rev3's 15.9 Hz / 8 Hz
  output HP (corners ≈ 1–3 Hz OD; ≈ 0.07 Hz SD-1 with its 22 µF caps). After a hard
  asymmetric SD-1 clip, the output recovers its DC shift over seconds, like the
  pedal (inaudible; the amp's own coupling removes it).
* **CPU** (stage alone, stereo, 192 kHz): OD-808 124 → 151 ns/frame (+21 %),
  SD-1 134 → 158 ns/frame (+18 %), i.e. about +0.5 % of one core while a Boost
  is on. With the Boost off nothing runs (unchanged).

## 6. OD-808 "compression" diagnosis

Cory hears the OD-808 as more compressed than expected. Measured with
`juicyboi_boostprobe` and ngspice of the full 808:

1. **The pedal model matches the real circuit.** Output vs input, 330 Hz, Tone
   5, Level 0 dB, 0.01…1 V: model and ngspice agree within 0.1 dB at every level
   for Drive 0/2/5/10. A 20 dB input change gives 11.1 / 10.3 / 8.2 / 6.3 dB out on
   both, and Rev3 gives the same, so the Rev3.1 tone work did not change it. The
   clipper, Drive taper and op-amp model aren't adding compression. Slew isn't
   a factor: a 0.5 V, 5 kHz output needs 0.016 V/µs against the 4558's 1 V/µs.
2. **It's the gain into the amp.** At Level 0 dB (= the real Level dimed) the TS
   adds ~+13–16 dB of mid gain (even at Drive 0) in front of an amp that is
   already saturated. Into the amp, 110 Hz, 0.01 → 0.1 V (a decaying note):

```
C) plugin OD-808 into the amp: 110 Hz 0.01 -> 0.1 V and 2-tone 0.05 V each, vs Drive / Level
amp        front end                  dOut for +20 dB in  2-tone IMD [dB]
Red v2     no boost                               6.0 dB            -16.4
Red v2     Drive 2, Level 0 dB                    2.7 dB            -19.6
Red v2     Drive 0, Level 0 dB                    3.0 dB            -19.5
Red v2     Drive 2, Level -6 dB                   5.4 dB            -19.9
Red v2     Drive 2, Level -12 dB                 10.3 dB            -18.2
Red v2     Drive 5, Level 0 dB                    1.5 dB            -19.6
5150 Lead  no boost                               6.2 dB            -16.6
5150 Lead  Drive 2, Level 0 dB                    2.3 dB            -18.5
5150 Lead  Drive 0, Level 0 dB                    2.6 dB            -18.5
5150 Lead  Drive 2, Level -6 dB                   5.5 dB            -20.7
5150 Lead  Drive 2, Level -12 dB                 10.3 dB            -21.2
5150 Lead  Drive 5, Level 0 dB                    1.2 dB            -18.8
```

   Drive barely matters (Drive 0 vs 2: 3.0 vs 2.7 dB). **Level does.** Level −6 dB
   (knob 5) doubles the dynamics and still tightens the low end (2-tone IMD
   −19.9 dB vs −16.4 dB with no boost). Level −12 dB (knob 3.3) gives more
   dynamics than no boost and keeps it tight. That's how a TS is usually run
   into a high-gain amp (Level well below max).
3. **Makeup gain** only sets where Level 0 dB sits (the dimed pedal, which is
   Rev3's level). It isn't a compressor.
4. Rev3.1 also makes the OD-808 top darker at Tone 5 (real roll-off). For more
   clarity, use Tone 6.5–7.5.

**Non-stock mods (measured only, NOT in the plugin)**, pedal alone, Tone 5,
Level 0 dB (dOut for +20 dB in; bigger = less compressed):

```
A) pedal alone, Tone 5, Level 0 dB
variant                       Drive      dOut for +20 dB in     2-tone IMD [dB]
stock TS-808 (1N914 pair)         2      10.3 dB (out -5.1 dBV)        -25.4
stock TS-808 (1N914 pair)         5       8.2 dB (out -5.0 dBV)        -21.0
mod: 2+1 asymmetric 1N914         2      12.0 dB (out -3.1 dBV)        -24.3
mod: 2+1 asymmetric 1N914         5       9.1 dB (out -3.0 dBV)        -18.7
mod: 2+2 1N914                    2      13.4 dB (out -1.5 dBV)        -41.8
mod: 2+2 1N914                    5       9.9 dB (out -1.3 dBV)        -27.1
mod: red LED pair                 2      17.1 dB (out +2.2 dBV)        -84.2
mod: red LED pair                 5      13.5 dB (out +2.7 dBV)        -84.3
mod: bass C3 0.1 uF               2       8.4 dB (out -4.0 dBV)        -20.0
mod: bass C3 0.1 uF               5       7.4 dB (out -4.0 dBV)        -18.6

B) into the amp (default knobs), 110 Hz sine 0.05 -> 0.5 V and 2-tone E2+B2 0.15 V each
```

Pedal alone, more diode headroom opens it up a lot. 2+2 1N914 gives 13.4 dB vs
10.3 dB at Drive 2. Red LEDs give 17.1 dB with almost no clipping (IMD −84 dB),
but they are also ~7 dB louder. Into the default-gain amp, none of them changes
the amp's compression or IMD by more than ~1 dB, because the amp's saturation
dominates. Turning the Boost Level down helps far more. Asymmetric 2+1 adds
even harmonics and is slightly *less* tight (IMD +1 dB). The bass mod (C3
0.1 µF) is *more* compressed (8.4 dB) and muddier (IMD −20 vs −25 dB).
