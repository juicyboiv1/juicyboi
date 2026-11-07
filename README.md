# juicyboi — heavy-riff preamp (v2)

A desktop guitar preamp plugin / standalone app (e.g. guitar DI through a Focusrite
interface) built with JUCE 9.

**This is original modeling.** The DSP is built from general circuit
principles: an op-amp feedback-diode overdrive (green-overdrive family) feeding a
two-channel, cascaded-triode high-gain preamp (Rev E-era American dual-channel
style), with a selectable 5150-inspired Lead voice. No commercial plugin code
is used; behavioral coefficients are original tuning, not an exact circuit solve.
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
| VST3       | `build/juicyboi_artefacts/Release/VST3/juicyboi.vst3` |
| Standalone | `build/juicyboi_artefacts/Release/Standalone/juicyboi` |
| LV2        | `build/juicyboi_artefacts/Release/LV2/juicyboi.lv2` |

The build also makes AU on macOS, and AAX on macOS/Windows (JUCE 9 ships a
bundled AAX SDK; you can point at a separate Avid tree with
`juce_set_aax_sdk_path` before `juce_add_plugin` if needed). No VST2.

| Format (macOS) | Path |
|----------------|------|
| AU | `build/juicyboi_artefacts/Release/AU/juicyboi.component` |
| VST3 | `build/juicyboi_artefacts/Release/VST3/juicyboi.vst3` |
| AAX | `build/juicyboi_artefacts/Release/AAX/juicyboi.aaxplugin` |

Install AAX to `/Library/Application Support/Avid/Audio/Plug-Ins/` (needs sudo).
Retail Pro Tools usually expects PACE-signed AAX; unsigned builds may not load
until you wrap/sign with Avid’s tools.

To install the VST3 on Linux, copy the bundle to `~/.vst3/`.

## Signal chain

```
Input trim
  └─► [ 4x oversampled ]
        Boost (OD-808 | SD-1) ── type + on/off crossfades (~15–20 ms)
          ├─► Rectifier Red v1/v2 / Orange ─► Rectifier TMB + presence
          └─► 5150-inspired Lead (6 stages) ─► Lead-specific TMB
  └─► Preamp Model crossfade (30 ms; after respective base-rate tones)
  └─► Graphic EQ (optional; 7 bands + Level, true bypass when off)   (base rate)
  └─► Cab IR sim (optional; IR A / IR B blend)   (base rate)
  └─► Gate VCA (optional) ◄── key: dry input, tapped right after Input trim
  └─► Master
  └─► Bypass crossfade against latency-aligned dry signal
```

Buses: mono→stereo (default, guitar DI), mono→mono, stereo→stereo.
Latency: the oversampler's FIR latency (61 samples at 48 kHz), reported to the host.
The dry/bypass path uses the same delay.

## Preamp Model

**Rectifier** (default) preserves Red v1/v2 and Orange, including all old
parameter IDs, indices, defaults and DSP coefficients. **5150-inspired** is
Lead-only: six gain stages mapped to V1A → V1B → V2A → V2B → V5B → V5A,
with a strongly asymmetric cold-stage approximation informed by V2A's 39 kΩ
cathode resistor. Distributed attenuation, bass cuts and high-frequency
rolloffs create a different compressed high-gain voice, not just output boost.
Use Gain/Bass/Mid/Treble as usual, then match perceived loudness with Master.
Channel, Red Voice and Presence are dimmed in Lead mode; their saved Rectifier
values remain untouched. The new `preamp_model` parameter is appended to the
layout, and states missing it explicitly restore Rectifier.

Scope: schematic-informed filter/shaper DSP at the existing **4×** rate, DC
rejection, sample-rate-aware filters and gain/tone smoothing. Filter frequencies
and normalized gains are behavioral tuning, not naive unloaded RC calculations.
The tone stack is a model-specific biquad approximation, not an interacting
passive network solve. Both complete model paths stay warm, with matching FIR
latency and even stage-inversion counts, and crossfade over 30 ms. Frequency-
dependent phase differences can still cause a brief timbral dip during a switch.
This costs additional CPU even when Rectifier is selected.

