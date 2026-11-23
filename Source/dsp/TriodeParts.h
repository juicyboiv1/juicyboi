#pragma once

// Rev3 component values for every solved triode stage. Sources and reasoning
// per value: docs/triode.md section 2. "Equivalent" networks collapse the
// passive parts around a stage onto TriodeStage's one topology
// (vs -Rs- Cc - n -Rgs- grid, Rg from n to ground); the gain pots and Pre
// Gain networks stay outside the stage (they are modelled separately).

#include "Triode.h"

namespace jb::dsp::triodeparts
{
using triode::StageParts;
//                        B+      Ra       Rk      Ck       Cc      Rs        Rg        Rgs
// ---- Dual Rectifier preamp, sheet RF-1F (6-93). B+ from the printed plate/cathode
//      voltages: E = 200 V + 220K * 1.6V/1.8K = 395 V, D = 280 V + 100K * 2.0V/1.8K = 391 V,
//      C = 213 V + 220K * 1.6V/1.8K = 409 V.
/** V1A: R221 220K, R291 1.8K, C31 1uF (LDR3 shorts R271), grid R211 1M, DC-coupled
    input; Rs = 10K nominal source (guitar / pedal output into the jack). */
inline StageParts rectoV1A() noexcept { return { 395.0, 220.0e3, 1.8e3, 1.0e-6, 0.0,     10.0e3,  1.0e6,   1.0e3 }; }
/** V2A: R201 100K, R292 1.8K, C32 1uF. Grid network (equivalent): V1A plate
    (220K || rp = 48K) -> C21 .02u -> node with R321 2.2M || (R322 2.2M + 1M gain pot)
    = 1.3M to ground -> R322 2.2M + R241 470K + wiper (~250K) to the grid. */
inline StageParts rectoV2A() noexcept { return { 391.0, 100.0e3, 1.8e3, 1.0e-6, 20.0e-9, 48.0e3,  1.3e6,   2.9e6 }; }
/** V2B (cold clipper): R202 100K, R103 39K unbypassed. V2A plate (100K || rp = 38K)
    + R242 470K -> C22 .02u -> R212 1M at the grid. */
inline StageParts rectoV2B() noexcept { return { 391.0, 100.0e3, 39.0e3, 0.0,   20.0e-9, 508.0e3, 1.0e6,   1.0e3 }; }
/** V3A: R224 220K, R293 1.8K, C33 1uF (LDR10). V2B plate (~100K, cold stage) ->
    C27 .02u -> R106 330K leak -> R225 220K grid stopper. */
inline StageParts rectoV3A() noexcept { return { 409.0, 220.0e3, 1.8e3, 1.0e-6, 20.0e-9, 100.0e3, 330.0e3, 220.0e3 }; }

// ---- 5150 preamp (EVH 120 preamp drawing 81501510). The supply nodes V1/V2 are
//      not printed on the sheets available: 290 V / 320 V are nominal assumptions.
inline constexpr double k5150V1 = 290.0, k5150V2 = 320.0;
/** V1A: R2 220K, R12 1.8K, C3 1uF; R22 68K stopper, R25 1M at the jack; DC-coupled. */
inline StageParts f5150V1A() noexcept { return { k5150V1, 220.0e3, 1.8e3, 1.0e-6, 0.0,     10.0e3,  1.0e6,   68.0e3 }; }
/** V1B: R3 100K, R17 1.8K, C4 1uF (Rhythm's C26 22uF shelf stays outside). Grid
    network (equivalent): V1A plate 48K + R29 470K -> C6 .022u -> R41 1M || pot ~
    500K -> wiper (~250K) + R82 470K stopper. */
inline StageParts f5150V1B() noexcept { return { k5150V1, 100.0e3, 1.8e3, 1.0e-6, 22.0e-9, 518.0e3, 500.0e3, 720.0e3 }; }
/** V2A (cold clipper): R1 100K, R15 39K unbypassed. V1B plate (38K) + R6 470K ->
    C2 .022u -> R9 1M at the grid (Rhythm's R20 220K shunt stays outside). */
inline StageParts f5150V2A() noexcept { return { k5150V1, 100.0e3, 39.0e3, 0.0,   22.0e-9, 508.0e3, 1.0e6,   1.0e3 }; }
/** V2B: R4 220K, R13 1.8K, C5 1uF. V2A plate (~100K) + R7 220K -> C1 .022u ->
    R11 330K at the grid (C40 1n stays in the stage low-pass). */
inline StageParts f5150V2B() noexcept { return { k5150V2, 220.0e3, 1.8e3, 1.0e-6, 22.0e-9, 320.0e3, 330.0e3, 1.0e3 }; }
/** V5B: R96 220K, R102 2.2K unbypassed. V2B plate (48K) + R32 1M -> C57 .022u ->
    R101 100K at the grid. */
inline StageParts f5150V5B() noexcept { return { k5150V2, 220.0e3, 2.2e3, 0.0,   22.0e-9, 1.05e6,  100.0e3, 1.0e3 }; }
/** V5A: R86 100K, R93 2.2K unbypassed. V5B plate (~124K) + R87 1M -> C56 .022u ->
    R91 1M leak -> R88 1M grid stopper. */
inline StageParts f5150V5A() noexcept { return { k5150V2, 100.0e3, 2.2e3, 0.0,   22.0e-9, 1.12e6,  1.0e6,   1.0e6 }; }
} // namespace jb::dsp::triodeparts
