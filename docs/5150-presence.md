# 5150 Presence: declared small-signal model

This is a **relative closed-loop response approximation**, not measured hardware
or a nonlinear power-amp simulation. Existing Lead gain/filter/TMB coefficients
are unchanged. Position 0/default is the old Lead sound exactly; turning up adds
the calculated change from that reference. Rectifier's `pre_presence` and DSP
remain unchanged.

## Circuit provenance and readable values

- Matching user-sheet redraw: [EVH 5150, sheet 3, Tone Stack/PI](https://www.prowessamplifiers.com/schematics/post/peavey_evh5150.pdf).
- Cross-check: [Peavey drawing 81501510, EVH 120 PREAMP, sheets 1/3 and 2/3](https://el34world.com/charts/Schematics/Files/Peavey/5150_evh.pdf).
  This is a clearer manufacturer-labelled scan, not proof of a particular
  production amp revision or of the later Fender EVH 5150 III circuit.
- R56 feedback resistor: 39 kOhm; R57 PI-tail ground resistor: 4.7 kOhm.
- VR9 Presence: 10 kOhm rheostat, wiper strapped to its capacitor-side terminal.
  The scan says `10KAVB`; an exact mechanical/electrical taper is not established.
- C8 0.1 uF and C62 0.033 uF are **parallel**, giving 0.133 uF. C8's `P10`
  suffix is not used as a guessed extra capacitance. The clearer scan confirms
  C62=.033; do not substitute the .068 cap from other 5150-family amplifiers.
- The separate VR8/C12 (.0068 uF) network is Resonance, not Presence. It is fixed
  at the bypassed/minimum-series-resistance reference here; no Resonance control
  or LF resonance boost is implemented.
- The feedback node reaches the PI tail through a 22 uF coupling capacitor
  (its designator is fuzzy in the redraw). That cap is approximated as an AC
  short in the Presence band. R85=220k and R81=2.2M ground the **signal-input
  grid** branch; they are not mistakenly paralleled as feedback-node loads.

## Explicit assumptions and limitations

1. Effective feedback-node load `RL = R57 = 4700 Ohm`. Finite active-PI loading
   through the tail/cathode network is not solved. This is a nominal reduction,
   not a claim that R57 is the complete physical input impedance.
2. Effective forward voltage gain `A = 20 V/V`, real and frequency-independent
   over the modeled band. This is a declared calibration assumption, **not a
   value measured from or completely derivable from the supplied sheets**.
   Actual PI/power-tube gain, transformer ratio, speaker impedance, output
   impedance and frequency response would be needed to establish a real loop.
3. Knob `p` runs 0..1. Use the explicit nominal two-decade law
   `R(p) = 10000 * (10^(2*(1-p))-1)/99` Ohm, decreasing from 10k to zero.
   Mid-position is 909.09 Ohm. This makes the top-end increase gradual and useful,
   but does not claim a verified factory pot taper. The resistance target is
   slewed linearly over 30 ms, once per sample frame (same for L/R).
4. The modeled transfer is a **change relative to p=0**. The old Lead voicing is
   retained as the reference; we do not assert it already equals a real 5150
   with its physical Presence at zero. DC is unity by normalization; the actual
   amp's DC/very-low-frequency signal path is outside this reduction.

## Nodal model and realizable filter

Let `C=.133e-6`, `Rf=39000`, `RL=4700` and `R=R(p)`:

```
Z(s,R)    = RL || (R + 1/(s*C))
beta(s,R) = Z(s,R) / (Rf + Z(s,R))
T(s,R)    = A / (1 + A*beta(s,R))
H(s,p)    = T(s,R(p)) / T(s,10000)
```

This includes resistor loading in the feedback divider; it is **not** a generic
RBJ shelf with a renamed control or a corner calculated from one isolated RC.
After normalizing the common DC gain of T, set:

```
d0   = Rf + RL
n0   = d0 + A*RL
tz(R)= C*(Rf*RL + R*d0)/d0
tp(R)= C*(Rf*RL + R*n0)/n0
F(s,R) = (1+s*tz(R))/(1+s*tp(R))
H(s,p) = F(s,R(p))/F(s,10000)
```

Thus H has two real positive time constants/poles, not just one shelf. All
resistances are nonnegative and both pole time constants remain positive at
both endpoints. The bilinear transform uses two trapezoidal-integrator (TPT)
one-poles; no explicit/algebraic audio feedback loop, phase-margin guess or
unstable recursive physical loop is run. For rate `fs`, `k=1/(2*fs*tau)` and
`alpha=k/(1+k)`. Pole mapping stays strictly within the unit circle.

Implementation uses `L1=1/(1+s*tp(R))`, `L2=L1/(1+s*tz(10000))` and weights:

```
g = tz(R)*tp(10000)/(tp(R)*tz(10000))
c = (tz(R)+tp(10000)-g*(tp(R)+tz(10000)))/tz(10000)
d = 1-g-c
output = input + (g-1)*input + c*L1(input) + d*L2(input)
```

At reference the residual weights are explicitly zero: output is bit-identical
input, even with warm internal states. This avoids a discontinuous bypass when
a moving control lands on zero. Double-precision filter states/weights reduce
cancellation error; coefficients change with smoothed physical resistance.

## Placement and state compatibility

Both complete model paths stay warm:

```
shared Boost -> Rectifier -> legacy TMB/Presence -> Rectifier Graphic EQ --\
                                                                          30ms mix -> Cab -> dry-key Gate -> Master
shared Boost -> Lead -> Lead TMB -> Lead Graphic EQ -> Lead Presence ------/
```

Two EQ instances share settings but have independent input histories. Moving
the model crossfade after those stages ensures only the Lead contribution gets
new Presence, including mid-switch and EQ-enable transitions. Cab and everything
downstream remain shared. No new fixed latency is added. Both default-off EQs
remain true bypass. The extra EQ instance costs CPU when EQ is on.

`lead_presence` (0..10, default 0) is appended after `preamp_model`, preserving
all existing indices/IDs/defaults. States lacking it explicitly restore 0 even
when loaded over a currently brighter processor. The Output Presence is enabled
only in Lead mode; the existing Preamp Presence remains Rectifier-only. Their
settings are saved independently and are never copied over each other.

## Test interpretation

Tests measure sine magnitude/phase at 54 points: min/mid/max, 20/100/300/1000/
3000/10000 Hz, and 44.1/48/96 kHz. They compare against the independent loaded
nodal equations above at the bilinear-warped frequency, and also bound the
magnitude difference from the unwarped analog surrogate. These are **model
agreement tests, not amp measurements**. Rapid resistance sweeps, exact-reference
null, silence/decay/DC, state migration, model reversals with EQ on/off, and
Rectifier isolation are tested alongside the existing full regression suite.

No nonlinear power tubes, PI drive/distortion, transformer or speaker-dependent
feedback, sag, output-stage damping or adjustable Resonance is simulated. The
new stage is linear for fixed controls; existing nonlinear DSP stays at 4x.
Quantitative whole-plugin aliasing and real Mac/Scarlett/listening validation
remain unperformed. A different actual forward gain/load/taper will change the
curve: this implementation does not make those unknowns disappear.