Reference: [public EVH 5150 schematic sheets, pp. 1–3](https://www.prowessamplifiers.com/schematics/post/peavey_evh5150.pdf).
These appear to describe the Peavey-era family; no precise production revision
or later 5150 III identity is claimed. Six pre-tone gain stages are distinct
from the downstream V3B follower/FX buffer, V3A recovery and V4A/B PI.
**Not modeled:** Rhythm/Bright/Crunch switching, FX loop/recovery, phase inverter,
power tubes, supply sag, dynamic grid/blocking behavior or NFB presence/resonance.
No schematic, IR or commercial model is bundled.

Headless coverage includes a captured 348fc68 Rectifier null fixture, old/new
state recall, rate/block/channel/Boost matrix, silence/DC, warmed switching,
gate/Standalone input regressions and offscreen GUI state/paint checks. See
[Tests/README.md](Tests/README.md). Real Scarlett hardware, macOS/AU/AAX hosting
and listening validation still require the user's machines.

## Boost section

Selectable front-end overdrive into the Dual Rec–style preamp. Shared **Drive /
Tone / Level / On**; **Type** picks the circuit. Original DSP of publicly
documented topology principles — not a product clone (no commercial branding).

- **OD-808** (default): symmetric feedback-diode mid hump (classic TS-family
  boost into high gain).
- **SD-1**: asymmetric 3-diode clip with Boutique Overdrive Mod RC cues (fatter
  coupling, 500 kΩ audio drive range, retuned mid, C6 omitted for an open top).
  Voiced slightly hot for surplus thickness into Dual Rec–style gain.
- Switching Type crossfades both circuits (state stays warm — no clicks).
- Tip: when A/B'ing Type, match perceived loudness with **Boost Level** (SD-1
  can hit a bit hotter at the same Drive).

## Graphic EQ section

Original 7-band graphic EQ in the spirit of classic octave-spaced pedal EQs
used to "carve after the drive" — not a clone of any commercial unit.
Sits **post-ToneStack / pre-Cab**, so cuts and boosts shape what hits the IR.

- Bands **100 / 200 / 400 / 800 / 1.6k / 3.2k / 6.4k Hz**, ±15 dB each
  (default 0). 100–3.2k are peaking biquads (Q 1.3, ~1 octave, overlapping
  like a gyrator EQ); **6.4k is a high shelf** (fizz / air control).
- **Level** ±15 dB (default 0) after the bands.
- **On** (`eq_enable`, default off). Off = 10 ms fade, then true bypass (no filtering).
- Faders: double-click resets to 0 dB. Band moves are slewed per block (no zipper).

Metal starting point: 100 −2, 200 −3, 400 −4, 800 +3, 1.6k +4, 3.2k +1, 6.4k −3 —
tighter low end, mid push for thickness into the cab, less fizz. Big boosts add
level: trim **EQ Level** (or **Master**) to keep the output out of the red.

## Cab (IR) section

Two-IR cabinet simulator after tone/presence, before Master.

- **Cab on/off** (`cab_enable`, default off). Off = hard bypass, no convolution
  runs — use this to feed an analog power amp + real cab.
- **Two IR slots** (IR A, IR B), loaded by drag-and-drop onto the GUI
  (.wav/.aif/.aiff/.flac, mono or stereo); "x" clears a slot.
