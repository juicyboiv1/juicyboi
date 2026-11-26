"""GEQ-7 (GE-7-style 7-band graphic EQ) analytic s-domain model from component values.

Topology and values: Jon Halverson's redrawn GE-7 schematic (2000), parts as
listed by the Swellamps bench model. Ideal op-amps / followers.
"""
import numpy as np

# Input network + pre-emphasis stage (U5a)
R30, C22, R29 = 10e3, 47e-9, 470e3
R31, R28, C20 = 4.7e3, 470.0, 15e-9
# Level stage (U4b): R27 in, R26 feedback, 10k pot, leg R2 + C16
R27, R26, RPOT = 10e3, 10e3, 10e3
RL, CL = 2.2e3, 10e-6
# Band stage (U4a): R24 in, R25 feedback, 7 x 10k pots
R24, R25 = 3.3e3, 3.3e3
# gyrator bands: (Cseries, Cgyr, R1, R2)
GYR = [
    (1.5e-6, 56e-9, 100e3, 330.0),   # 100 Hz  C10 C13 R16 R17
    (680e-9, 33e-9, 82e3, 330.0),    # 200 Hz  C9  C8  R9  R8
    (330e-9, 15e-9, 100e3, 330.0),   # 400 Hz  C7  C14 R18 R19
    (150e-9, 8.2e-9, 100e3, 330.0),  # 800 Hz  C5  C6  R7  R6
    (100e-9, 3.9e-9, 82e3, 330.0),   # 1.6 kHz C4  C15 R21 R22
    (39e-9, 2.2e-9, 82e3, 330.0),    # 3.2 kHz C2  C3  R5  R4
]
C1, R3 = 47e-9, 820.0                # 6.4 kHz leg (C1 + R3)
# Output: C19 coupling, R32 to Vb, de-emphasis R33 / (R36 + C27), C26 + R34 into follower,
# emitter R14 1k -> C11 1u -> R12 100k (unloaded output)
C19, R32 = 1e-6, 1e6
R33, R36, C27 = 4.7e3, 470.0, 15e-9
C26, R34 = 47e-9, 1e6
R14, C11, R12 = 1e3, 1e-6, 100e3
# Emitter follower (2SC1815-class, beta 300) small-signal at its bias point
# (Ic = 0.296 mA with the base fed through R34 1M: gm = 11.45 mS, gpi = 36.2 uS,
# go = 2.81 uS; ngspice .op): Re_eff = R15 || (R14 + R12) || ro = 8.872k.
R15 = 10e3
_gm, _gpi, _go = 11.4527e-3, 36.2186e-6, 2.81037e-6
_Re = 1.0 / (1.0 / R15 + 1.0 / (R14 + R12) + _go)
FOLLOWER_GAIN = (_gm + _gpi) * _Re / (1.0 + (_gm + _gpi) * _Re)   # 0.99029
FOLLOWER_RIN = (1.0 + (_gm + _gpi) * _Re) / _gpi                   # 2.84 Mohm
R34_EFF = 1.0 / (1.0 / R34 + 1.0 / FOLLOWER_RIN)                    # 0.740 Mohm

BAND_HZ = [100, 200, 400, 800, 1600, 3200, 6400]

# Op-amps as single-pole integrators A(s) = wt / s (unity-gain frequency):
# U4a / U4b uPC4558 ~3 MHz, gyrator followers TL022 ~0.5 MHz. U5a (HA1457W
# low-noise preamp) is modelled ideal. Set to None for ideal op-amps.
GBW_4558 = 3e6
GBW_TL022 = 0.5e6


def follower(s, gbw):
    return 1.0 if gbw is None else 1.0 / (1.0 + s / (2 * np.pi * gbw))


def leg_Z(k, s, gbw=None):
    if gbw is None:
        gbw = GBW_TL022
    if k < 6:
        Cs, Cg, R1, R2 = GYR[k]
        # gyrator: Cg from X to u, R1 u->Vb, follower(u) -> R2 -> X
        hp = s * Cg * R1 / (1 + s * Cg * R1)
        Yg = hp / R1 + (1 - follower(s, gbw) * hp) / R2
        return 1 / (s * Cs) + 1 / Yg
    return R3 + 1 / (s * C1)


