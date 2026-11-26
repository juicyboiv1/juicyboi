# Tuner pedal ("juicyboi TUNE", Rev3.2)

A chromatic pedal tuner sits first on the pedalboard, before the Boost. It has
two parameters, both appended after `preamp_out`, so older sessions load with
the tuner off:

| ID          | Name              | Range                 | Default |
|-------------|-------------------|-----------------------|---------|
| `tuner_on`  | Tuner             | bool (footswitch)     | off     |
| `tuner_ref` | Tuner Reference A | 436 … 445 Hz, 1 Hz steps (CAL knob; double-click = 440) | 440 Hz |

## Behaviour

- **On = output muted, TU-3 style.** The mute is applied at the very end of
  the chain (after Master and the bypass crossfade), so amp hiss and cab tails
  are silent too. The fade is a 10 ms ramp with a raised-cosine shape in and out,
  so it does not click.
- **Off = untouched.** Once the fade has settled, `Tuner::applyMute()` returns
  without touching the buffer. The output is bit-identical to Rev3.1 (tested
  against a processor that never used the tuner, and through the Rev3.1 fixtures).
- **What it listens to.** The raw input, folded to mono (the channel average),
  before Input Cal and the Boost. The amp settings do not affect the reading.

## Detection

`Source/dsp/PitchDetector.*` and `Source/dsp/Tuner.*`.

1. **Audio thread.** The input is decimated to ≤ 50 kHz, after a 4th-order
   Butterworth anti-alias filter. It is written into a lock-free ring of
   `std::atomic<float>`. Nothing is allocated on the audio thread, and nothing
   is computed there while the tuner is off.
2. **Analysis thread.** A low-priority `juce::Thread` runs one detection every
   25 ms while the tuner is on.
3. **YIN.** The method is de Cheveigné & Kawahara (2002). The difference
   function is computed by FFT over a frame of W = 2048 + maxLag = 4096 samples.
   The steps are:
   - cumulative-mean normalisation;
   - absolute threshold 0.15;
   - parabolic interpolation.
4. **Refinement.** The period is re-estimated on the normalised
   cross-correlation at the largest multiple of the period (up to 64 periods)
   that fits the lag range. This gives k-times finer resolution and does not
   depend on amplitude decay. It holds sub-cent accuracy from about 25 Hz
   (drop tunings) to about 2 kHz (the top frets).
5. **Silence gate.** Input below −70 dBFS gives no reading.
6. **Display smoothing.**
   - median of the last 5 readings, then a one-pole smoother (0.35) in cents;
   - a note change needs two agreeing frames;
   - the last note is held for 0.4 s after the signal stops.

## Validation (`Tests/TunerTest.cpp`, part of the smoketest)

**Accuracy.** Test tones:
- signals: sine, band-limited sawtooth, and a decaying "pluck" (per-harmonic
  decay, random phases);
- 17 notes from 27.5 Hz to 1975.5 Hz, covering E1 to B6 plus the drop/low-B range;
- each detuned −23 / 0 / +7 / +41 cents;
- sample rates 44.1, 48 and 96 kHz.

Every tone is detected. The worst error is 0.33 cents (limit ±1 cent).

**Display.**
- An E2 pluck at −4 cents with noise shows E2 −4.0 c, with 0.04 c jitter.
- With A = 442, a 440 Hz tone reads −7.83 c (expected −7.85).
- Silence clears the display.

**Plugin path.**
- An A2 pluck through the processor is detected within 0.01 c.
- With the tuner on, the output is exact zeros.
- The fade gain changes by at most 0.33 % per sample.
- With the tuner off, the output is bit-identical before it was ever used and
  again once the 10 ms fade-out has finished.

## GUI

The design is original and inspired by the general layout of compact pedal
tuners:
- a small gloss-black enclosure with a dark display;
- a 21-segment fan of vertical LEDs (5 cents each), taller toward the centre,
  with a centre needle bar;
- flat/sharp arrows;
- a large seven-segment note name with "#" and the octave;
- an "A=440 MUTE / BYPASS" line;
- a CAL knob, a footswitch and a red mute LED.

Segments are amber off pitch and turn bright green within ±2 cents. The pedal
uses no third-party names, logos or artwork. The label reads "juicyboi TUNE".