- **IR Blend** 0 = 100% IR A, 1 = 100% IR B (linear wet crossfade). **Cab Level** ±24 dB.
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
| `input_trim`   | Input       | -24 … +24 dB     | 0 dB    |
| `od_enable`    | Boost On    | bool             | off     |
| `od_type`      | Boost Type  | OD-808 / SD-1    | OD-808  |
| `od_drive`     | Boost Drive | 0 … 10           | 2       |
| `od_tone`      | Boost Tone  | 0 … 10           | 5       |
| `od_level`     | Boost Level | -24 … +12 dB     | 0 dB    |
| `channel`      | Channel     | Red / Orange     | Red     |
| `red_voice`    | Red Voice   | v1 / v2          | v2      |
| `preamp_model` | Preamp Model | Rectifier / 5150-inspired Lead | Rectifier |
| `pre_gain`     | Gain        | 0 … 10           | 6       |
| `pre_bass`     | Bass        | 0 … 10 (5 = flat)| 5       |
| `pre_mid`      | Mid         | 0 … 10 (5 = flat)| 4       |
| `pre_treble`   | Treble      | 0 … 10 (5 = flat)| 5.5     |
| `pre_presence` | Presence    | 0 … 10           | 5       |
| `eq_enable`    | EQ On       | bool             | off     |
| `eq_100` … `eq_6k4` | EQ 100 Hz … EQ 6.4 kHz (`eq_100`, `eq_200`, `eq_400`, `eq_800`, `eq_1k6`, `eq_3k2`, `eq_6k4`) | -15 … +15 dB | 0 dB |
| `eq_level`     | EQ Level    | -15 … +15 dB     | 0 dB    |
| `cab_enable`   | Cab On      | bool             | off     |
| `cab_mic_blend`| IR Blend    | 0 … 1            | 0.5     |
| `cab_level`    | Cab Level   | -24 … +24 dB     | 0 dB    |
| `gate_enable`  | Gate On     | bool             | off     |
| `gate_threshold`| Gate Threshold | -80 … -20 dB (key envelope) | -60 dB |
| `gate_hold`    | Gate Hold   | 0 … 500 ms       | 60 ms   |
| `gate_release` | Gate Release| 1 … 500 ms (−60 dB fall time) | 60 ms |
| `master`       | Master      | -60 … +12 dB     | -6 dB   |
| `bypass`       | Bypass      | bool (host bypass param) | off |

All parameter IDs use version hint 1.

## Source layout

```
Source/
  PluginProcessor.h/.cpp   APVTS, bus layouts, processBlock wiring, state
  PluginEditor.h/.cpp      minimal rotary-knob editor attached to the APVTS
  Parameters.h/.cpp        parameter IDs + layout
  dsp/
    Filters.h              OnePole (RC-style 6 dB/oct) + RBJ Biquad, per-channel state
    SoftClip.h             tanh, diode-pair, diodeAsym3, asymmetric triode curve
    Od808Stage.h/.cpp      OD-808–style boost (symmetric feedback-diode)
    Sd1Stage.h/.cpp        SD-1–style boost (asym 3-diode, Boutique OD RC cues)
    ChannelVoice.h         Red v1 / Red v2 / Orange voicing structs (stage corners, gains, bias)
    PreampCascade.h/.cpp   4× [coupling HP → gain → asym sat (inverting) → plate LP]
    ToneStack.h/.cpp       placeholder biquad TMB + presence shelf
    GraphicEq.h/.cpp       7-band graphic EQ (6 peaks + 6.4k shelf) + Level, true bypass
    OversampledNonlinear.h juce::dsp::Oversampling wrapper (4x, FIR, integer latency)
    CabSim.h/.cpp          dual-mic IR convolution (4 slots, blend, hard bypass)
    NoiseGate.h/.cpp       key/sidechain gate (dry-input detector, VCA after cab)
Tests/
  SmokeTest.cpp            headless run through every channel/boost-type/gain combo
```

## DSP stage notes

- **Od808Stage**: OD-808 rev.1 BOM cues (4.7k/50n gain leg ≈677 Hz, R11=51k +
  500kA drive, C1=50p feedback LP, 1N914 pair, 220n/1k tone ≈723 Hz + 20kA
  treble restore, 100k Balance level). Feedback-clipper form
  `out = in + clip(gain·hp(in))` — original DSP, not a product clone.
