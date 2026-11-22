# Headless regression tests

`cmake --build build --target juicyboi_smoketest -j8` then run
`build/juicyboi_smoketest_artefacts/Release/juicyboi_smoketest` (Linux).

**Rev2.2 fixtures.** Rev2.2 changes the audio again (solved Boost clippers,
post-TMB recovery on Red/5150, tone stacks inside the 4× section), so the same
renders were re-captured ONCE from the Rev2.2 build: `rect-rev2.2-clippers.f32`
(`legacyRegression`, which also prints each voice/boost's RMS vs the Rev2.1
render and requires |Δ| < 1.5 dB) and `lead-rev2.2-clippers.f32`
(`ResonanceTest` section 7). The Rev2.1 renders below are kept as level
references only. Capture code was removed again.

`ClipperTest.cpp` validates the Rev2.2 clippers: small-signal gain vs the
analytic linear network (< 0.1 dB), four stored ngspice transients
(`clipper-ngspice-ref.txt`, written by `tools/clippers/plot_clippers.py --export`),
diode clip levels / 2:1 asymmetry, stability at the 4× rate of 44.1/48/96/192 kHz
with hot/impulse/quiet/silent input (no NaN/denormals, Newton ≤ 12 iterations),
smoothed Drive/Level jumps, the recovery stage's unity small-signal gain, and a
CPU bound.

**Rev2 (2.1) fixtures.** Rev2 deliberately changes the audio (solved passive tone
stacks), so the regression renders were re-captured ONCE from the Rev2 build:
`rect-rev2-tonestack.f32` (73,728 floats, the same 9 Rectifier renders as the
original 348fc68 capture: Red v1/v2/Orange × Boost off/OD-808/SD-1, defaults,
48 kHz, mono, 256-sample blocks; `legacyRegression`) and
`lead-rev2-tonestack.f32` (40,960 floats, the 5 `LeadFixture.h` configurations;
`ResonanceTest` section 7). Tolerance 2e-6; local builds null exactly. The
capture code was removed again; do not regenerate these from later feature code
to bless a failure.

The pre-tone-stack renders stay in the repo as level references:
`legacy-348fc68.f32` (unmodified 348fc68) and `lead-62a1239.f32` (unmodified
62a1239). `ToneStackTest` compares the Rev2 default-knob RMS against them
(must be within 2 dB; currently −1.7…+1.7 dB).

`ToneStackTest.cpp` validates the passive TMB (`Source/dsp/ToneStack.h`):
- closed-form H(s) vs an independent nodal (MNA) solve, pot ends included
- digital vs analog H(jw): exact at the bilinear-warped frequency, and within
  1 / 0.25 / 0.06 dB to 8 kHz at 44.1-48 / 96 / 192 kHz, for all three part sets
  over a 5×5×5 knob grid; sines through `process()` match
- interaction (Mid changes the Bass knob's range, Bass moves 1 kHz, Treble moves
  500 Hz), all-max not flat and passive, usable knob ranges
- Jury stability at every knob extreme and rate, impulse tails decay to exact 0,
  rapid random automation incl. part-set switches: no NaN/Inf/denormals
- click-free knob jumps and Red/Orange switches (2nd-difference detector)
- default-knob voicing vs the Rev1 biquads, whole-plugin default level vs the
  old renders, whole plugin finite at tone extremes at 44.1/48/96/192 kHz

`PresenceTest.cpp` independently solves the declared loaded feedback-divider
equations, measures digital sine magnitude/phase at 54 points, checks the
bilinear-frequency mapping and raw analog-surrogate discrepancy, rapid sweeps,
reference null, silence/DC, missing/new state recall and model/EQ transitions.
This validates implementation against a documented nominal model, not against
real amp measurements. See `docs/5150-presence.md`.

`ResonanceTest.cpp` checks the 5150 Resonance stage (VR8 1M || C12 .0068u in
series with R56) against independent nodal equations at 90 points, including
Presence interaction, bit-exact reference at 0, sweeps, silence/DC, state
migration and model-crossfade isolation. Its whole-processor null (section 7) now uses
`lead-rev2.2-clippers.f32` (see above).

`LevelTaperTest.cpp` covers the amp-style Input/Master/Boost Level/Cab Level scales (`Source/LevelTaper.h`):
anchors (Input 5 = 0 dB, Master 6.5 = −6 dB, 7.5 = 0 dB, 10 = +12 dB, 0 = exact
mute), monotonicity, knob↔dB round trips, every legacy 0.1 dB step migrating to
the same gain, legacy (unversioned) state migration through
`setStateInformation` without double conversion on re-save, Master as a pure
post-chain gain on rendered audio, exact silence at 0, and gain smoothing.
Boost/Cab Level: anchors, exact unity at the defaults (incl. the Boost 20/3
detent), round trips, legacy dB grids, migration from both v1 and v2 states,
no re-conversion on re-save, and bit-identical rendering of a v1 default
session with Boost on.

`EditorTest.cpp` paints the editor offscreen, with no native window. It checks:
- the revision plate reads Rev2 / Rev2 · 5150 and the plugin name is `juicyboi Rev2`
- every parameter has a bound control with the same component ID, plus the IR A/B slots
- every head knob has a creme caption below it on both faceplates, with none above, and every head knob has a tooltip (Input/Master keep their dB equivalents)
- the Model toggle sets `preamp_model` and swaps the faceplates; Rectifier-only controls are hidden on 5150 and their values are kept
- Red Voice is disabled on Orange
- the drawer opens and closes with a fixed aspect ratio, and scaling works
- all 7 EQ bands plus Level are shown
- opening and closing the editor leaves the session state byte-identical

`Screenshots.cpp` builds `juicyboi_screenshots <dir> [scale]`, which writes PNGs of the Rectifier and 5150 faceplates with the drawer closed and open.
