# juicyboi — heavy-riff preamp (v2)

A desktop guitar preamp plugin / standalone app (e.g. guitar DI through a Focusrite
interface) built with JUCE 9.

**This is original modeling.** The DSP is built from general circuit
principles: an op-amp feedback-diode overdrive (green-overdrive family) feeding a
two-channel, cascaded-triode high-gain preamp (Rev E-era American dual-channel
style). No commercial plugin code, product values, or trademarks are used. All
coefficients are original starting points meant to be tuned.

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
        Boost (OD-style)  ── blended in/out with a 15 ms crossfade
          └─► Preamp cascade: Riff A ┐  both run; 20 ms crossfade
                              Riff B ┘  on channel switch
  └─► Tone stack (bass / mid / treble) + presence   (base rate)
  └─► Cab IR sim (optional; IR A / IR B blend)   (base rate)
  └─► Gate VCA (optional) ◄── key: dry input, tapped right after Input trim
  └─► Master
  └─► Bypass crossfade against latency-aligned dry signal
```

Buses: mono→stereo (default, guitar DI), mono→mono, stereo→stereo.
Latency: the oversampler's FIR latency (61 samples at 48 kHz), reported to the host.
The dry/bypass path uses the same delay.

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
| `od_drive`     | Boost Drive | 0 … 10           | 2       |
| `od_tone`      | Boost Tone  | 0 … 10           | 5       |
| `od_level`     | Boost Level | -24 … +12 dB     | 0 dB    |
| `channel`      | Channel     | Riff A / Riff B  | Riff B  |
| `pre_gain`     | Gain        | 0 … 10           | 6       |
| `pre_bass`     | Bass        | 0 … 10 (5 = flat)| 5       |
| `pre_mid`      | Mid         | 0 … 10 (5 = flat)| 4       |
| `pre_treble`   | Treble      | 0 … 10 (5 = flat)| 5.5     |
| `pre_presence` | Presence    | 0 … 10           | 5       |
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
    SoftClip.h             tanh, smooth diode-pair, asymmetric triode curve
    Od808Stage.h/.cpp      OD-style boost (feedback-diode topology principles)
    ChannelVoice.h         Red v1 / Red v2 / Orange voicing structs (stage corners, gains, bias)
    PreampCascade.h/.cpp   4× [coupling HP → gain → asym sat (inverting) → plate LP]
    ToneStack.h/.cpp       placeholder biquad TMB + presence shelf
    OversampledNonlinear.h juce::dsp::Oversampling wrapper (4x, FIR, integer latency)
    CabSim.h/.cpp          dual-mic IR convolution (4 slots, blend, hard bypass)
    NoiseGate.h/.cpp       key/sidechain gate (dry-input detector, VCA after cab)
Tests/
  SmokeTest.cpp            headless run through every channel/boost/gain combo
```

## DSP stage notes

- **Od808Stage**: OD-808 rev.1 BOM cues (4.7k/50n gain leg ≈677 Hz, R11=51k +
  500kA drive, C1=50p feedback LP, 1N914 pair, 220n/1k tone ≈723 Hz + 20kA
  treble restore, 100k Balance level). Feedback-clipper form
  `out = in + clip(gain·hp(in))` — original DSP, not a product clone.
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
- **Cab streamlined to 2 IR slots** — IR A / IR B drop zones + IR Blend (0 = A, 1 = B). Removed the 4-slot layout and the `cab_sm57_axis` / `cab_md421_axis` params; old sessions load fine (those values are ignored). `cab_mic_blend` ID kept. IR paths now save as `ir_a` / `ir_b`, so IRs from old 4-slot sessions need re-dropping once.
- **Red v1 / v2 A/B switch** — `red_voice` param (Preamp row, "Voice" box next to Channel; default **v2**). v1 = original tight Red; v2 = max-gain / fat-bottom Red (more cascade drive, Gain pot max 1.6, harder stage knees, tighter V2B HP to keep palm mutes crisp), roughly level-matched. Switching is a ~30 ms crossfade, so you can flip it while playing.
- **Red channel fattened** — eased input/mid high-passes (input 95→65 Hz), cascade drive up ~15–30%, Gain pot max 1.15→1.35 for more gain and bass before Boost.
- **Dual preamp** — Red and Orange channels, plus an **OD808** boost in front.
- **Gate** with sidechain detection.
- **Cab IR scaffold** (IR loading path in place; bundled IRs still minimal).
- **Standalone: Scarlett mono inputs** — patched JUCE Standalone so multichannel interfaces (e.g. Scarlett 4i4) show inputs 1/2/3/4 individually.
- **macOS microphone permission** — Standalone now ships `NSMicrophoneUsageDescription` so macOS prompts for input access.
- **Green/gold GUI** refresh.

Plugin identity (PLUGIN_CODE `Jcb1`) is unchanged, so hosts keep existing sessions.
