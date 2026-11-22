# Per-amp settings (Rev2.4, state version 4)

Each amp has its own complete set of settings, and switching amps
(`preamp_model`) recalls that amp's set.

## Scope

| | Rectifier set | 5150 set |
|---|---|---|
| amp knobs | input_trim, pre_gain, pre_bass, pre_mid, pre_treble, pre_presence, master, channel (Red/Orange), red_voice (V1/V2) | m5150_input_trim, m5150_channel (Rhythm/Lead), m5150_bright, m5150_crunch, m5150_rhythm_pre, m5150_lead_pre, m5150_low, m5150_mid, m5150_high, m5150_rhythm_post, m5150_lead_post, lead_resonance, lead_presence |
| boost | od_enable, od_type (OD-808/SD-1), od_drive, od_tone, od_level | m5150_od_* |
| post EQ | eq_enable, eq_100 … eq_6k4, eq_level | m5150_eq_* |
| cab | cab_enable, cab_mic_blend, cab_level, IR A/B files (state properties ir_a / ir_b) | m5150_cab_*, m5150_ir_a / m5150_ir_b |
| gate | gate_enable, gate_threshold, gate_hold, gate_release | m5150_gate_* |

Global: `preamp_model` (which amp) and `bypass`.

**The Rectifier modes share one set.** Red v1, Red v2 and Orange are channels
and voicings of one amp: on the real amp they share the boost in front, the
pedals and the cab. Red v1/v2 is also an A/B tool, which only works if
everything else stays the same. The 5150 is a different amp, so it gets its
own set.

Every amp gets its own input copy, boost, EQ, cab convolver and gate (keyed
from the dry input). The two amps can never affect each other's audio
(`PerAmpTest`, both directions bit-identical). During the 30 ms model
crossfade both chains run. The inaudible amp's convolver is skipped and reset
before reuse.

## Parameter IDs and automation compatibility

- Every Rev2.3 parameter keeps its ID, index and range. The legacy IDs are
  now the **Rectifier** set.
- All new parameters (`m5150_*`) are **appended** after `lead_resonance`. Host
  automation that indexes by position is therefore unchanged, and the plugin
  ID/code/name stay the same (com.juicyboi.juicyboi-rev2, Jcb2, "juicyboi
  Rev2"), so Rev2.4 replaces Rev2 in place.
- **Behaviour change for old automation:** lanes recorded on
  `pre_gain`, `od_*`, `eq_*`, `cab_*`, `gate_*`, `input_trim` or `master`
  while the 5150 was active now move the Rectifier. To automate the 5150,
  re-record on the "5150 …" parameters.
  `lead_presence` / `lead_resonance` already belonged to the 5150 and still
  do.

## Migration (state v3 → v4)

This runs once, when a session saved by Rev2.3 or older loads
(`migrateToPerAmp`, after the v1/v2 dB→knob conversions):

1. Every per-amp slot value (input, boost, EQ, cab, gate, IR paths) is copied
   into the 5150 set. The legacy IDs keep it for the Rectifier.
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
