# Headless regression tests

**Rev2.4 (per-amp settings + new 5150 engine).**
- `lead-rev2.4.f32` was captured ONCE from the Rev2.4 build with the five
  `LeadFixture` configs. In those configs, configs 0–2 now set the 5150's own
  `m5150_*` IDs. `ResonanceTest` section 7 requires:
  - the 5150 configs (0–2) to null vs Rev2.4;
  - Red v2 / Orange (3–4) to be **bit-identical** (error 0) to `lead-rev2.3.f32`;
  - Orange to be bit-identical to Rev2.2.
- `rect-rev2.3.f32` still nulls exactly (`legacyRegression`).
- `PerAmpTest.cpp` loads four sessions saved by the Rev2.3 build
  (`rev2.3-state-{0..3}.bin`). It checks:
  - the Rectifier renders bit-identically to what Rev2.3 rendered from them
    (`rev2.3-migration-rect.f32`);
  - the v3→v4 migration copies boost/EQ/cab/gate into the 5150 set and
    maps Gain→Lead Pre, Bass/Mid/Treble→Low/Mid/High, Master→Post;
  - there is no re-migration on re-save;
  - each amp's settings (except the shared `input_trim` calibration) have
    zero effect on the other amp's audio; `m5150_input_trim` is unused;
  - switching amps keeps both sets;
  - a session where the 5150 had a different Input uses the shared
    (Rectifier) `input_trim`.
- `Amp5150Test.cpp` checks:
  - the Pre Gain networks (time-domain vs analytic nodal magnitude, < 0.1 dB);
  - the Lead bright cap;
  - Rhythm Bright (+6 dB at 2 kHz between Pre 5 and 7) and Crunch gain;
  - a click-free Bright/Crunch switch;
  - that the 4x12 bump grows with Resonance;
  - the Post taper.
- `EditorTest`: the 5150 faceplate (no Master or Input; RHY/LEAD, Bright/Crunch,
  per-channel Pre/Post) and the per-amp pedalboard, plus the shared Input Cal
  trim and Preamp Out toggle on the drawer front (bypassed cab / Presence /
  Resonance controls grey out).
- `PreampOutTest.cpp`: `preamp_out` is last and defaults to Full Amp; Red v1 /
  Orange with cab off are bit-identical in Preamp Out; Red v2 and 5150 differ;
  cab / Presence / Resonance are silent in Preamp Out; the 25 ms switch is
  click-free; old sessions load as Full Amp; default RMS/peak is printed.
- Capture code was removed again. Do not regenerate fixtures from later
  feature code to bless a failure.

`cmake --build build --target juicyboi_smoketest -j8` then run
`build/juicyboi_smoketest_artefacts/Release/juicyboi_smoketest` (Linux).

**Rev2.3 fixtures.** Rev2.3 changes Red v2 only (power-amp / 4x12 load bump
after the recovery stage). The same renders were re-captured ONCE from the
Rev2.3 build: `rect-rev2.3.f32` and `lead-rev2.3.f32` are now the null
references. The Rev2.2 fixtures stay and must still null (< 2e-6; 0 on Linux)
for every configuration except Red v2: Red v1 and Orange × 3 boosts, and the
four 5150/Orange `LeadFixture` configs. Red v2 must be 0.3–3 dB louder than
Rev2.2 on the 82.4 Hz-heavy signal (measured +2.13 dB Boost off, +0.49 dB
boosted). `SpeakerLoadTest.cpp` checks:
- the analytic bump (+3.0 dB at 106 Hz, +2.4–2.8 at 80 Hz, +2.8–3.1 at 100 Hz, < +1 dB at 40 Hz, flat at 1/5 kHz);
- digital vs analytic gain within 0.02 dB at 4× of 44.1/48/96 kHz;
- the exact bypass, and that only Red v2 enables the bump;
- a click-free Red v1 → v2 switch.

Capture code was removed again. `juicyboi_bassprobe` (BassProbe.cpp) is the
measurement tool behind docs/red-v2-bass.md.

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
- every head knob has a creme caption below it on both faceplates, with none above, and every head knob has a tooltip (Master and the drawer Input Cal keep their dB equivalents)
- the Model toggle sets `preamp_model` and swaps the faceplates; Rectifier-only controls are hidden on 5150 and their values are kept
- Red Voice is disabled on Orange
- the drawer opens and closes with a fixed aspect ratio, and scaling works
- all 7 EQ bands plus Level are shown
- opening and closing the editor leaves the session state byte-identical

