# Rev2.3: more low end on Rectifier Red v2

Goal (from Cory's Rev2.2 test): more bass on **Red v2** only, without making
the high gain flubby or muddy; palm mutes must stay tight. Red v1, Orange and
the 5150 Lead must not change.

Result: a linear **power-amp / 4x12 load bump** after the Red recovery stage,
Red v2 only. +2.6 dB at 80 Hz, +3.0 dB at 100 Hz, +0.8 dB at 40 Hz, mids and
highs untouched (+0.07 dB at 1 kHz). The preamp cascade and every nonlinear
stage are bit-identical to Rev2.2. So is every other voice; the fixture nulls
check this with max error 0 on Linux.

Plot (outside the repo, same convention as the clipper plots): `red-v2-bass.png`.

## 1. Where Red v2 loses bass compared with the real amp

Source: Mesa/Boogie Dual Rectifier schematic (`Boogie_dualrectifier.pdf`).
Sheets used: preamp RF-1F, power amp, and the switching matrix LDR table.

The switching matrix sets these LDRs in **RD VINT** (Red Vintage):

| LDR | Function | RD VINT | RD MDRN |
|---|---|---|---|
| LDR3a/3b | V2a/V1a cathode caps | ON | ON |
| LDR10 | V3a cathode cap | ON | ON |
| LDR7 | treble rolloff | ON | OFF |
| LDR19 | power-amp feedback | ON | OFF |
| LDR20 | more feedback | OFF | OFF |

Red-channel low-frequency corners, real amp vs juicyboi Red v2 (Rev2.2):

| Stage | Real amp (schematic) | Red v2 cascade |
|---|---|---|
| Input / V1A | Grid 1M, no input cap. R291 1.8K, C31 1µF with R271 47K shorted by LDR3 (full bypass) | Input-tight HP 65 Hz, then stage-1 HP 30 Hz |
| V1A → V2A | C21 .02µ into R321 2.2M ∥ (R322 2.2M∥C10 82p + 1M Red Gain): **6.1 Hz** | Gain pot only |
| V2A | R292 1.8K, C32 1µF (LDR3 ON: full bypass) | Stage-2 HP 80 Hz |
| V2A → V2B | C22 .02µ, R242 470K + R212 1M: **5.4 Hz**. V2B cathode R103 39K is **unbypassed** (flat) | Stage-3 HP **150 Hz** |
| V2B → V3A | C27 .02µ into R106 330K (R225 220K grid stopper): **24 Hz** | Stage-4 HP 22 Hz |
| V3A | R293 1.8K, C33 1µF (LDR10 ON: full bypass) | (none) |
| TMB | Red set (C23/C24 .02µ, Bass 1M) | Same solved network |
| Recovery / PI | (FX loop, PI coupling) | HP 12 Hz |
| Power amp | NFB: FB → .1µF → R275 47K → R372 4.7K at the PI tail (LDR19 ON) | **not modelled** |

A fully bypassed 1.8K/1µF cathode is a low shelf, not a high-pass:
zero 1/(2π·1.8k·1µ) = 88 Hz, pole (1+K)·88 Hz, with K = (µ+1)Rk/(rp+RL)
(12AX7: µ 100, rp 62.5k). The shelf floors at −4.8 dB (V1A, RL ≈ 183k),
−6.7 dB (V2A, RL ≈ 94k) and −4.3 dB (V3A, RL 220k).

Findings:

1. **Before the clipping stages**, Red v2 is much tighter than the
   schematic. The 150 Hz HP in front of V2B has no counterpart in the real
   amp (5.4 Hz, flat cathode), and neither does the 65 Hz input-tight HP. The
   80 Hz V2A HP is close to the real V2A shelf at 60–100 Hz.
2. **After the clipping stages** (V3A coupling, CF, TMB, recovery), juicyboi
   matches the schematic within 0.5 dB above 60 Hz. The model even omits the
   real V3A cathode shelf, which would cut bass. The TMB is the real network,
   and Bass 10 is already the real maximum (1M audio pot). The 12 Hz recovery
   HP costs only 0.1 dB at 80 Hz.
3. **Power amp and cab.** juicyboi has no power amp. A cab IR is normally
   captured with a low-impedance amp, so the bass bump a tube amp makes into a
   4x12's impedance peak is missing entirely. This is the biggest gap
   between "real amp into a real cab" and "Red v2 into an IR".

## 2. What was tried: schematic coupling corners before the clipping stages

These were measured with `juicyboi_bassprobe` (§4) and the palm-mute release
analysis. Driven = 0.1 FS sine; release = time from the mute until the
≤200 Hz envelope falls 20/30 dB below the note.

| Variant (Red v2, Bass 5) | small 80/100 Hz | driven 80/100 Hz | palm release −20/−30 dB | SMPTE IMD | sub-45 Hz after mute |
|---|---|---|---|---|---|
| Rev2.2 | — | — | 18.0 / 19.1 ms | −2.1 dB | −25.8 dB |
| V2B coupling 150 → 5.4 Hz (schematic) | +6.5 / +5.1 | **+0.6 / +0.3** | 22.0 / 25.5 ms | **+4.1 dB** | −21.7 dB |
| V2B 5.4 Hz + V2A real shelf (88/191 Hz) | +4.7 / +3.1 | +0.6 / +0.3 | 22.6 / 27.7 ms | +3.1 dB | −23.2 dB |
| Full schematic (shelves V1A/V2A/V3A, real couplings) | −0.9 / −1.8 | −0.6 / −0.6 | 15.5 / 22.8 ms | −0.9 dB | −27.9 dB |
| V2B 150 → 110 Hz | +1.9 / +1.7 | +0.3 / +0.2 | 18.5 / 19.6 ms | +2.8 dB | −24.5 dB |
| V2B 150 → 90 Hz | +3.0 / +2.5 | +0.4 / +0.3 | 18.9 / 20.2 ms | +4.0 dB | −23.9 dB |
| **Rev2.3: load bump after recovery** | **+2.6 / +3.0** | **+2.6 / +3.0** | **18.2 / 22.0 ms** | **−2.1 dB (=)** | **−27.9 dB** |

Restoring the schematic coupling caps before the clipping stages adds almost
no bass at the output (+0.3…0.6 dB), because saturation compresses it. It
does add the sub-bass "thump" a static waveshaper model gets from rectified DC
(the real amp has grid-conduction blocking dynamics that juicyboi does not
model). The result is up to 11 ms slower palm-mute release, 5–6 dB worse
SMPTE IMD and 2–4 dB more sub-45 Hz energy: exactly the flub Cory wants to
avoid. So Rev2.3 keeps
the Rev2.2 cascade corners (65 / 30 / 80 / 150 / 22 Hz) as a documented
deviation: they stand in for blocking recovery and keep the palm mutes tight.
The full-schematic cascade has less sub-bass, but it loses 80–100 Hz, has
worse SMPTE IMD and changes Cory's approved saturation voice, so it was not
taken.

## 3. The change: tube power amp into a V30 4x12 (SpeakerLoad.h)

The amp's output impedance Zo and the speaker impedance Zs form a divider.
Normalised to the voice-coil resistance at DC/HF:

    H = [Zs/(Zs+Zo)] / [Re/(Re+Zo)]
    Zs(s) = Re + Res·(s/(w0 Q)) / (s²/w0² + s/(w0 Q) + 1)
    =>  H(s) = (s²/w0² + s/(w0 Qn) + 1) / (s²/w0² + s/(w0 Qd) + 1)
        Qn = Q/(1+Res/Re),  Qd = Q/(1+Res/(Re+Zo)),  |H(w0)| = (1+Res/Re)/(1+Res/(Re+Zo))

| Part | Value | Source / status |
|---|---|---|
| f0 | 106 Hz | Impedance resonance of a Rectifier Oversized 4x12 with V30s (Redwirez sweep posted in the Fractal Audio forum thread "LF Resonant Frequency"; Recto 4x12 curves sit around 106–111 Hz) |
| Re | 6.4 Ω | 8 Ω V30 4x12, DC resistance (nominal) |
| Res | 25 Ω (\|Z\|max ≈ 31 Ω) | Typical closed-box 4x12 peak (nominal, not a measured curve) |
| Q | 3 | Typical width of a closed-box impedance peak (nominal) |
| Zo | 3.7 Ω (damping factor ≈ 2.2) | Red Vintage keeps power-amp NFB (LDR19 ON: FB → .1µF → 47K → 4.7K tail), so damping is moderate. Chosen for +3.0 dB at f0. Not measured. |

Response: 30 Hz +0.46, 40 Hz +0.81, 60 Hz +1.70, 80 Hz +2.55, 100 Hz +2.97,
106 Hz +3.00, 150 Hz +2.36, 200 Hz +1.52, 1 kHz +0.07 dB. Because of the
second-order shape, the boost sits in the 70–150 Hz "chug" band. Below 40 Hz,
where flub lives, it adds less than 1 dB.

Implementation:
- Bilinear transform prewarped at f0. Double-precision coefficients and state,
  running at 4×.
- Placed after `rectRecovery`, with weight `rvAmt·(1−chAmt)`, which is the Red
  v2 share of the Rectifier output. It follows the Red v1/v2 and Red/Orange
  crossfades without clicks.
- At weight 0 the sample passes through untouched, so Red v1 and Orange are
  bit-identical. The filter always runs, so its state stays warm.
- It is linear and time-invariant, so it adds no harmonics and no IMD.

Not modelled:
- Voice-coil inductance (the treble rise into a real speaker).
- The extra damping loss below about 31 Hz, where the .1µF NFB cap opens
  the loop. Both would add response outside the 70–150 Hz band asked for.
- Red Modern (no NFB) would show a larger bump. juicyboi's Red v2 is the
  Vintage-style voice.

## 4. Measurements (full plugin, 48 kHz, Gain 6, Cab/EQ/Gate/Boost off)

`juicyboi_bassprobe [csv-dir]` (Tests/BassProbe.cpp) measures:
- **Small-signal response:** a 1e-5 FS sine.
- **Driven response:** a 0.1 FS sine.
- **Low-E THD:** 82.5 Hz at 0.25 FS.
- **Two-tone IMD:** E2+B2, 82.5 + 123.75 Hz at 0.1 each. It counts products on the 41.25 Hz grid that are not harmonics of either tone.
- **SMPTE-style IMD:** 82.5 Hz (0.16) + 2 kHz (0.04), sidebands 2k ± n·82.5.
- **Palm-mute chugs:** synthetic damped low-E, 8ths at 140 bpm, 150 ms hold then a 5 ms mute.

| Red v2 | small 80/100 (dB re in) | driven 80/100 (dBFS) | low-E THD (n ≥ −40 dBc) | 2-tone IMD re f1+f2 | 41 Hz diff tone | SMPTE IMD | palm release −20/−30 dB | palm 20–60 Hz re total |
|---|---|---|---|---|---|---|---|---|
| Rev2.2 Bass 5 | 17.4 / 20.6 | −12.4 / −11.9 | −8.7 dB (18) | −9.9 dB | −24.8 dBc | −2.1 dB | 18.0 / 19.1 ms | −31.1 dB |
| **Rev2.3 Bass 5** | 20.0 / 23.6 | −9.8 / −8.9 | −9.7 dB (15) | −11.7 dB | −26.7 dBc | −2.1 dB | 18.2 / 22.0 ms | −32.8 dB |
| Rev2.2 Bass 10 | 19.2 / 21.9 | −11.0 / −11.0 | −9.0 dB (17) | −10.2 dB | −20.9 dBc | −1.7 dB | 18.2 / 19.8 ms | −31.3 dB |
| **Rev2.3 Bass 10** | 21.7 / 24.9 | −8.5 / −8.0 | −10.1 dB (12) | −11.8 dB | −22.9 dBc | −1.7 dB | 18.5 / 19.9 ms | −32.8 dB |

- **Gain:** +2.55 dB at 80 Hz and +2.98 dB at 100 Hz, at both Bass 5 and
  Bass 10, small-signal and driven alike, because the bump is linear and comes
  after the clipping stages. 40 Hz: +0.8 dB. 1 kHz: +0.06 dB.
- **No extra flub:**
  - The ≤200 Hz palm-mute release to −20 dB is unchanged (+0.2/+0.3 ms).
  - The −30 dB point moves +2.9 ms at Bass 5 and +0.1 ms at Bass 10. The
    schematic-V2B variant was +4…+11 ms.
  - Sub-45 Hz energy goes down relative to the note, both during the note
    (−13.9 vs −11.8 dB) and after the mute (−27.9 vs −25.8 dB).
- **No IMD mud:**
  - SMPTE IMD is identical.
  - The 41 Hz difference tone drops 2 dB relative to the chord. In absolute
    terms it rises +0.8 dB, the bump's value at 41 Hz, while the chord notes
    rise +2.6/+2.9 dB.
- **THD:** THD relative to the fundamental reads 1.0 dB lower, and n(−40 dBc)
  drops by 3–5. This is only because the fundamental (82–110 Hz) is now
  2.6–3 dB louder. The harmonic levels themselves are unchanged or up to
  +1.5 dB (the 220 Hz H2). Every nonlinear stage is bit-identical to Rev2.2.

`juicyboi_harmonics` (110 Hz, matched knobs) shows the same effect: Red v2
THD re H1 sits about 2 dB below Rev2.2. Against Rev1 it is now −0.04…−0.38 dB
at Gain 5–10 and +0.35…+3.1 dB at Gain 3. Saturation itself is unchanged; the
reading moves only because the 110 Hz reference got the +2.9 dB bump.

Unchanged voices (smoketest): Red v1 and Orange (all Boost modes) and the
5150 configurations null against the Rev2.2 fixtures with max error 0
(`rect-rev2.2-clippers.f32`, `lead-rev2.2-clippers.f32`).

## 5. Reproduce

    cmake --build build --target juicyboi_bassprobe
    build/juicyboi_bassprobe_artefacts/Release/juicyboi_bassprobe /tmp/bass/after
    # (same file builds against Rev2.2 for the "before" CSVs / palm renders)
