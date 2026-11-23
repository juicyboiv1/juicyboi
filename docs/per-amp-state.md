# Per-amp settings (Rev2.4, state version 4)

Each amp has its own complete set of settings, and switching amps
(`preamp_model`) recalls that amp's set.

## Scope

| | Rectifier set | 5150 set |
|---|---|---|
| amp knobs | pre_gain, pre_bass, pre_mid, pre_treble, pre_presence, master, channel (Red/Orange), red_voice (V1/V2) | m5150_channel (Rhythm/Lead), m5150_bright, m5150_crunch, m5150_rhythm_pre, m5150_lead_pre, m5150_low, m5150_mid, m5150_high, m5150_rhythm_post, m5150_lead_post, lead_resonance, lead_presence |
| boost | od_enable, od_type (OD-808/SD-1), od_drive, od_tone, od_level | m5150_od_* |
| post EQ | eq_enable, eq_100 … eq_6k4, eq_level | m5150_eq_* |
| cab | cab_enable, cab_mic_blend, cab_level, IR A/B files (state properties ir_a / ir_b) | m5150_cab_*, m5150_ir_a / m5150_ir_b |
| gate | gate_enable, gate_threshold, gate_hold, gate_release | m5150_gate_* |

Global: `preamp_model` (which amp), `bypass`, `input_trim` (shared input
calibration for both amps; 0–10, 5 = 0 dB), and `preamp_out` (Full Amp /
Preamp Out). The Input Cal trim is an interface-level calibration (set once
for the Scarlett, or any other DI), not a faceplate gain knob. It lives in a
small setup plate on the left of the pedalboard drawer front, labelled
**INPUT CAL**, next to the **PREAMP OUT** toggle.

`m5150_input_trim` stays in the parameter layout (same ID, index and range)
so host automation indices stay stable, but it is unused and hidden. Lanes
recorded on it no longer change the sound.

**The Rectifier modes share one set.** Red v1, Red v2 and Orange are channels
and voicings of one amp: on the real amp they share the boost in front, the
pedals and the cab. Red v1/v2 is also an A/B tool, which only works if
everything else stays the same. The 5150 is a different amp, so it gets its
own set.

Each amp has its own boost, EQ, cab convolver and gate (keyed from the dry
input after the shared calibration). Aside from `input_trim`, the two amps
can never affect each other's audio (`PerAmpTest`, both directions
bit-identical). During the 30 ms model crossfade both chains run. The
inaudible amp's convolver is skipped and reset before reuse.

## Parameter IDs and automation compatibility

- Every Rev2.3 parameter keeps its ID, index and range. The legacy IDs are
  now the **Rectifier** set.
