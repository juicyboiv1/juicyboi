#pragma once

#include "Filters.h"
#include "SoftClip.h"
#include "ToneStack.h"
#include "RecoveryStage.h"

namespace jb::dsp
{
/* Rev2.4: the six-stage cascade, Pre/Post Gain and power section moved to
   Amp5150.h (Rhythm + Lead channels). This header keeps the shared 5150 TMB. */

/** Lead passive TMB, read off the Peavey EVH 5150 schematic, sheet 3 (Tone
    Stack/PI), https://www.prowessamplifiers.com/schematics/post/peavey_evh5150.pdf:
      VR5 HIGH 250K (B = linear), C14 470pF || C60 100pF (= 570 pF treble cap),
      R28 47K (slope), C18 .022uF (bass), C21 .022uF (mid), VR3 LOW 1M
      (A = audio, wired as rheostat), VR4 MID 50K (B = linear).
    Not modelled: the R65 47K branch from the Low/Mid junction, the R24 33K /
    C59-R94 loading at the V5A drive node and the load on the treble wiper
    (ideal source, unloaded output). makeupDb only restores the Rev1 Lead level
    at default knobs. */
inline TmbComponents leadTmb()
{
    return { 250e3, 1e6, 50e3, 47e3, 570e-12, 22e-9, 22e-9, 2.80 };
}

/** Model-specific passive TMB after the Lead cascade (solved network, see
    ToneStack.h). Ideal source/unloaded output: the plate's source impedance and
    the following stage's load are not modelled. No PI, power amp, NFB
    presence/resonance (those are LeadPresence / LeadResonance). Shared by the
    Rhythm and Lead channels (one EQUALIZATION block on the real amp). */
class Lead5150Tone
{
public:
    Lead5150Tone() { stack.setComponents (leadTmb()); }

    void setKnobs (float bass, float mid, float treble) noexcept
    {
        stack.setKnobs (std::clamp (bass, 0.0f, 10.0f), std::clamp (mid, 0.0f, 10.0f),
                        std::clamp (treble, 0.0f, 10.0f));
    }
    void prepare (double sampleRate) noexcept { stack.prepare (sampleRate); }
    void snap() noexcept { stack.snap(); }
    void beginFrame() noexcept { stack.beginFrame(); }
    float process (float x, int ch) noexcept { return stack.process (x, ch); }

    const PassiveToneStack& passive() const noexcept { return stack; }

private:
    PassiveToneStack stack;
};
} // namespace jb::dsp
