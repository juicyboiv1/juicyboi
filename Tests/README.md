# Headless regression tests

`cmake --build build --target juicyboi_smoketest -j8` then run
`build/juicyboi_smoketest_artefacts/Release/juicyboi_smoketest` (Linux).

`legacy-348fc68.f32` contains 73,728 little-endian float samples captured from
unmodified processor/DSP commit `348fc68`, GCC 14 Release, 48 kHz, mono input,
256-sample blocks. All three Rectifier voices and Boost off/OD-808/SD-1 are
covered, using default Gain/TMB/Master, no cab/EQ/gate. The deterministic signal
and ordering live in `legacyRegression`. Comparison tolerance is 2e-6 to allow
minor compiler/platform math differences; local builds should null exactly.
Do not regenerate this fixture from the feature processor to bless a failure.
The capture-only code was removed after saving the baseline to avoid accidental
regeneration with feature code.

`PresenceTest.cpp` independently solves the declared loaded feedback-divider
equations, measures digital sine magnitude/phase at 54 points, checks the
bilinear-frequency mapping and raw analog-surrogate discrepancy, rapid sweeps,
reference null, silence/DC, missing/new state recall and model/EQ transitions.
This validates implementation against a documented nominal model, not against
real amp measurements. See `docs/5150-presence.md`.

`ResonanceTest.cpp` checks the 5150 Resonance stage (VR8 1M || C12 .0068u in
series with R56) against independent nodal equations at 90 points, including
Presence interaction, bit-exact reference at 0, sweeps, silence/DC, state
migration and model-crossfade isolation. `lead-62a1239.f32` (40,960 floats,
5 configurations from `LeadFixture.h`) was captured once from unmodified
commit `62a1239`: three 5150 configurations (must null at default Resonance)
and two Rectifier configurations at Bass 5 with other tone knobs, Boost and EQ
non-default (must null after the FMV Bass-centre move to 100 Hz). Do not
regenerate it from feature code to bless a failure.

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

`EditorTest.cpp` paints the Rev1 editor offscreen, with no native window. It checks:
- every parameter has a bound control with the same component ID, plus the IR A/B slots
- the head has no text labels, and every head knob has a tooltip (Input/Master keep their dB equivalents)
- the Model toggle sets `preamp_model` and swaps the faceplates; Rectifier-only controls are hidden on 5150 and their values are kept
- Red Voice is disabled on Orange
- the drawer opens and closes with a fixed aspect ratio, and scaling works
- all 7 EQ bands plus Level are shown
- opening and closing the editor leaves the session state byte-identical

`Screenshots.cpp` builds `juicyboi_screenshots <dir> [scale]`, which writes PNGs of the Rectifier and 5150 faceplates with the drawer closed and open.
