# PITCH pedal (Rev3.3, v3.3.0)

A pitch shifter / whammy stomp, second on the board: **TUNE → PITCH → BOOST →
GEQ-7 → GATE → CAB**. Like the Boost, GEQ-7, Gate and Cab, each amp has its own
PITCH settings (Rectifier = bare IDs, 5150 = `m5150_` prefix). The board shows
the selected amp's set. Original artwork, no branding.

## Placement

The pedal runs at the base sample rate on the trimmed guitar (after the shared
Input Cal), in front of each amp's 4x Boost/preamp region. Pitch shifters track
a clean signal far better than distortion, which is why a real whammy goes
first. The tuner still listens to the raw, unshifted input. Each gate's key
follows the shifted signal, so it opens and closes with what you hear.
An amp you can't hear skips its PITCH engine, the same way the solved cascades
are skipped.

## Controls (slim, TUNE-style enclosure, 104 px wide)

| Control | Parameter | Range | Default |
|---|---|---|---|
| Footswitch + LED | `pitch_on` | on / off | off |
| SHIFT | `pitch_shift` | −24 … +24 semitones (1 st steps) | +12 |
| DETUNE | `pitch_detune` | −50 … +50 cents | 0 |
| MIX | `pitch_mix` | 0 … 100 % shifted | 50 % |
| SWEEP | `pitch_sweep` | 0 (heel) … 1 (toe), continuous | 1 (toe) |
| WHAM button | `pitch_whammy` | on / off | off |
| TRANS button | `pitch_transpose` | on / off | **on** |
| HARM button | `pitch_harmony` | on / off | off |
| POLY button (amber LED) | `pitch_poly` | on / off | off |

With the defaults, stomping the pedal gives a clean octave up. Every control
has a host-automatable parameter. SWEEP is meant for an expression pedal or
MIDI CC via your host's MIDI learn, and is smoothed, so 7-bit CC steps don't
zipper.

**Slim layout.** All the controls fit in a TUNE-sized enclosure (TUNE is 120 px,
PITCH is 104 px), so nothing had to be dropped. To fit, the knobs are 40 px
instead of the 48–56 px on the other pedals. They sit in a 2×2 grid (SHIFT /
DETUNE over MIX / SWEEP), and each has a live value readout under its caption,
for example "−2 st", "+7 ct", "60%". The four modes are small latching LED
buttons in a 2×2 grid labelled WHAM / TRANS / HARM / POLY. The full names are in
their tooltips. The title, footswitch and LED sit at the bottom like on every
other stomp.

To make room in the same 1100-wide drawer, the other pedals were tightened:
- gaps went from 10 px to 6 px;
- TUNE went from 128 to 120 px, BOOST from 214 to 184, GEQ-7 from 304 to 300,
  GATE from 200 to 160, and CAB from 148 to 136;
- the BOOST knobs went from 60 to 56 px, and the GATE knobs from 60 to 48 px.

The window size and aspect ratio are unchanged.

## How the toggles combine

```
interval (semitones) =
    Whammy or Transpose on:  (Whammy ? SWEEP : 1) × SHIFT  +  (Transpose ? DETUNE/100 : 0)
    both off:                0   → the dry signal passes (no engine runs)

output = Harmony ? (1 − MIX)·dry + MIX·shifted : shifted
engine = Poly ? phase vocoder (chords, ~43 ms) : grain shifter (single notes, ~5–12 ms)
```

- **Whammy only:** SWEEP heel = dry pitch, toe = SHIFT, halfway = half the
  interval (so +12 at 50 % is +6 st). DETUNE is ignored.
- **Transpose only:** a fixed drop or raise tuning of SHIFT + DETUNE, for
  example −1 st for E♭ standard, −2 st for D standard, or +5 st. DETUNE also
  allows non-semitone tunings such as −2 st −25 ct.
- **Whammy + Transpose:** Whammy sweeps SHIFT, and DETUNE adds on top.
- **Harmony:** adds the dry signal back (MIX 0 % = dry, 100 % = all shifted).
  With Harmony off the output is 100 % shifted.
