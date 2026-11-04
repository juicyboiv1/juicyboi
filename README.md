# juicyboi — heavy-riff preamp (V1 scaffold)

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
  └─► Cab IR sim (optional; SM57 + MD421 blend)   (base rate)
  └─► Master
  └─► Bypass crossfade against latency-aligned dry signal
```

Buses: mono→stereo (default, guitar DI), mono→mono, stereo→stereo.
Latency: the oversampler's FIR latency (61 samples at 48 kHz), reported to the host.
The dry/bypass path uses the same delay.

## Cab (IR) section

Dual-mic 4x12 cabinet simulator after tone/presence, before Master.

- **Cab on/off** (`cab_enable`, default off). Off = hard bypass, no convolution
  runs — use this to feed an analog power amp + real cab.
- **Four IR slots**, loaded by drag-and-drop onto the GUI: SM57 On-axis,
  SM57 Off-axis, MD421 On-axis, MD421 Off-axis (.wav/.aif/.aiff/.flac, mono or stereo).
- **SM57 / MD421 axis** pickers choose which slot each mic uses.
- **Mic Blend** 0 = all SM57, 1 = all MD421. **Cab Level** ±24 dB.
- Empty slots: if a mic's selected IR is empty, the other mic gets 100%;
  if neither is loaded, the cab passes audio through unchanged.
- IR paths are saved in plugin state (absolute paths) and reloaded if the files still exist.
- Zero-latency convolution, so no extra latency is reported.

**No IRs are bundled.** Mesa/Boogie, Celestion Vintage 30, SM57 and MD421 are
trademarks of their owners; supply IRs you own or are licensed to use.

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
    ChannelVoice.h         Riff A / Riff B voicing structs (stage corners, gains, bias)
    PreampCascade.h/.cpp   4× [coupling HP → gain → asym sat (inverting) → plate LP]
    ToneStack.h/.cpp       placeholder biquad TMB + presence shelf
    OversampledNonlinear.h juce::dsp::Oversampling wrapper (4x, FIR, integer latency)
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
5. Add a power-amp stage with presence moved into its NFB loop, then cab IR
   loading (convolution) and a noise gate (essential for heavy riffing).
6. Smooth OD level and tone-coefficient changes per sample. Add an oversampling
   quality setting and presets.
7. Custom UI / LookAndFeel.
