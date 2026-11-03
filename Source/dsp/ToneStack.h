#pragma once

#include "Filters.h"
#include "ChannelVoice.h"

namespace jb::dsp
{
/** PLACEHOLDER tone stack: three non-interacting biquads (low shelf, mid
    peak, high shelf) plus a presence high shelf.

    Real passive treble/mid/bass networks are INTERACTING (each knob shifts
    the others' curves) and lossy; presence in a tube amp lives in the power
    amp's negative-feedback loop. Next step: replace the three biquads with an
    analytic 3rd-order passive TMB transfer function (derived from your own
    component values) discretised via bilinear transform, and move presence
    into a future power-amp stage.

    Knobs are 0..10, 5 = flat (presence 5 = mild lift). Runs at base rate. */
class ToneStack
{
public:
    void prepare (double sampleRate);
    void reset();

    void setVoicing (const ChannelVoicing& v);
    void setKnobs (float bass, float mid, float treble, float presence);

    inline float process (float x, int ch) noexcept
    {
        x = bassF.process (x, ch);
        x = midF.process (x, ch);
        x = trebleF.process (x, ch);
        return presenceF.process (x, ch);
    }

private:
    void update();

    double fs = 44100.0;
    double bassHz = 100.0, midHz = 650.0, midQ = 0.8, trebleHz = 2800.0, presenceHz = 4000.0;
    float kBass = 5.0f, kMid = 5.0f, kTreble = 5.0f, kPresence = 5.0f;
    bool dirty = true;

    Biquad bassF, midF, trebleF, presenceF;
};
} // namespace jb::dsp
