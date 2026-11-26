# juicyboi — heavy-riff preamp (Rev3, v3.2.0)

A desktop guitar preamp plugin / standalone app (e.g. guitar DI through a Focusrite
interface) built with JUCE 9.

**This is original modeling.** The DSP is built from general circuit
principles: an op-amp feedback-diode overdrive (green-overdrive family) feeding a
two-channel, cascaded-triode high-gain preamp (Rev E-era American dual-channel
style), with a selectable 5150-inspired Lead voice. No commercial plugin code
is used; the gain stages use original behavioral tuning, not an exact circuit
solve. Since Rev2 the passive Treble/Bass/Mid tone stacks are solved as real
networks from the part values printed on the referenced schematics (see
*DSP stage notes → ToneStack* and [docs/tonestack.md](docs/tonestack.md)), and
since Rev2.2 the Boost clipping stages are solved op-amp/diode circuits with the
schematic part values ([docs/diode-clippers.md](docs/diode-clippers.md)).
Rev2.3 adds the low-frequency power-amp / 4x12 load interaction to Red v2
([docs/red-v2-bass.md](docs/red-v2-bass.md)).
Rev2.4 rebuilds the 5150 as Rhythm (Bright/Crunch) + Lead channels:
- real Pre Gain networks;
- Post Gain driving a power-section stage;
- a 4x12 load on which Resonance acts ([docs/5150.md](docs/5150.md));
- a separate full set of settings for each amp (knobs, boost, EQ, cab, gate),
  recalled when you switch amps ([docs/per-amp-state.md](docs/per-amp-state.md));
- a shared **Input Cal** trim (not on either faceplate) for the interface level;
- a global **Preamp Out** toggle (Full Amp vs Preamp Out) for a real amp's
  effects return ([docs/per-amp-state.md](docs/per-amp-state.md)).

**Rev3 (v3.0.0, new baseline)** replaces every preamp tube's behavioural
waveshaper with a solved 12AX7 stage (Koren plate law, Cohen/Hélie grid current,
coupling-cap bias shift / blocking), validated against ngspice to < 0.03 % RMS
of swing ([docs/triode.md](docs/triode.md)). Bass at 40/80/100 Hz stays within
~1 dB of Rev2.4. Rev3 is a new plugin (`juicyboi Rev3`, `Jcb3`) that replaces Rev2;
Rev2.4 is the git tag `rev2.4`. Next step: power-supply sag.

**Rev3.1 (v3.1.0)** makes both Boost pedals fully component-level: the post-clip
LP, active tone stage, Level pot and output buffer of the OD-808 (TS-808 values,
808 output network) and SD-1 (Boutique-mod values) are solved exactly and
validated against ngspice (≤ 0.08 dB to 10 kHz) ([docs/pedal-tone.md](docs/pedal-tone.md)).
Same plugin/app identity as Rev3 (`juicyboi Rev3`, `Jcb3`, `juicyboi Rev3.app`), so
it installs over Rev3 and state carries over. Boost off is bit-identical to Rev3.
Descriptive product names belong to their respective owners; no affiliation is implied.

## Build

Requirements: CMake ≥ 3.22, Ninja, g++ 14 (or clang), JUCE 9 at `/workspace/JUCE`
(override with `-DJUCE_DIR=...`). On Linux you also need the usual JUCE deps
(ALSA/JACK, X11, freetype, …).

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
# headless smoke test (sanity, latency, bypass alignment)
./build/juicyboi_smoketest_artefacts/Release/juicyboi_smoketest
```

Artifacts (Linux):

| Format     | Path |
|------------|------|
| VST3       | `build/juicyboi_artefacts/Release/VST3/juicyboi Rev3.vst3` |
| Standalone | `build/juicyboi_artefacts/Release/Standalone/juicyboi Rev3` |
| LV2        | `build/juicyboi_artefacts/Release/LV2/juicyboi Rev3.lv2` |

The build also makes AU on macOS, and AAX on macOS/Windows (JUCE 9 ships a
bundled AAX SDK; `-DJUICYBOI_AAX_SDK=/path` uses a separate Avid tree,
`-DJUICYBOI_AAX=OFF` skips AAX; see [docs/aax.md](docs/aax.md)). No VST2.

| Format (macOS) | Path |
|----------------|------|
| AU | `build/juicyboi_artefacts/Release/AU/juicyboi Rev3.component` |
| VST3 | `build/juicyboi_artefacts/Release/VST3/juicyboi Rev3.vst3` |
| AAX | `build/juicyboi_artefacts/Release/AAX/juicyboi Rev3.aaxplugin` |
| Standalone | `build/juicyboi_artefacts/Release/Standalone/juicyboi Rev3.app` |

The Standalone app is named after the major revision only: every 3.x is
`juicyboi Rev3.app` (Rev3.1 installs over Rev3; the version is in the bundle info). Rev3 has its own identity
(`juicyboi Rev3`, code `Jcb3`, bundle/AAX ID `com.juicyboi.juicyboi-rev3`, LV2
`urn:juicyboi:juicyboi-rev3`) and replaces Rev2 as the baseline: remove the old
`juicyboi Rev2` / `juicyboi` bundles when installing. Rev2 sessions do not load
into Rev3 (Rev2.4 = tag `rev2.4`, Rev1 = tag `rev1`).

### Rev1 and Rev2 side by side (history)

Rev2 is a separate plugin so both can sit in one session for A/B: product name
`juicyboi Rev2`, plugin code `Jcb2` (Rev1: `Jcb1`; this also sets the AU subtype,
VST3 class ID and AAX product ID), bundle ID / AAX identifier
`com.juicyboi.juicyboi-rev2`, LV2 URI `urn:juicyboi:juicyboi-rev2`, same
manufacturer code `Jcyb`. Rev1 is the git tag `rev1` (commit `2b6ad4c`); build it
from a separate worktree (`git worktree add ../juicyboi-rev1 rev1`) and install both
bundles side by side (AAX: `/Library/Application Support/Avid/Audio/Plug-Ins/`,
AU: `~/Library/Audio/Plug-Ins/Components/`). Hosts treat Rev2 as a different
plugin, so existing sessions keep their Rev1 instances (Rev1 must stay installed for
them). Rev2 keeps the same parameter IDs and ranges. Rev2.4 appends the 5150 set and uses state version 4 (older sessions migrate automatically; see docs/per-amp-state.md).

Install AAX to `/Library/Application Support/Avid/Audio/Plug-Ins/` (needs sudo).
Retail Pro Tools usually expects PACE-signed AAX; unsigned builds may not load
until you wrap/sign with Avid’s tools.

To install the VST3 on Linux, copy the bundle to `~/.vst3/`.

## Signal chain

```
Tuner tap (raw input, folded to mono; analysis on a background thread)
Input trim
  └─► [ 4x oversampled ]
        Boost (OD-808 | SD-1, solved op-amp diode clippers) ── type + on/off crossfades (~15–20 ms)
          ├─► Rectifier Red v1/v2 / Orange ─► passive TMB (RF-1F parts, Red/Orange set) + presence ─► recovery stage (Red)
          │                                  ─► Red v2 only: power-amp / 4x12 load bump (Rev2.3, linear)
          └─► 5150-inspired Lead (6 stages) ─► passive TMB (5150 sheet-3 parts) ─► recovery stage
        GEQ-7 graphic EQ (per model, solved circuit, Rev3.2: inside the 4x region)
  └─► 5150: Lead Presence ─► Lead Resonance   (base rate)
  └─► Preamp Model crossfade (30 ms; after respective EQ / Presence paths)
  └─► Cab IR sim (optional; IR A / IR B blend)   (base rate)
  └─► Gate VCA (optional) ◄── key: dry input, tapped right after Input trim
  └─► Master
  └─► Bypass crossfade against latency-aligned dry signal
  └─► Tuner mute (10 ms fade while the tuner is on; untouched when off)