- **Poly:** picks the engine. Switching crossfades the two engines over 30 ms.
- **Every mode off, pedal on:** exact dry passthrough, bit-for-bit. Harmony
  and/or Poly on with no interval source (no Whammy or Transpose) also passes
  dry.
- **Interval of 0** (SHIFT 0 with no DETUNE, or the Whammy heel): the output is
  the exact dry signal with no latency. The engine keeps running underneath, so
  sweeping off the heel starts cleanly.

Every change is click-free: interval glides over 25 ms (in semitones), Mix
fades over 20 ms, the engine switch over 30 ms, and the dry/shifted park over
25 ms. During 14 abrupt moves plus 7-bit Sweep automation, the largest sample step
was 0.058 (Shift −12 → +24). The click threshold is 0.12.

## True bypass

Off means a 15 ms fade, after which `PitchPedal::process` returns without
touching the buffer. No DSP runs: the engine, pitch detector and delay lines are
all idle. Output is then bit-identical to v3.2.2. `Tests/rev3.2.2-bypass.f32`
was captured once from the v3.2.2 build (297dbd4) with
`Tests/BypassRender.h`, and four scenarios must reproduce it exactly with PITCH
off:
- Red v2 with cab and gate;
- Orange with SD-1, EQ and cab filters;
- 5150 Lead with OD-808, gate and cab HPF;
- Rhythm Crunch in Preamp Out.

They also reproduce it exactly with every PITCH knob and mode changed but the
pedal left off. Pre-Rev3.3 sessions load with both PITCH pedals off.

## Engines

### Mono: pitch-synchronous dual-delay grain shifter (`GrainShifter`)

Two read taps move through a delay line at (1 − ratio) samples per sample and
crossfade with sin²/cos² windows. The grain length W adapts to the pitch: it is
an **even** number of periods of the note being played. That way every tap jump
(W) and the half-grain spacing between the taps (W/2) are whole periods, so
splices and crossfades line up with the waveform. W is at least 10 ms, or 15 ms
beyond an octave up, and is at most about 40 ms.

The period comes from the tuner's own `PitchDetector` (YIN plus period
refinement). It runs here on a ~12 kHz decimated copy of the input every
~10.7 ms (window 256, lag 512, 45–1000 Hz), and only while the mono engine is
running. If there's no clear pitch (chords, noise), W is 15 ms. A tap takes a
new W only when it wraps, which is exactly when its gain is zero, so W changes
never click. Reads use 4-point Hermite interpolation. After a reset, each tap
fades in only once the line holds signal at its delay.

### Poly: peak-locked phase vocoder (`PhaseVocoderShifter`)

This is the Laroche–Dolson "rigid" pitch-shifting phase vocoder. In each frame,
spectral peaks (3-point maxima) and their regions of influence are moved as
blocks by the integer bin shift nearest each peak's exact new frequency. Each
region's phase is rotated by an accumulated (ω′−ω)·H, tracked from the previous
frame's region owner. This keeps the partials of a chord coherent with each
other and pitch-exact.

The FFT is 2048 points with a 256 hop (8× overlap, Hann/Hann) at 44.1–48 kHz,
and scales with the rate (4096 at 88.2–96 kHz, 8192 at 176.4–192 kHz) so time
and frequency resolution stay constant.

## Latency, and why it isn't reported to the host

Measured as the energy-centroid delay of a tone burst at 48 kHz:

| Engine | Interval | Latency |
|---|---|---|
| Mono | −2 st | 7.5 ms |
| Mono | −12 st | 6.2 ms |
| Mono | +12 st | 5.8 ms |
| Mono | +24 st | 8.5 ms |
| Poly | any | 2048 samples = **42.7 ms** at 48 kHz (46.4 ms at 44.1 kHz) |

The mono delay sweeps between 0.25 ms and W, and averages about W/2. It's
shorter on higher notes and longer on low drop-tuned notes (up to ~12 ms).

