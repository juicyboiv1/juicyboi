#pragma once

#include <juce_graphics/juce_graphics.h>

namespace jb::gui::metal
{
/** Rev3.2.1 amp-badge lettering: tall, narrow, jagged capitals made of straight
    angular strokes that taper into long spikes overshooting above and below the
    letter body, plus a few hairline splinters. Original vector outlines (baked
    into MetalGlyphData.inc by tools/metal_glyphs.py, deterministic, no font, no
    licence, no curves). Letters available: I J U C Y B O (the badge word);
    see docs/metal-badge.md. */
bool hasGlyph (juce::juce_wchar);

/** True when every non-space character of `text` has a glyph. */
bool canSpell (const juce::String& text);

/** The word as one vector Path in design units: cap top y = 0, baseline y = 100
    (y down), spikes reach about y = -40 / +140. Contours are wound so a non-zero
    fill gives solid strokes and clear holes (O). Identical on every call. */
juce::Path wordPath (const juce::String& text);

/** Draws the word fitted (aspect preserved) and centred in `area`: soft drop
    shadow, brushed-steel fill, dark outline, thin highlight on the upper edges.
    Pure vector, so it is crisp at every UI scale. */
void drawWord (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area);
}
