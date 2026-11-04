#pragma once

#include "Filters.h"
#include "ChannelVoice.h"

namespace jb::dsp
{
/** Per-channel tone stack + presence.

    Centres come from the Red/Orange FMV component cues on ChannelVoicing
    (Ct / Cm / Cb / Rs / pots). Still a non-interacting biquad stand-in for a
    full bilinear passive TMB — next step is the analytic 3rd-order TF.

    Presence models the NFB-shelf idea (Red schematic: ~22k + 3n ≈ 2.4 kHz).
    Knobs 0..10; 5 ≈ neutral for TMB (presence 5 = mild lift). */
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
    double bassHz = 100.0, midHz = 650.0, midQ = 0.75, trebleHz = 2800.0, presenceHz = 2400.0;
    float kBass = 5.0f, kMid = 5.0f, kTreble = 5.0f, kPresence = 5.0f;
    bool dirty = true;

    Biquad bassF, midF, trebleF, presenceF;
};
} // namespace jb::dsp