**Decision: PITCH latency is not added to `setLatencySamples`.** The host
latency stays 61 samples, exactly as in v3.2.2. Reasons:
1. In Standalone (live playing) latency reporting does nothing except shift
   the global bypass dry path. The guitar would be no earlier, and the sound
   would not change.
2. In a DAW, reported latency that changes with a stomp or with the Poly
   button forces the host to re-compensate live. Many hosts glitch, drop out
   or ignore the change mid-playback. A hardware whammy isn't compensated
   either.
3. With the pedal off, nothing changes. Bypass alignment and bit-identity stay
   exactly as before.

The trade-off: on a recorded DI re-amped through PITCH in a DAW, the shifted
part sits ~6–12 ms late (mono) or ~43 ms late (Poly). Nudge the track or print
it if that matters. Harmony's dry part isn't delayed: it's the lowest-latency
path for the player, but in Poly mode the harmony is a ~43 ms slap behind the
dry note.

## CPU

On one core at 48 kHz with 256-sample blocks (Linux x86-64; the Mac numbers will
differ):
- Pedal alone: mono 0.49 %, Poly 0.83 % (stereo-in Poly 1.69 %), off
  0.0001 %.
- Whole plugin with PITCH on: see the table below, from `juicyboi_dynamics cpu`
  with `JB_CPU_PITCH=mono|poly`.

| Whole plugin, mono in | PITCH off | PITCH mono | PITCH poly |
|---|---|---|---|
| Red v2 | 17.1 % | 18.0 % | 18.6 % |
| 5150 Lead | 24.6 % | 24.0 % | 23.8 % |

On Lead the pedal's cost is smaller than the run-to-run noise.

## Limitations

- **Mono engine:** best on single notes, riffs and power chords. Big
  full chords warble a little (grain splices can't line up with several
  periods at once). That's the classic whammy character, and Poly is the fix.
- **Poly engine:** about 43 ms latency. It smears very sharp pick attacks a
  little (phase-vocoder transient softening), and low notes closer than
  ~2 FFT bins (~47 Hz at 48 kHz) share a region.
- **No formant preservation** (like the hardware it imitates): big
  shifts sound chipmunky or growly.
- **Latency isn't reported to the host** (see above).
- The gate key follows the shifted signal.
- Built and tested only on Linux (Standalone + VST3). Check the GUI on the
  Retina display and the feel on macOS.

## Tests

`Tests/PitchShiftTest.cpp` runs inside `juicyboi_smoketest`, and
`juicyboi_pitchprobe` runs only these tests (set `JB_PITCH_ONLY=4,7` for
specific sections). They check:
- Parameter order (18 appended after `m5150_cab_lpf`), defaults, and that the
  host latency is unchanged.
- Old sessions load with the pedal off. Bit-identity vs v3.2.2 when off.
- Every interval from −24 to +24 semitones (12 of them) in both engines, within
  1 cent by the YIN detector **and** a fine spectral-peak search. Measured
  error: 0.01 cents worst for mono, 0.00 cents for poly. Level within 3 dB.
- A Poly power chord shifted +12: new partials are at −1.3 dB and the old
  ones are gone (−81 dB).
- Toggle logic: Detune, Whammy at half sweep, Whammy alone ignoring Detune,
  Whammy + Transpose, exact-dry cases, Harmony 0/50 % and off.
- True bypass after the fade.
- Click-free moves.
- 44.1, 48, 96 and 192 kHz × mono/poly × block sizes 1/13/256/4096, stereo,
  noise then silence: no NaNs or denormals, and the tail is exactly zero.
- Latency and CPU.
- In-processor per-amp independence, and that only the audible amp runs.

`EditorTest` checks that:
- the pedal sits between TUNE and BOOST and is no wider than TUNE;
- all six pedals fit inside the drawer walls without overlapping;
- every PITCH control is inside the pedal for both amps;
- no overlaps, every label and readout fits, and the buttons and LED are bound
  correctly.

Screenshots (`juicyboi_screenshots <dir>`) are written as `rev3.3-*.png`.