- All new parameters (`m5150_*`) are **appended** after `lead_resonance`. Host
  automation that indexes by position is therefore unchanged, and the plugin
  ID/code/name stay the same (com.juicyboi.juicyboi-rev2, Jcb2, "juicyboi
  Rev2"), so Rev2.4 replaces Rev2 in place.
- **Behaviour change for old automation:** lanes recorded on
  `pre_gain`, `od_*`, `eq_*`, `cab_*`, `gate_*` or `master`
  while the 5150 was active now move the Rectifier. To automate the 5150,
  re-record on the "5150 …" parameters.
  `lead_presence` / `lead_resonance` already belonged to the 5150 and still
  do. `input_trim` is once again a shared calibration (as it was before
  Rev2.4): automating it moves both amps. Lanes recorded on
  `m5150_input_trim` in Rev2.4 no longer have any effect.

## Migration (state v3 → v4)

This runs once, when a session saved by Rev2.3 or older loads
(`migrateToPerAmp`, after the v1/v2 dB→knob conversions):

1. Every per-amp slot value (boost, EQ, cab, gate, IR paths, and unused
   `m5150_input_trim`) is copied into the 5150 set. The legacy IDs keep it
   for the Rectifier. The shared calibration `input_trim` already holds the
   Rectifier / legacy input level, so old sessions keep that level.
2. The old 5150 was the Lead channel on the shared knobs, so the mapping is:
   channel = Lead; Lead Pre = Gain; Low/Mid/High = Bass/Mid/Treble.
3. The 5150 has no Master. Lead Post and Rhythm Post are set to the value
   with the **same small-signal gain change** as the old Master had from its
   default (Master 6.5 ↔ Post 5, `leadPostForMaster`).
4. Rhythm Pre, Bright and Crunch start at their defaults.
5. Any parameter still missing gets its default. The session is re-saved as
   v4 and never migrated again.

**Result.** Old sessions on the Rectifier render **bit-identically** to
Rev2.3. This is checked against renders that the Rev2.3 build made from four
saved sessions (`Tests/rev2.3-state-*.bin`, `rev2.3-migration-rect.f32`: max
error 0). Old 5150 sessions keep all their boost/EQ/gate/cab settings. They
sound like the new 5150 engine, which is intentional and not identical to
Rev2.3. Because Post now drives the power section, a session that had a very
high or low Master will compress more or less than before.

## Shared input calibration (after Rev2.4)

A real Rectifier or 5150 has no Input knob. `input_trim` is now one shared
input calibration for both amps, still with the same ID, index and 0–10
law. Old sessions keep the level the Rectifier (or the pre-Rev2.4 shared
knob) was using.

**Rev2.4 sessions where the 5150 had a different Input** now use the shared
value (`input_trim`, i.e. the Rectifier's old Input). The unused
`m5150_input_trim` value is left as saved but is not heard. The Rectifier
path at default and at migrated settings is bit-identical to Rev2.4
(`PerAmpTest` vs `rev2.3-migration-rect.f32`, `legacyRegression` vs
`rect-rev2.3.f32`, `ResonanceTest` section 7 vs `lead-rev2.3.f32`).

## Preamp Out (`preamp_out`)

A global toggle, default **Full Amp**, for running juicyboi into a real amp's
effects return (for example a Mesa Triple Rectifier FX return).

| | Full Amp (default) | Preamp Out |
|---|---|---|
| preamp stages, tone stacks, boosts, post EQ, gate | yes | yes |
| Rectifier recovery, 5150 Post Gain / Power5150, Rectifier Master | yes | yes |
| cab IR / cab sim | yes | bypassed |
| Red v2 SpeakerLoad, 5150 SpeakerLoad | yes | bypassed |
| 5150 Presence and Resonance (power-amp NFB) | yes | bypassed |

The parameter is **appended** after the 5150 slot set (`preamp_out`, choice
"Full Amp" / "Preamp Out", default Full Amp). Every earlier host index is
unchanged, and old sessions without the parameter load as Full Amp, so
existing Rectifier and 5150 fixtures stay bit-identical.

Switching is a 25 ms crossfade (click-free). While Preamp Out is on, the
bypassed controls are greyed out and disabled: the cab panel (both amps) and
the 5150 Presence and Resonance knobs. Their saved values are kept.

**Level.** Preamp Out is unity gain relative to the post-EQ preamp signal
(`JuicyboiAudioProcessor::preampOutGain = 1`). There is no extra pad. At
default knobs (cab and gate off, 110 Hz sine) `PreampOutTest` measured:

| | Full Amp rms / peak | Preamp Out rms / peak | vs Full Amp |
|---|---|---|---|
| Red v2 | −11.27 / −6.67 dBFS | −13.87 / −9.10 dBFS | −2.60 dB rms |
| 5150 Lead | −11.24 / −7.67 dBFS | −16.40 / −13.14 dBFS | −5.16 dB rms |

That is already a sensible line level for an FX return. Preamp Out is
quieter here because the speaker-load bump (and, when Cab is on, the cab
sim) is out of the path — which is what you want into a real power amp.