def graphic_stage(s, Rin, Rf, legs, gbw=None):
    """legs: list of (alpha, Rpot, Z(s)); alpha = wiper position from the + (input) end (0) to the - end (1).
    alpha=1 -> wiper at the minus input -> boost. gbw: op-amp unity-gain frequency (None = ideal)."""
    s = np.asarray(s)
    a11 = 1 / Rin + 0j * s
    a22 = 1 / Rf + 0j * s
    c = 0j * s
    for alpha, Rp, Z in legs:
        ga = 1 / max(alpha * Rp, 1e-3)
        gb = 1 / max((1 - alpha) * Rp, 1e-3)
        gr = 1 / Z
        G = ga + gb + gr
        a11 = a11 + ga * (gb + gr) / G
        a22 = a22 + gb * (ga + gr) / G
        c = c + ga * gb / G
    if gbw is None:
        # P = N: Vin/Rin = (a11 - c) P ; Vout/Rf = (a22 - c) P
        return (a22 - c) / (a11 - c) * Rf / Rin
    A = 2 * np.pi * gbw / s
    # a11 P - c N = Vin/Rin ; -c P + a22 N = A (P - N) / Rf
    # -> N = P (c + A/Rf) / (a22 + A/Rf)
    r = (c + A / Rf) / (a22 + A / Rf)
    P = (1 / Rin) / (a11 - c * r)
    N = P * r
    return A * (P - N)


def H_input(s):
    return R29 / (R29 + R30 + 1 / (s * C22))


def H_pre(s):
    return 1 + R31 / (R28 + 1 / (s * C20))


def H_level(s, aL, gbw=GBW_4558):
    return graphic_stage(s, R27, R26, [(aL, RPOT, RL + 1 / (s * CL))], gbw)


def H_bands(s, alphas, gbw=GBW_4558, gbw_f=GBW_TL022):
    return graphic_stage(s, R24, R25, [(alphas[k], RPOT, leg_Z(k, s, gbw_f)) for k in range(7)], gbw)


def H_out(s):
    """C19 -> node A (R32 to gnd) -> R33 -> node B ((R36 + C27) to gnd) -> C26 -> node C (R34 to gnd, follower in).
    Follower (small-signal BJT) -> R14 -> C11 -> R12 (output)."""
    Zc19 = 1 / (s * C19)
    Zsh = R36 + 1 / (s * C27)
    Zc = 1 / (s * C26) + R34_EFF
    ZB = 1 / (1 / Zsh + 1 / Zc)
    ZA = 1 / (1 / R32 + 1 / (R33 + ZB))
    vA = ZA / (Zc19 + ZA)
    vB = vA * ZB / (R33 + ZB)
    vC = vB * R34_EFF / Zc
    vo = FOLLOWER_GAIN * vC * R12 / (R14 + 1 / (s * C11) + R12)
    return vo


def H_total(f, alphas, aL=0.5, ideal=False):
    s = 2j * np.pi * np.asarray(f, dtype=float)
    if ideal:
        return H_input(s) * H_pre(s) * H_level(s, aL, None) * H_bands(s, alphas, None, 1e30) * H_out(s)
    return H_input(s) * H_pre(s) * H_level(s, aL) * H_bands(s, alphas) * H_out(s)


if __name__ == "__main__":
    f = np.logspace(np.log10(20), np.log10(20000), 2000)
    flat = 20 * np.log10(np.abs(H_total(f, [0.5] * 7)))
    print("flat: min %.3f max %.3f dB  @1k %.3f" % (flat.min(), flat.max(), np.interp(1000, f, flat)))
    for k in range(7):
        for a in (1.0, 0.0):
            al = [0.5] * 7; al[k] = a
            m = 20 * np.log10(np.abs(H_total(f, al))) - flat
            i = np.argmax(np.abs(m))
            c = 20 * np.log10(abs(H_total([BAND_HZ[k]], al)[0] / H_total([BAND_HZ[k]], [0.5]*7)[0]))
            print("band %5d a=%.0f: extreme %+.2f dB at %7.1f Hz; at label %+.2f dB" % (BAND_HZ[k], a, m[i], f[i], c))
    for a in (1.0, 0.0):
        print("level a=%.0f: %+.2f dB @1k" % (a, 20 * np.log10(abs(H_level(2j*np.pi*1000, a)))))
