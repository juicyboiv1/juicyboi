# Cab HPF / LPF (Rev3.2.2)

High-pass and low-pass filters dedicated to the cab IR section, one pair per
amp (Rectifier and 5150 keep their own settings, like the rest of the cab).

| ID | 5150 ID | Name | Range | Default |
|----|---------|------|-------|---------|
| `cab_hpf` | `m5150_cab_hpf` | Cab HPF | 20 … 300 Hz, log-scaled | 20 Hz (bypass) |
| `cab_lpf` | `m5150_cab_lpf` | Cab LPF | 2 … 20 kHz, log-scaled | 20 kHz (bypass) |

Both IDs are **appended** after `tuner_ref`, so every earlier host index is
unchanged. Sessions saved before Rev3.2.2 load with both knobs at their
defaults (true bypass) and sound identical.

## Placement

The filters sit on the **wet (IR) path only**, after the IR A / IR B mix and
Cab Level, and before the cab enable dry/wet crossfade:

```
preamp ─┬─ (dry, used only while Cab fades on or off)
        └─ IR A / IR B mix × Level ─► HPF ─► LPF ─► enable crossfade ─► Gate
```

This is the usual guitar-IR-loader convention: the filters shape the cabinet,
not the dry preamp. Consequences:

- **BLEND** is IR A vs IR B; both IRs share the same HPF/LPF pair (one cab
  tone for the blended result).
- **Cab Level** is a linear gain, so it commutes with the filters. It is
  applied first, then HPF, then LPF.
- **Cab off** is still a hard bypass: once the 20 ms enable fade has settled,
  convolution and the filters are not run at all. This is the path into a
  real power amp / FX return, and it is bit-identical to Rev3.2.1.
- **Preamp Out** still skips the whole cab stage (filters included).
- **No IR loaded**, Cab on: the wet path is unity passthrough (as before).
  The filters still apply to that passthrough, so HPF/LPF can be used as a
  cab-section tone control even without an IR. At the default knob positions
  they are bypassed, so existing “cab on, no IR” sessions stay bit-identical.

## Slope, bypass and smoothing

- **12 dB/oct Butterworth** (one RBJ biquad each, Q = 1/√2). A 24 dB/oct
  option or a slope switch was left out to keep the narrow CAB panel to four
  knobs; a second biquad is cheap to add later if it is wanted.
- **Endpoint bypass.** HPF at (or within 2 % of) 20 Hz, and LPF at (or within
  2 % of) 20 kHz, are true bypass of that filter: the biquad is not run and
  the wet sample is untouched. Double-click a knob to return it to the
  bypassed end. This also avoids a 20 kHz LPF at 44.1 kHz, where that cutoff
  sits against the bilinear clamp (0.45 × fs).
- Cutoffs are smoothed in log-Hz (~30 ms) and each filter has a 15 ms dry/wet
  mix when it engages or bypasses, so knob moves do not zipper or click.
- Coefficients are rebuilt only when the smoothed cutoff changes, and are
  pre-warped via the RBJ cookbook (`Filters.h`). They stay well-behaved up to
  192 kHz (`safeCutoff` clamps to 0.45 × fs).
- The plugin’s `ScopedNoDenormals` already covers this stage. Filter state is
  cleared when the cab hard-bypasses or is reset (amp fade / sample-rate
  change).

## GUI

The CAB side panel keeps its OFF/ON toggle, then a 2×2 of knobs (BLEND and
LEVEL on the first row, HPF and LPF on the second) and the IR A / IR B drop
zones under them. Captions are HPF and LPF. No plugin name appears on the
panel. Hover a knob for its frequency; 20 Hz / 20 kHz are labelled as bypass
in the tooltip.

## Validation (`Tests/CabFilterTest.cpp`, part of the smoketest)

- Defaults (and cab off with the filters engaged) are bit-identical
  passthrough at 44.1, 48, 96 and 192 kHz.
- HPF 80 Hz + LPF 5 kHz matches the analytic digital biquad transfer function
  to ≤ 0.08 dB at ten frequencies; each cutoff is within 0.4 dB of −3 dB;
  1 kHz stays within 0.15 dB of unity.
- HPF 100 Hz alone matches the analytic response at 44.1 / 96 / 192 kHz (0.12 dB).
- Stereo channels match; a quiet tail stays finite and decays.
- A simultaneous jump of both cutoffs under a midband sine has a max sample
  step well under a zipper threshold.
- On the plugin path: cab on at default filters with no IR is identity after
  the enable fade; moving HPF to 80 Hz changes that impulse; state
  round-trips both amps’ knobs; loading a default/older session restores
  20 Hz / 20 kHz.
- `EditorTest` checks that HPF/LPF knobs and captions sit inside the CAB
  panel for both amps, that captions do not cover the knobs, and that the IR
  drop zones remain under the filter row.