```

Buses: mono→stereo (default, guitar DI), mono→mono, stereo→stereo.
Latency: the oversampler's FIR latency (61 samples at 48 kHz), reported to the host.
The dry/bypass path uses the same delay.

## Input Cal, Master, Boost Level & Cab Level (amp-style 0–10)

All four level knobs read **0.0–10.0** like an amp (one decimal); hover a knob to see the
equivalent dB. The sound for a given dB level is unchanged — only the scale moved.

- **Input Cal** (`input_trim`) — linear in dB, **4.8 dB per number**:
  `dB = −24 + 4.8 × knob`. **5 = unity (0 dB)**, the default. This is a
  **shared interface-level calibration** used by both amps, not a preamp
  gain knob (a real Rectifier or 5150 has no Input control). Set it once
  for your interface (e.g. a Scarlett 4i4). It lives in a small **INPUT CAL**
  plate on the left of the pedalboard drawer front, always visible. Above 5
  hits the first stage harder (more saturation/compression); below 5 cleans
  it up. `m5150_input_trim` is kept in the parameter layout (stable host
  index) but is unused and hidden. A Rev2.4 session where the 5150 had a
  different Input now uses this shared value (the Rectifier's old Input).
- **Master** — smooth audio taper with a hard mute at 0:
  `t = knob / 10`, `dB = −60 + (9224 t − 2352 t² − 320 t³) / 91` for knob > 0,
  **knob 0 = silence**. Monotonic, ~10 dB per number at the bottom down to ~4 dB
  per number at the top. **6.5 = −6 dB (default)**, **7.5 = unity (0 dB)**,
  **10 = +12 dB**.

| Knob | Input (dB) | Master (dB) |
|-----:|-----------:|------------:|
| 0 | -24.0 | mute (silent) |
| 1 | -19.2 | -50.1 |
| 2 | -14.4 | -40.8 |
| 3 | -9.6 | -32.0 |
| 4 | -4.8 | -23.8 |
| 5 | +0.0 | -16.2 |
| 6 | +4.8 | -9.2 |
| 6.5 | +7.2 | **−6.0 (default)** |
| 7 | +9.6 | -2.9 |
| 7.5 | +12.0 | **0.0 (unity)** |
| 8 | +14.4 | +2.7 |
| 9 | +19.2 | +7.7 |
| 10 | +24.0 | +12.0 |

- **Boost Level** — linear in dB, **3.6 dB per number**: `dB = −24 + 3.6 × knob`
  (0 = −24 dB, 10 = +12 dB). **6.7 (exactly 20/3) = unity (0 dB)**, the default.
  Because 20/3 isn't exact in floating point, anything within 0.0001 dB of 0 is
  treated as exactly 0 dB, so the default is bit-identical to the old 0 dB.
- **Cab Level** — same law as Input: `dB = −24 + 4.8 × knob`, **5 = unity**, default 5.0.

| Knob | Boost Level (dB) | Cab Level (dB) |
|-----:|-----------:|------------:|
| 0 | -24.0 | -24.0 |
| 1 | -20.4 | -19.2 |
| 2 | -16.8 | -14.4 |
| 3 | -13.2 | -9.6 |
| 4 | -9.6 | -4.8 |
| 5 | -6.0 | **0.0 (unity, default)** |
| 6 | -2.4 | +4.8 |
| 6.7 (20/3) | **0.0 (unity, default)** | +8.0 |
| 7 | +1.2 | +9.6 |
| 8 | +4.8 | +14.4 |
| 9 | +8.4 | +19.2 |
| 10 | +12.0 | +24.0 |

EQ faders (±15 dB) and Gate controls (dB / ms) stay in their real units.

**Old sessions:** saved states carry `jb_state_version`. Version 1 (no
property; everything saved before 73125d5) stores Input, Master, Boost Level and
Cab Level in dB. Version 2 (73125d5) stores Input/Master as knobs but Boost/Cab
Level in dB. Current builds write version 3 and, on load, convert exactly the
dB-valued ones to the knob value with the same gain (old −60 dB Master, which
was already silent, becomes 0). Parameter IDs (`input_trim`, `master`,
`od_level`, `cab_level`) are unchanged. Host *automation lanes* recorded against
the old dB ranges: Input, Boost Level and Cab Level lanes play back identically
(the dB laws are linear across the same old ranges, so normalised positions are
unchanged); Master lanes will not match the old levels and should be re-recorded.

## Rev1 GUI (amp head + pedalboard drawer)

The editor is a vector-drawn amp head with a pedalboard drawer underneath. It's
all drawn in code with JUCE `Graphics`: procedural basketweave and tolex, and no
bitmap assets or commercial logos. The whole layout lives at a fixed design size
(1100 wide) and scales as one piece. Drag the corner to resize from 0.7× to 1.6×.
The aspect ratio is fixed, so everything stays crisp at any size.

**Head.** Black tolex with chrome corner caps and a blonde basketweave grille.
The **juicyboi** badge appears on the grille and nowhere else. The black control
panel has a small *Rev2* plate (the plate shows the build revision; the GUI design is the Rev1 amp head). Each knob has a small creme caption **below** it,
and nothing above the knobs. Hover any knob for a tooltip with its name and value. Input Cal, Master, Boost Level and Cab Level
tooltips also show the equivalent dB. A value bubble shows while you drag.
Double-click a knob to reset it to its default. The toggles carry small position
marks.

| Faceplate | Toggles (left → right) | Knobs (left → right) |
|-----------|------------------------|----------------------|
| **Rectifier** (blonde grille, *Rev2* plate) | Standby (ON / STBY = `bypass`), Model (RECT / 5150), Channel (RED / ORNG), Voice (V1 / V2, dimmed on Orange) | Gain, Bass, Mid, Treble, Presence, Master |
| **5150** (all-black, dark grille, *Rev2 · 5150* plate) | Standby, Model, Channel (RHY / LEAD), Bright and Crunch (Rhythm only, dimmed on Lead) | Rhy Pre, Lead Pre, Low, Mid, High, Rhy Post, Lead Post, Resonance, Presence |

The **Model** toggle is the `preamp_model` parameter, so this is still one
plugin. Flipping it fades between the faceplates in about 200 ms, and the knobs
move to their new positions during the fade. Controls that belong only to the
other model are hidden, but their saved values are kept. Neither faceplate has
an Input knob; the shared Input Cal trim sits on the drawer front.

**Pedalboard drawer.** Click the wooden drawer front (or its handle) to open or
close it. The window grows or shrinks to match, and the open/closed state and
window width are remembered while the plugin stays loaded. A small **INPUT CAL**
setup plate on the left of the drawer front is always visible (drawer open or
closed). It holds the shared `input_trim` calibration described above and a
**PREAMP OUT** toggle (Full Amp / Preamp Out). Preamp Out bypasses the cab,
the Red v2 and 5150 speaker-load models, and 5150 Presence/Resonance, and
keeps the preamp, boosts, tone stacks, post EQ and gate — for a real amp's
effects return. Those bypassed controls are greyed out while it is on.
Switching is a 25 ms crossfade. The output is the post-EQ preamp at unity
gain, a sensible FX-return line level (see `docs/per-amp-state.md`).
- **Boost**: green stompbox with Drive, Tone and Level, an OD/SD mini toggle
  (`od_type`), a footswitch for `od_enable`, and an LED.
- **7-band EQ**: creme pedal with all 7 faders (100, 200, 400, 800, 1.6k, 3.2k
  and 6.4k) plus Level, in ±15 dB. Double-click a fader for 0 dB. Footswitch for
  `eq_enable`, plus an LED.
- **Gate**: Threshold, Hold and Release, with a footswitch for `gate_enable`.
  The LED is green when the gate is open and orange-red when it's closed.
- **Cab** side panel: OFF/ON toggle (`cab_enable`), Blend and Level knobs, and
  IR A / IR B slots. Drag an IR file onto a slot or click it to browse; **x**
  clears the slot.

Every parameter keeps its ID, range, 0–10 taper and state migration, and every
control uses an APVTS attachment. Opening the editor never changes the session
state or the audio, and the editor tests check this.

## Preamp Model

**Rectifier** (default) preserves Red v1/v2 and Orange, including all old
parameter IDs, indices, defaults and cascade coefficients. Rev2 deliberately
replaces the tone-stack stand-in with the solved passive TMB (see *ToneStack*). **5150-inspired** is
Lead-only: six gain stages mapped to V1A → V1B → V2A → V2B → V5B → V5A,
with a strongly asymmetric cold-stage approximation informed by V2A's 39 kΩ
cathode resistor. Distributed attenuation, bass cuts and high-frequency
rolloffs create a different compressed high-gain voice, not just output boost.
Use Gain/Bass/Mid/Treble as usual, then match perceived loudness with Master.
Channel, Red Voice and **Preamp Presence** are hidden on the 5150 faceplate. Their saved
Rectifier values stay untouched. **5150 Presence/Resonance** appear only on the 5150 faceplate. The new `preamp_model` parameter is appended to the
layout, and states missing it explicitly restore Rectifier.

Scope: schematic-informed filter/shaper DSP at the existing **4×** rate, DC
rejection, sample-rate-aware filters and gain/tone smoothing. Filter frequencies
and normalized gains are behavioral tuning, not naive unloaded RC calculations.
The tone stacks (Rev2) are solved passive TMB networks with each model's own
schematic part values; everything else in this paragraph remains behavioral. Both complete model paths stay warm through independent
Graphic EQ histories and Lead-only Presence, with matching FIR latency and even
stage-inversion counts, then crossfade over 30 ms. Frequency-
dependent phase differences can still cause a brief timbral dip during a switch.
This costs additional CPU even when Rectifier is selected.

Reference: [public EVH 5150 schematic sheets, pp. 1–3](https://www.prowessamplifiers.com/schematics/post/peavey_evh5150.pdf).
These appear to describe the Peavey-era family; no precise production revision
or later 5150 III identity is claimed. Six pre-tone gain stages are distinct
from the downstream V3B follower/FX buffer, V3A recovery and V4A/B PI.
**Not modeled:** Rhythm/Bright/Crunch switching, FX loop/recovery, phase inverter,
power tubes, supply sag, dynamic grid/blocking behavior or speaker-dependent
feedback. The separate linear Presence/Resonance surrogates below do not add
those nonlinear power-amp behaviors.
No schematic, IR or commercial model is bundled.

Headless coverage includes Rev2 tone-stack Rectifier/5150 null fixtures, a
default-level comparison against the pre-tone-stack 348fc68 / 62a1239 renders, old/new
state recall, rate/block/channel/Boost matrix, silence/DC, warmed switching,
gate/Standalone input regressions and offscreen GUI state/paint checks. See
[Tests/README.md](Tests/README.md). Real Scarlett hardware, macOS/AU/AAX hosting
and listening validation still require the user's machines.

## Output Presence (5150 only)

A circuit-informed **small-signal closed-loop response approximation** sits
**after Lead Graphic EQ, before Cab**. It derives `T=A/(1+A*beta)` from the loaded
feedback divider: R56=39k, nominal R57=4.7k load, VR9=10k and parallel C8=.1u /
C62=.033u. It does not merely rename a generic shelf. Unknown forward-stage gain
is explicitly assumed to be 20 V/V, resonance is fixed bypassed, and active PI /
transformer / speaker loading is not solved. Pot taper is a documented nominal
two-decade law, not a verified factory taper. Two stable bilinear/TPT poles,
double-precision states and 30 ms resistance smoothing implement the relative
change from the reference response without running an audio feedback loop.

`lead_presence` is 0..10, default **0 = exact unchanged Lead tone**. Old feature-
branch states missing it restore 0. The Output Presence knob is Lead-only; the
existing Preamp Presence is Rectifier-only and remains bit-identical. Each has
its own saved value. The extra Lead EQ state prevents the new stage from
filtering the Rectifier contribution during a model crossfade.

At 48k, tests predict about +4.36/+5.19 dB at 1k/10k for position 5, and
+5.80/+7.99 dB for position 10, relative to position 0. A small relative dip
around 100 Hz is part of the normalized divider response, not added Resonance.
These are measurements of the digital surrogate, **not of an analog amp**.
See [derivation, sources, assumptions and test method](docs/5150-presence.md).
Match loudness with Master; high Presence plus boosted EQ can still exceed
0 dBFS. No limiter has been silently added to the legacy signal path.

## Output Resonance (5150 only)

Same declared closed-loop reduction as Output Presence, adding the 5150's
Resonance network: **VR8 1 MΩ audio (rheostat) in parallel with C12 .0068 µF,
in series with R56 39k**. Turning it up removes low-frequency negative feedback,
so the closed-loop gain rises below roughly 1/(2π·VR8·C12). Sits right after
Lead Presence (after Lead Graphic EQ, before Cab), and is one exact factor of the
same loop, so Presence and Resonance interact as in a single NFB path.

`lead_resonance` is 0..10, default **0 = VR8 shorted = exact unchanged Lead tone**
(bit-identical). States saved without it restore 0. Knob is Lead-only.
Assumptions: forward gain 20 V/V and a **resistive** load, so the result is a
broad low shelf (≈ +3.6 / +6.8 dB at 80 Hz relative to 1 kHz at 5 / 10), not
the speaker-impedance peak a real cab produces; no OT/speaker/damping model.
See [derivation, tables and tests](docs/5150-resonance.md).

## Boost section

Selectable front-end overdrive into the Dual Rec–style preamp. Shared **Drive /
Tone / Level / On**; **Type** picks the circuit. Original DSP of publicly
documented topology principles — not a product clone (no commercial branding).

- **OD-808** (default): TS-808 clipping stage solved per sample (4.7k/0.047µF
  gain leg, 51k + 500kA Drive, 51 pF, 1N914 pair, 4558-class op-amp) — symmetric
  mid-hump boost into high gain. Rev3.1: R7/C5 post LP, active tone stage
  (20k G-taper Tone, 220 Ω/0.22 µF, 1k feedback, 10k bias), 100k Level and the
  2SC1815 follower with the 808 output network (100 Ω / 10 µF / 10 kΩ) are solved too.
- **SD-1**: SD-1 clipping stage solved per sample with the Boutique Overdrive
  mod values (1k/0.22µF leg, 10k + 500kA Drive, no feedback cap) and the stock
  asymmetric diodes (2 one way, 1 the other). Rev3.1: 1k/0.15 µF post LP, 5k linear
  Tone with 470 Ω/0.15 µF, 1k feedback, C6 removed, 22 µF coupling, 100kA Level, follower.
- Level 0 dB (default) = the real pedal with its Level pot fully up. Into a
  high-gain amp, Level −6 to −12 dB keeps more dynamics ([docs/pedal-tone.md](docs/pedal-tone.md) §6).
- Values, sources, assumptions and ngspice validation: [docs/diode-clippers.md](docs/diode-clippers.md) (clippers),
  [docs/pedal-tone.md](docs/pedal-tone.md) (tone/level/output).
- Switching Type crossfades both circuits (no clicks); Drive/Tone/Level are smoothed per sample.
- Tip: when A/B'ing Type, match perceived loudness with **Boost Level** (SD-1
  can hit a bit hotter at the same Drive).

## Graphic EQ section (GEQ-7, Rev3.2)

A component-level model of the classic 7-band gyrator pedal EQ (GE-7-style
circuit, published part values, op-amp bandwidth and output follower included).
In the UI it is the "juicyboi GEQ-7"; there is no affiliation. It sits
**post-ToneStack / pre-Cab**, so cuts and boosts shape what hits the IR. Full
details: [docs/graphic-eq.md](docs/graphic-eq.md).

- **Bands.** 100 / 200 / 400 / 800 / 1.6k / 3.2k / 6.4k Hz, ±15 on the slider
  (default 0). The 6.4k band is the circuit's shelf-like top leg.
- **Slider scale.** The slider is dB-calibrated: ±15 is the end of the wiper
  travel, and in between the band peak is (value/15) × that band's maximum
  (about ±13.7 to ±15.9 dB; ±12.2 dB for 6.4k). The bands are broad and
  interact like the hardware. With every slider at 0 the circuit loses about
  0.5 dB, as the pedal does.
- **Level.** ±15 on the slider (about ±14.9 dB at the ends).
- **On** (`eq_enable`, default off). Off = 10 ms fade, then true bypass.
- **Faders.** Double-click resets to 0. Wiper moves are smoothed per sample, so
  they don't zipper.

Metal starting point: 100 −2, 200 −3, 400 −4, 800 +3, 1.6k +4, 3.2k +1, 6.4k −3.
This gives a tighter low end, a mid push for thickness into the cab, and less
fizz. Big boosts add level: trim **EQ Level** (or **Master**) to keep the output
out of the red.

## Tuner pedal (Rev3.2)

First on the pedalboard. Press the footswitch to tune: the output mutes with a
10 ms fade, and the display shows the note with a 21-segment cents fan. The
segments turn bright green within ±2 cents. The CAL knob sets the reference A
(436–445 Hz, double-click = 440). Off, the signal is untouched. See
[docs/tuner.md](docs/tuner.md).

## Cab (IR) section

Two-IR cabinet simulator after tone/presence, before Master.

- **Cab on/off** (`cab_enable`, default off). Off = hard bypass, no convolution
  runs — use this to feed an analog power amp + real cab.
- **Two IR slots** (IR A, IR B), loaded by drag-and-drop onto the GUI
  (.wav/.aif/.aiff/.flac, mono or stereo); "x" clears a slot.
- **IR Blend** 0 = 100% IR A, 1 = 100% IR B (linear wet crossfade). **Cab Level** 0–10 (5 = 0 dB, 4.8 dB per number; ±24 dB).
- Empty slots: if one slot is empty, the loaded IR gets 100%;
  if neither is loaded, the cab passes audio through unchanged.
- IR paths are saved in plugin state (absolute paths) and reloaded if the files still exist.
- Zero-latency convolution, so no extra latency is reported.

**No IRs are bundled.** Mesa/Boogie, Celestion Vintage 30, SM57 and MD421 are
trademarks of their owners; supply IRs you own or are licensed to use.

## Gate section

Key/sidechain noise gate for tight high-gain riffing. Original DSP that follows
general key + VCA gate principles: a clean detector driving a VCA placed after
the gain. It was inspired by the public PedalPCB Muzzle (THAT4301-class
VCA/RMS) gate architecture and the common two-knob-plus-hold control layout
of popular guitar gates. It is not a clone of any commercial product, and no
schematic is bundled.

- **Key = dry guitar.** The detector taps the signal right after Input trim,
  before Boost and the preamp. Its envelope follows your picking dynamics
  instead of the hiss floor of the distorted signal, so the threshold behaves
  the same whatever the Gain setting is.
- **Mute = after Cab, before Master.** The VCA silences preamp hiss and cab
  noise along with the note tail. (With Cab off, it sits right after tone/presence.)
- Detector: 20 Hz DC block → rectify → peak follower (0.1 ms attack, 8 ms release),
  4 dB hysteresis (closes 4 dB below the open threshold) to prevent chatter.
- Gain law: Open → Hold → Closing → Closed. Fixed 0.5 ms open ramp (no click).
  Release falls 60 dB over the Release time, then snaps to 0 below −80 dB.
- Because the key is taken before the oversampler, it runs ~61 samples
  (~1.3 ms @ 48 kHz) *ahead* of the wet path. That works as free look-ahead,
  so pick attacks are never chopped.
- **Gate off = true bypass.** After a 10 ms fade, there is no multiply and no
  state change. Gate is off by default.
- GUI LED: green = open, orange-red = closed, dim = gate bypassed.

Tips: start with Threshold around −60 dB on a typical interface DI. Raise it
until the hiss between palm-mutes is gone. For staccato chugs, use short Hold
(20–60 ms) and Release (30–80 ms). For sustained chords, use longer Hold/Release.

## Parameters

| ID             | Name        | Range            | Default |
|----------------|-------------|------------------|---------|
| `input_trim`   | Input Cal   | 0 … 10 (5 = 0 dB, 4.8 dB/number); shared by both amps | 5 (= 0 dB) |
| `od_enable`    | Boost On    | bool             | off     |
| `od_type`      | Boost Type  | OD-808 / SD-1    | OD-808  |
| `od_drive`     | Boost Drive | 0 … 10           | 2       |
| `od_tone`      | Boost Tone  | 0 … 10           | 5       |
| `od_level`     | Boost Level | 0 … 10 (3.6 dB/number, 6.67 = 0 dB) | 6.67 = 20/3 (= 0 dB) |
| `channel`      | Channel     | Red / Orange     | Red     |
| `red_voice`    | Red Voice   | v1 / v2          | v2      |
| `preamp_model` | Preamp Model | Rectifier / 5150-inspired Lead | Rectifier |
| `lead_presence` | Output Presence (5150) | 0 … 10, reference-relative | 0 |
| `lead_resonance` | Output Resonance (5150) | 0 … 10 (VR8 0 Ω … 1 MΩ) | 0 |
| `pre_gain`     | Gain        | 0 … 10           | 6       |
| `pre_bass`     | Bass        | 0 … 10 (5 = flat, ±12 dB shelf @100 Hz on Rectifier) | 5 |
| `pre_mid`      | Mid         | 0 … 10 (5 = flat)| 4       |
| `pre_treble`   | Treble      | 0 … 10 (5 = flat)| 5.5     |
| `pre_presence` | Presence    | 0 … 10           | 5       |
| `eq_enable`    | EQ On       | bool             | off     |
| `eq_100` … `eq_6k4` | EQ 100 Hz … EQ 6.4 kHz (`eq_100`, `eq_200`, `eq_400`, `eq_800`, `eq_1k6`, `eq_3k2`, `eq_6k4`) | -15 … +15 dB | 0 dB |
| `eq_level`     | EQ Level    | -15 … +15 dB     | 0 dB    |
| `cab_enable`   | Cab On      | bool             | off     |
| `cab_mic_blend`| IR Blend    | 0 … 1            | 0.5     |
| `cab_level`    | Cab Level   | 0 … 10 (5 = 0 dB, 4.8 dB/number) | 5 (= 0 dB) |
| `gate_enable`  | Gate On     | bool             | off     |
| `gate_threshold`| Gate Threshold | -80 … -20 dB (key envelope) | -60 dB |
| `gate_hold`    | Gate Hold   | 0 … 500 ms       | 60 ms   |
| `gate_release` | Gate Release| 1 … 500 ms (−60 dB fall time) | 60 ms |
| `master`       | Master      | 0 … 10 audio taper (0 = mute, 7.5 = 0 dB, 10 = +12 dB) | 6.5 (= −6 dB) |
| `bypass`       | Bypass      | bool (host bypass param) | off |
| `preamp_out`   | Preamp Out  | Full Amp / Preamp Out (appended; default Full Amp) | Full Amp |
| `tuner_on`     | Tuner       | bool (on = output muted; appended Rev3.2) | off |
| `tuner_ref`    | Tuner Reference A | 436 … 445 Hz (appended Rev3.2) | 440 Hz |

All parameter IDs use version hint 1.

## Source layout

```
Source/
  PluginProcessor.h/.cpp   APVTS, bus layouts, processBlock wiring, state
  PluginEditor.h/.cpp      Rev1 editor: fixed-aspect scaled root (head + drawer)
  gui/Rev1Style.h/.cpp     palette, LookAndFeel (chrome/pedal knobs, faders, tooltips),
                           ParamSlider / ToggleSwitch / Footswitch parameter controls
  gui/AmpHead.h/.cpp       amp head, Rectifier/5150 faceplates + fade, cached static art
  gui/Pedalboard.h/.cpp    drawer: Boost, 7-band EQ, Gate pedals, Cab panel, IR slots, Input Cal, Preamp Out
  Parameters.h/.cpp        parameter IDs + layout
  dsp/
    Filters.h              OnePole (RC-style 6 dB/oct) + RBJ Biquad, per-channel state
    SoftClip.h             tanh, diode-pair, diodeAsym3, asymmetric triode curve
    DiodeClipper.h         solved op-amp + diode feedback clipper (Newton–Raphson, Rev2.2)
    Od808Stage.h/.cpp      OD-808 boost (TS-808 values, solved clipper)
    Sd1Stage.h/.cpp        SD-1 boost (Boutique-mod values, solved asym clipper)
    RecoveryStage.h        post-TMB recovery gain (Rectifier Red, 5150 Lead; Rev2.2)
    SpeakerLoad.h          Red v2 tube-amp / 4x12 impedance-peak low-end bump (Rev2.3)
    ChannelVoice.h         Red v1 / Red v2 / Orange voicing structs (stage corners, gains, bias)
    PreampCascade.h/.cpp   4× [coupling HP → gain → asym sat (inverting) → plate LP]
    ToneStack.h/.cpp       passive TMB: closed-form H(s), bilinear, nodal state-space realisation
                           (smoothed knobs/parts), + Rectifier presence shelf
    GraphicEq.h/.cpp       7-band graphic EQ (6 peaks + 6.4k shelf) + Level, true bypass
    OversampledNonlinear.h juce::dsp::Oversampling wrapper (4x, FIR, integer latency)
    CabSim.h/.cpp          dual-mic IR convolution (4 slots, blend, hard bypass)
    NoiseGate.h/.cpp       key/sidechain gate (dry-input detector, VCA after cab)
