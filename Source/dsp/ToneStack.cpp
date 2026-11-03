#include "ToneStack.h"

namespace jb::dsp
{
void ToneStack::prepare (double sampleRate)
{
    fs = sampleRate;
    dirty = true;
    update();
    reset();
}

void ToneStack::reset()
{
    bassF.reset(); midF.reset(); trebleF.reset(); presenceF.reset();
}

void ToneStack::setVoicing (const ChannelVoicing& v)
{
    if (differs (v.bassHz, bassHz) || differs (v.midHz, midHz) || differs (v.midQ, midQ)
        || differs (v.trebleHz, trebleHz) || differs (v.presenceHz, presenceHz))
    {
        bassHz = v.bassHz; midHz = v.midHz; midQ = v.midQ;
        trebleHz = v.trebleHz; presenceHz = v.presenceHz;
        dirty = true;
        update();
    }
}

void ToneStack::setKnobs (float bass, float mid, float treble, float presence)
{
    if (differs (bass, kBass) || differs (mid, kMid) || differs (treble, kTreble) || differs (presence, kPresence))
    {
        kBass = bass; kMid = mid; kTreble = treble; kPresence = presence;
        dirty = true;
        update();
    }
}

void ToneStack::update()
{
    if (! dirty)
        return;

    dirty = false;

    // Knob (0..10) -> dB. Mid range is asymmetric: high-gain riffing mostly
    // wants cut/scoop, with a little boost available.
    const auto bassDb     = (kBass   - 5.0) * 2.4;            // ±12 dB
    const auto midDb      = kMid < 5.0f ? (kMid - 5.0) * 2.4  // -12 dB
                                        : (kMid - 5.0) * 1.6; // +8 dB
    const auto trebleDb   = (kTreble - 5.0) * 2.4;            // ±12 dB
    const auto presenceDb = -6.0 + kPresence * 1.5;           // -6..+9 dB

    bassF    .setCoeffs (BiquadCoeffs::lowShelf  (fs, bassHz,     0.707, bassDb));
    midF     .setCoeffs (BiquadCoeffs::peak      (fs, midHz,      midQ,  midDb));
    trebleF  .setCoeffs (BiquadCoeffs::highShelf (fs, trebleHz,   0.707, trebleDb));
    presenceF.setCoeffs (BiquadCoeffs::highShelf (fs, presenceHz, 0.6,   presenceDb));
}
} // namespace jb::dsp