`Screenshots.cpp` builds `juicyboi_screenshots <dir> [scale]`, which writes PNGs of the Rectifier and 5150 faceplates with the drawer closed and open.

## Rev3.2

- **`TunerTest.cpp`**
  - Pitch within ±1 cent: sine, sawtooth and pluck; 27.5 Hz to 1.98 kHz;
    44.1, 48 and 96 kHz. The worst case is 0.33 c.
  - Display smoothing, reference A, note names and the silence hold.
  - TU-3 mute: exact zeros, gain step ≤ 1 % per sample.
  - Tuner off is bit-identical, both before it is used and after its fade-out.
- **`GraphicEqTest.cpp`**
  - The GEQ-7 circuit against `geq7-ngspice-ref.txt`, generated by
    `tools/geq7/geq7_ngspice.py --export`:
    - analytic model ≤ 0.01 dB;
    - digital at 4x ≤ 0.1 dB to 10 kHz and ≤ 0.25 dB to 20 kHz.
  - Slider law, true bypass, click-free fade and sweep, and CPU.
- **`lead-rev3.2.f32` and `rev3.2-migration-rect.f32`** were captured ONCE
  from the Rev3.2 build with `JB_CAPTURE_REV32=1`.
  - Configurations with the EQ on (lead 1 and 4; Rev2.3 states 0, 2 and 3)
    null against these and stay within 2 dB of Rev3.1.
  - The EQ changed on purpose; see docs/graphic-eq.md.
  - Every configuration with the EQ off still nulls against Rev3.1.
- **`SmokeTest` EQ checks** now compare against the analytic circuit response at
  the plugin's 4x rate.

## Rev3.2.2

- **`CabFilterTest.cpp`**
  - Default HPF 20 Hz / LPF 20 kHz (and cab off) are bit-identical passthrough
    at 44.1, 48, 96 and 192 kHz.
  - HPF 80 Hz + LPF 5 kHz matches the analytic digital biquad to ≤ 0.08 dB;
    cutoffs are within 0.4 dB of −3 dB.
  - HPF 100 Hz matches at 44.1 / 96 / 192 kHz.
  - Stereo match, quiet-tail decay, zipper-free cutoff jumps.
  - Plugin path: identity at defaults, HPF engages, state round-trip, old
    sessions restore the bypass ends.
- **`EditorTest`** also checks that the HPF/LPF knobs and captions sit inside
  the CAB panel for both amps without covering the knobs, and that the IR
  drop zones remain under the filter row.
- `juicyboi_screenshots` writes `rev3.2.2-cab-hpf-lpf*.png`, pedalboard /
  editor shots, and `rev3.2.2-cab-filters-response.png`.
- `JB_CPU_CABFILT=1 juicyboi_dynamics cpu` measures the cost with cab on and
  both filters engaged (no IR loaded).

## Rev3.3

- **`PitchShiftTest.cpp`** (in the smoketest; `juicyboi_pitchprobe` runs only these tests).
  - `rev3.2.2-bypass.f32` was captured ONCE from the v3.2.2 build (297dbd4)
    with `BypassRender.h`. With PITCH off, all four scenarios must match it
    bit for bit, including with every PITCH knob and mode changed.
  - −24…+24 st, mono and poly, within 1 cent by `PitchDetector` and a
    spectral-peak search. Poly chord: the old partials are gone.
  - Toggle logic, exact-dry cases, true bypass, click-free moves, NaN /
    denormal checks at four rates and odd block sizes, latency, per-amp
    independence, CPU.
- **`EditorTest`**: the slim PITCH pedal sits between TUNE and BOOST, no pedals
  overlap, every control and readout fits, and the buttons and LED are bound.
- `juicyboi_screenshots` writes `rev3.3-*.png`.
- `JB_CPU_PITCH=mono|poly juicyboi_dynamics cpu` measures the whole plugin with
  PITCH on (−12 st).
- Do not regenerate `rev3.2.2-bypass.f32` from later code.