Tests/
  SmokeTest.cpp            headless run through every channel/boost-type/gain combo
  EditorTest.cpp           offscreen editor checks (bindings, faceplates, drawer, Rev2 plate)
  ToneStackTest.cpp        TMB vs nodal solve / analog H(jw), interaction, stability, clicks, levels
  ClipperTest.cpp          clippers vs analytic network + stored ngspice runs, stability, smoothing, CPU
  ClipperDump.cpp          juicyboi_clipper_dump (tools/clippers/plot_clippers.py)
  HarmonicsCompare.cpp     juicyboi_harmonics: THD / harmonic probe for Rev A/B at matched knobs
  SpeakerLoadTest.cpp      Rev2.3 load bump: analytic values, digital vs analytic, bypass, switch clicks
  BassProbe.cpp            juicyboi_bassprobe: LF response, low-E THD, 2-tone/SMPTE IMD, palm-mute flub
  ToneStackDump.cpp        juicyboi_tonestack_dump: CSV response for the plot script
tools/tonestack/
  tmb-red.cir / tmb-orange.cir / tmb-lead.cir   SPICE netlists (LTspice / ngspice), same parts
  plot_tonestack.py        digital vs analog (and ngspice) plots -> ../juicyboi-shots/tonestack-*.png
  Screenshots.cpp          juicyboi_screenshots <dir> [scale]: offscreen editor PNGs