- **Sd1Stage**: Boutique Overdrive Mod RC cues on an SD-1–topology path
  (R6=1k/C3=0.22u ≈723 Hz mid, R5=10k + 500kA drive, asym 3-diode clip,
  R7/C4 ≈1 kHz post LP, C6 omitted for open top, fatter ~8 Hz coupling).
  Same `out = in + asym_clip(gain·hp(in))` form; slight makeup for Dual Rec
  thickness. Original DSP, not a product clone.
- **PreampCascade**: tight HP → 4 triode-like stages, each with a coupling HP,
  a gain, an asymmetric soft-sat (positive half flattens harder, bias offset gives
  even harmonics, polarity inverts per stage), and a plate/Miller LP. The Gain pot
  (exponential taper) sits after stage 1. After the stages come a fizz LP,
  per-channel makeup gain, and a DC block.
- **ChannelVoice**: Riff A has fatter low-mids, less gain, and a warmer top.
  Riff B has a tighter interstage low cut, more gain, a brighter top, and the mid
  control centred higher. These are original voicing numbers.
- **ToneStack**: placeholder. It uses three non-interacting biquads plus a
  presence shelf, with per-channel centre frequencies.

## Next tuning steps

1. Replace the placeholder tone stack with an analytic passive TMB transfer
   function from your own component values (bilinear transform, interacting knobs).
2. Add dynamic tube effects: grid-conduction bias shift / blocking, cathode
   bypass dynamics, and supply sag. These are what give a high-gain amp its feel.
3. Tune stage gains, corners, and bias against measurements (frequency sweeps and
   level-dependent THD of reference circuits or SPICE sims you build).
4. Fit the OD clipper knee to a SPICE diode model (or use a WDF / Newton solve
   of the feedback clipper).
5. Add a power-amp stage with presence moved into its NFB loop. (Cab IR and the
   key/sidechain noise gate are now in.)
6. Smooth OD level and tone-coefficient changes per sample. Add an oversampling
   quality setting and presets.
7. Custom UI / LookAndFeel.

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

- **Graphic EQ** — 7-band (100 Hz … 6.4 kHz, ±15 dB) + Level ±15 dB, post-ToneStack / pre-Cab, off by default (true bypass). Vertical green/gold faders between Gate and Cab. New params only; existing sessions load unchanged (EQ off).
- **Cab streamlined to 2 IR slots** — IR A / IR B drop zones + IR Blend (0 = A, 1 = B). Removed the 4-slot layout and the `cab_sm57_axis` / `cab_md421_axis` params; old sessions load fine (those values are ignored). `cab_mic_blend` ID kept. IR paths now save as `ir_a` / `ir_b`, so IRs from old 4-slot sessions need re-dropping once.
- **Red v1 / v2 A/B switch** — `red_voice` param (Preamp row, "Voice" box next to Channel; default **v2**). v1 = original tight Red; v2 = max-gain / fat-bottom Red (more cascade drive, Gain pot max 1.6, harder stage knees, tighter V2B HP to keep palm mutes crisp), roughly level-matched. Switching is a ~30 ms crossfade, so you can flip it while playing.
- **Red channel fattened** — eased input/mid high-passes (input 95→65 Hz), cascade drive up ~15–30%, Gain pot max 1.15→1.35 for more gain and bass before Boost.
- **Dual preamp** — Red and Orange channels, plus a selectable **Boost** (OD-808 | SD-1) in front.
- **Gate** with sidechain detection.
- **Cab IR scaffold** (IR loading path in place; bundled IRs still minimal).
- **Standalone: Scarlett mono inputs** — patched JUCE Standalone so multichannel interfaces (e.g. Scarlett 4i4) show inputs 1/2/3/4 individually.
- **macOS microphone permission** — Standalone now ships `NSMicrophoneUsageDescription` so macOS prompts for input access.
- **Green/gold GUI** refresh.

Plugin identity (PLUGIN_CODE `Jcb1`) is unchanged, so hosts keep existing sessions.
