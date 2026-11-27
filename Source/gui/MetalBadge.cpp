#include "MetalBadge.h"
#include "Rev1Style.h"

namespace jb::gui::metal
{
using namespace juce;

#include "MetalGlyphData.inc"

namespace
{
constexpr float kSpaceAdvance = 34.0f;
constexpr float kTrack = -1.0f; // glyph advance is shortened by this much: spikes interleave

int glyphIndex (juce_wchar c)
{
    for (int i = 0; kGlyphLetters[i] != 0; ++i)
        if (kGlyphLetters[i] == (char) CharacterFunctions::toUpperCase (c))
            return i;
    return -1;
}

void addGlyph (Path& out, const float* data, float x0)
{
    int i = 1; // data[0] = advance
    for (;;)
    {
        const int n = (int) data[i++];
        if (n == 0) break;
        const int count = std::abs (n);
        // Contours are baked in the right orientation already: positive = solid, negative = hole.
        for (int k = 0; k < count; ++k)
        {
            const float x = x0 + data[i + 2 * k], y = data[i + 2 * k + 1];
            if (k == 0) out.startNewSubPath (x, y); else out.lineTo (x, y);
        }
        out.closeSubPath();
        i += 2 * count;
    }
}
}

bool hasGlyph (juce_wchar c) { return glyphIndex (c) >= 0; }

bool canSpell (const String& text)
{
    for (auto c : text)
        if (c != ' ' && ! hasGlyph (c)) return false;
    return true;
}

Path wordPath (const String& text)
{
    Path p;
    p.setUsingNonZeroWinding (true);
    float x = 0.0f;
    for (auto c : text)
    {
        if (c == ' ') { x += kSpaceAdvance; continue; }
        const int gi = glyphIndex (c);
        if (gi < 0) continue;
        addGlyph (p, kGlyphTable[gi], x);
        x += kGlyphTable[gi][0] + kTrack;
    }
    return p;
}

void drawWord (Graphics& g, const String& text, Rectangle<float> area)
{
    auto path = wordPath (text);
    if (path.isEmpty()) return;
    const auto b = path.getBounds();
    const float s = jmin (area.getWidth() / b.getWidth(), area.getHeight() / b.getHeight());
    const auto t = AffineTransform::translation (-b.getCentreX(), -b.getCentreY())
                       .scaled (s).translated (area.getCentreX(), area.getCentreY());
    path.applyTransform (t);
    const auto fb = path.getBounds();

    g.setColour (Colours::black.withAlpha (0.75f));
    g.fillPath (path, AffineTransform::translation (1.2f, 2.0f));
    // dark outline: stroked UNDER the fill, so only its outer half shows and the overlaps
    // between the separate stroke contours never leave seams inside a letter
    g.setColour (Colours::black.withAlpha (0.92f));
    g.strokePath (path, PathStrokeType (jmax (1.0f, s * 2.4f), PathStrokeType::mitered, PathStrokeType::butt));
    fillChrome (g, path, fb);
    // steel shading: darker towards the bottom so the spikes read as cut metal
    ColourGradient shade (Colours::white.withAlpha (0.10f), 0, fb.getY(), Colours::black.withAlpha (0.40f), 0, fb.getBottom(), false);
    g.setGradientFill (shade);
    g.fillPath (path);
}
}