```

## DSP stage notes

- **DiodeClipper / Od808Stage / Sd1Stage (Rev2.2)**: the op-amp clipping
  stage is solved every sample at 4×: single-pole 3 MHz op-amp, trapezoidal
  gain-leg and feedback caps, BDF2 op-amp integrator, Shockley diode strings
  (1N914 model IS 2.52 nA, n 1.752) solved by bracketed Newton–Raphson (mean
  2 iterations). TS-808 values: C2 1µ/R5 10k, R4 4.7k/C3 0.047µ, R6 51k + 500kA,
  C4 51p, 1N914 pair, R7 1k/C5 0.22µ. SD-1: stock topology + Boutique-mod values
  (C2 .055µ/R4 100k, R6 1k/C3 0.22µ, R5 10k + 500kA, no feedback cap, 2+1
  diodes, R7 1k/C4 0.15µ). ngspice: worst rms error 0.33 % (OD) / 1.1 % (SD-1)
  at 192 kHz. The Tone/Level stages after it are still the Rev2.1 behavioural
  ones. Full tables: [docs/diode-clippers.md](docs/diode-clippers.md).
- **RecoveryStage (Rev2.2)**: post-TMB recovery gain on Rectifier Red and the
  5150 Lead (stand-in for the FX-loop recovery / phase-inverter triodes): unity
  small-signal asymmetric triode curve, so the stack's insertion loss is made up
  with harmonics instead of clean gain. Brings Red v2 / Lead THD and harmonic
  count to at least Rev1 at matched knobs (see the doc, §6).
- **SpeakerLoad (Rev2.3, Red v2 only)**: the bass bump a tube power amp
  makes into a 4x12's impedance peak, which an IR captured with a low-impedance
  amp lacks. The divider Zs/(Zs+Zo) with a parallel-RLC speaker resonance
  reduces exactly to one peaking biquad. Values: f0 106 Hz (Recto Oversized V30
  sweep), Re 6.4 Ω, |Z|max ≈ 31 Ω, Q 3, and Zo 3.7 Ω (Red Vintage keeps the
  power-amp NFB, LDR19). Response: +2.6 dB at 80 Hz, +3.0 dB at 100 Hz, +0.8 dB
  at 40 Hz, flat above 1 kHz. It is linear and sits after the recovery stage,
  so the saturation is bit-identical to Rev2.2. Schematic coupling corners
  ahead of the clipping stages were measured and rejected (flub).
  [docs/red-v2-bass.md](docs/red-v2-bass.md).
- **PreampCascade**: tight HP → 4 triode-like stages, each with a coupling HP,
  a gain, an asymmetric soft-sat (positive half flattens harder, bias offset gives
  even harmonics, polarity inverts per stage), and a plate/Miller LP. The Gain pot
  (exponential taper) sits after stage 1. After the stages come a fizz LP,
  per-channel makeup gain, and a DC block.
- **ChannelVoice**: Riff A has fatter low-mids, less gain, and a warmer top.
  Riff B has a tighter interstage low cut, more gain, a brighter top, and the mid
  control centred higher. These are original voicing numbers.
- **ToneStack (Rev2)**: a real passive Treble/Bass/Mid network per model and
  channel (slope resistor into bass/mid caps, treble cap into the treble pot,
  bass rheostat into the mid pot, mid cap to the mid wiper). Its 3rd-order
  closed form (Yeh/Smith TMB form) is cross-checked against an independent nodal
  solve and ngspice. Runtime: nodal state-space model whose states are the
  capacitor companion sources (trapezoidal = bilinear, prewarped at 3 kHz), so
  steady state equals the bilinear transfer function exactly while knob moves act
  on charged caps like the real circuit. Knobs, parts (Red↔Orange) and makeup
  are smoothed over ~20 ms, the model is re-solved every 8 samples while moving
  and ramped per sample in between: no zipper noise or clicks. Since Rev2.2 it
  runs inside the 4× section (ahead of the recovery stage); prewarp clamps to fs/4.
  The network keeps its real insertion loss; a fixed makeup gain per part set
  sits after it. Part values and sources:

  | Part | Rectifier Red | Rectifier Orange | 5150 Lead |
  |------|---------------|------------------|-----------|
  | Source | Mesa/Boogie Dual Rectifier preamp sheet **RF-1F** (6-93) | same sheet, Orange stack | Peavey **EVH 5150** schematic, **sheet 3** (Tone Stack/PI), [link](https://www.prowessamplifiers.com/schematics/post/peavey_evh5150.pdf) |
  | Treble pot | TRBL 250K | TRBL 250K | VR5 HIGH 250K (B, linear) |
  | Treble cap | C4 680 pF | C5 500 pF | C14 470 pF ∥ C60 100 pF = 570 pF |
  | Slope resistor | R273 47K | R274 47K | R28 47K |
  | Bass cap | C23 .02 µF | C25 .02 µF | C18 .022 µF |
  | Mid cap | C24 .02 µF | C26 .02 µF | C21 .022 µF |
  | Bass pot | BASS 1M | BASS 1M | VR3 LOW 1M (A, audio) |
  | Mid pot | MID 25K | MID 25K | VR4 MID 50K (B, linear) |
  | Makeup after the stack | +5.0 dB | +6.0 dB | +2.8 dB |

  **Assumptions / not sourced:** the RF-1F sheet does not print pot tapers, so
  the Rectifier uses the 5150 sheet's laws (Bass audio, Treble and Mid linear);
  the audio law is modelled as 15 % resistance at mid-rotation for every Bass pot.
  RF-1F prints the bass/mid caps as ".02" (20 nF), used as printed (earlier
  juicyboi notes said 22n). A 0.1 % pot end resistance is modelled. Not modelled:
  source impedance of the driving stage, the load on the treble wiper (1M master
  on RF-1F), the RF-1F LDR8/LDR9 + R215 1M channel switching, and on the 5150 the
  R65 47K branch at the Low/Mid junction and the R24 33K / C59–R94 network at the
  drive node. The makeup values only restore the Rev1 level at default knobs;
  they never alter the network. Presence stays a separate stage (Rectifier
  preamp shelf; 5150 Output Presence/Resonance), as before.

## Next tuning steps

1. ~~Replace the placeholder tone stack with an analytic passive TMB transfer
   function (bilinear transform, interacting knobs).~~ **Done in Rev2**: solved
   passive TMB with the schematic part values (see *DSP stage notes*). Possible
   follow-up: add the driving-stage source impedance and the wiper load.
2. Add dynamic tube effects: grid-conduction bias shift / blocking, cathode
   bypass dynamics, and supply sag. These are what give a high-gain amp its feel.
3. Tune stage gains, corners, and bias against measurements (frequency sweeps and
   level-dependent THD of reference circuits or SPICE sims you build).
4. ~~Fit the OD clipper knee to a SPICE diode model (or use a WDF / Newton solve
   of the feedback clipper).~~ **Done in Rev2.2**: Newton-solved op-amp/diode
   clippers validated against ngspice. Follow-ups: solve the active Tone stage
   after the clipper; op-amp rails.
5. Add a power-amp stage with presence moved into its NFB loop (Rev2.3 has only
   the Red v2 low-frequency speaker-load bump; voice-coil treble rise, NFB-cap
   damping loss and Red Modern's no-NFB load are still open). (Cab IR and the
   key/sidechain noise gate are now in.)
6. ~~Smooth OD level changes per sample~~ (done in Rev2.2). Add an oversampling
   quality setting and presets.
7. Rev1 GUI polish after hands-on testing (see *Rev1 GUI*).

## Rev2.3 changelog

- **More low end on Rectifier Red v2** (only). A linear power-amp / V30 4x12
  load bump after the recovery stage gives +2.6 dB at 80 Hz and +3.0 dB at
  100 Hz (Bass 5 and Bass 10, small-signal and driven), +0.8 dB at 40 Hz and
  +0.07 dB at 1 kHz.
- Palm mutes stay tight: the ≤200 Hz release is unchanged within 0.3 ms at
  −20 dB, sub-45 Hz energy drops 2 dB relative to the note, and SMPTE IMD is
  identical. See [docs/red-v2-bass.md](docs/red-v2-bass.md) for the schematic
  comparison and the variants that were measured and rejected.
- Red v1, Orange and the 5150 Lead are bit-identical to Rev2.2 (fixture nulls,
  max error 0).
- Standalone app is now named with the version: **`juicyboi Rev2.3.app`**
  (Linux `juicyboi Rev2.3`). The plugin name stays `juicyboi Rev2` with the
  same IDs, so sessions keep working.
- New fixtures `rect-rev2.3.f32` and `lead-rev2.3.f32`; the Rev2.2 fixtures
  must still null for every voice except Red v2. New `SpeakerLoadTest` and
  `juicyboi_bassprobe`. Version 2.3.0.

## Rev2.2 changelog

- **Solved diode clippers.** OD-808 and SD-1 Boosts now solve the op-amp
  clipping stage (diode law, gain leg, feedback cap, op-amp GBW) per sample with
  the TS-808 values and the SD-1 Boutique-mod values; validated against ngspice
  (`tools/clippers/`). Boost Drive/Tone/Level are smoothed per sample; the
  clippers only run while the Boost is audible.
- **More gain where Rev1 had it.** New post-tone-stack recovery stage on
  Rectifier Red and the 5150 Lead; the tone stacks moved inside the 4× section.
  Red v2 / Lead THD is now +1.8 dB or more above Rev1 at every probed Gain/input
  level (Rev2.1 was 1.4–2.6 dB below), with the output level within −0.8…+0.6 dB
  of Rev2.1. Orange is unchanged.
- Default-knob level vs Rev2.1: Boost off −0.1…+0.7 dB, OD-808/SD-1 −0.2…0.0 dB,
  5150 −0.1 dB (boosted 5150 −1.4/−1.5 dB from recovery compression).
- Reference renders re-captured once (`rect-rev2.2-clippers.f32`,
  `lead-rev2.2-clippers.f32`); the Rev2.1 renders stay as level references.
- New `ClipperTest`, `juicyboi_clipper_dump`, `juicyboi_harmonics`. Version 2.2.0;
  still the *Rev2* plugin (`Jcb2`), plate unchanged.

## Rev2 changelog

- **Real passive tone stacks.** The three non-interacting biquads (and the 5150's
  fixed mid contour) are replaced by solved passive Treble/Bass/Mid networks with
  the part values printed on the Dual Rectifier RF-1F sheet (Red and Orange) and
  the EVH 5150 sheet 3 (Lead). Bass, Mid and Treble now interact like an amp: e.g.
  on Red the Bass knob's 80 Hz range is 12.0 dB at Mid 0 but 8.0 dB at Mid 10, and
  everything on 10 is not flat. Insertion loss is real; a fixed makeup per part set
  keeps the default-knob level within ±1.7 dB of Rev1 (Red −1.4…+1.3 dB, Orange
  −1.7…+1.7 dB across Boost off/OD-808/SD-1, 5150 Lead 0.0 dB). This intentionally
  changes the sound, so the reference renders were re-captured once
  (`rect-rev2-tonestack.f32`, `lead-rev2-tonestack.f32`); the old renders stay for
  the level comparison.
- **Installs next to Rev1.** New product name `juicyboi Rev2`, plugin code `Jcb2`,
  bundle ID / AAX identifier `com.juicyboi.juicyboi-rev2`, LV2 URI
  `urn:juicyboi:juicyboi-rev2`; version 2.1.0. Rev1 is tagged `rev1`.
- Panel plate reads *Rev2* / *Rev2 · 5150*. Parameter IDs, ranges, the 0–10 knob
  laws and state version 3 are unchanged; old sessions load with every setting.
- New `ToneStackTest`, SPICE netlists and a plot script under `tools/tonestack/`.

## Rev1 changelog

- **Rev1 amp-head GUI replaces the v2 editor.** It's a vector amp head with a blonde basketweave grille, black tolex, chrome corner caps, and chrome-skirted knobs and toggles on a black panel. A pedalboard drawer (Boost, all-7-band EQ, Gate, and the Cab panel) opens and closes. The front-panel Model toggle (`preamp_model`) fades to an all-black 5150 faceplate. The juicyboi badge is on the grille only, and a *Rev1* / *Rev1 · 5150* plate sits on the panel. Each head knob has a small creme caption below it (none above), and hover tooltips give its name and value, including the dB equivalents. The window resizes from 0.7× to 1.6× with a fixed aspect ratio. Nothing changes in the audio or the parameters: IDs, ranges, tapers, state version 3 and the reference renders are all identical. IRs can now also be loaded by clicking a slot.

## v2 changelog

- **Standalone: guitar input fixed** — stock JUCE Standalone starts with *Mute audio input* (Feedback Loop) ON and remembers it, so the Scarlett input reached juicyboi as silence. The patched Standalone now starts unmuted (setting stored under a new key, so an old saved "muted" is ignored), and sums every ticked interface input to one mono guitar signal → stereo out, so the guitar is heard whichever Scarlett input it's on. VST3/AU/AAX bus behaviour is unchanged.

### Standalone silent on Mac (Focusrite)

If Standalone was working and then goes silent after a rebuild, the usual cause is JUCE's *Mute audio input* still ON in a stale settings file, or the JUCE unmute patch not actually applied to `~/src/JUCE` (a plain `cmake --build` can rebuild the app against an unpatched header).

Settings file: `~/Library/Application Support/juicyboi.settings`  
Keys: `audioSetup`, `juicyboiMuteInput` (default unmuted), `filterState`.

Recovery (Mac Mini paths):

```bash
bash ~/src/juicyboi/scripts/recover-standalone-mac.sh
```

Or manually: quit juicyboi → delete that settings file → `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR=~/src/JUCE && cmake --build build -j8 --target juicyboi_Standalone` → open the app → untick *Mute audio input*, tick Input 1, Direct Monitor off.

- **Rectifier Bass now works in the guitar range** — the FMV Bass shelf centre moved from a 40 Hz clamp (the old 1/(2π·1M·22n) = 7.2 Hz estimate) to **100 Hz**, range still ±12 dB. Bass 10 now lifts 80 Hz by about +8 dB vs Bass 5 (was +1 dB); Bass 0 cuts about −8 dB. **Bass 5 (default) is bit-identical** to before (null-tested), so default and Bass-5 sessions sound the same; sessions saved with Bass ≠ 5 will sound bassier/leaner than before.
- **Boost Level and Cab Level are amp-style 0–10 knobs too** — Boost Level 6.7 (20/3) = unity, 3.6 dB/number (0 = −24, 10 = +12 dB); Cab Level 5 = unity, 4.8 dB/number; hover for dB. State version 3; v1 and v2 sessions convert on load to the same level.
- **Input / Master are amp-style 0–10 knobs** — Input 5 = unity (4.8 dB/number), Master 0 = mute, 6.5 = −6 dB default, 7.5 = unity, 10 = +12 dB; hover for dB. Old sessions are converted on load to the same level (see *Input & Master*).
- **5150 Output Resonance** — `lead_resonance` (0–10, default 0 = unchanged), VR8 1M / C12 .0068 µF NFB network, Lead-only, next to Output Presence. See `docs/5150-resonance.md`.
- **Graphic EQ** — 7-band (100 Hz … 6.4 kHz, ±15 dB) + Level ±15 dB, post-ToneStack / pre-Cab, off by default (true bypass). Vertical green/gold faders between Gate and Cab. New params only; existing sessions load unchanged (EQ off).
- **Cab streamlined to 2 IR slots** — IR A / IR B drop zones + IR Blend (0 = A, 1 = B). Removed the 4-slot layout and the `cab_sm57_axis` / `cab_md421_axis` params; old sessions load fine (those values are ignored). `cab_mic_blend` ID kept. IR paths now save as `ir_a` / `ir_b`, so IRs from old 4-slot sessions need re-dropping once.
- **Red v1 / v2 A/B switch** — `red_voice` param (Preamp row, "Voice" box next to Channel; default **v2**). v1 = original tight Red; v2 = max-gain / fat-bottom Red (more cascade drive, Gain pot max 1.6, harder stage knees, tighter V2B HP to keep palm mutes crisp), roughly level-matched. Switching is a ~30 ms crossfade, so you can flip it while playing.
- **Red channel fattened** — eased input/mid high-passes (input 95→65 Hz), cascade drive up ~15–30%, Gain pot max 1.15→1.35 for more gain and bass before Boost.
- **Dual preamp** — Red and Orange channels, plus a selectable **Boost** (OD-808 | SD-1) in front.
- **Gate** with sidechain detection.
- **Cab IR scaffold** (IR loading path in place; bundled IRs still minimal).
- **Standalone: Scarlett mono inputs** — patched JUCE Standalone so multichannel interfaces (e.g. Scarlett 4i4) show inputs 1/2/3/4 individually.
- **macOS microphone permission** — Standalone now ships `NSMicrophoneUsageDescription` so macOS prompts for input access.
- **Green/gold GUI** refresh (superseded by the Rev1 GUI).

Plugin identity (PLUGIN_CODE `Jcb1`) is unchanged, so hosts keep existing sessions.
