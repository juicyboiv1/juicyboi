#!/usr/bin/env python3
"""Generates Source/gui/MetalGlyphData.inc (baked, deterministic) and a preview PNG.

Glyph design: tall, narrow, straight-edged capitals in a jagged black-metal style.
Every stroke is a polyline with a width per vertex; ending a stroke at width 0
produces the long tapering spike that overshoots the letter body. No curves.
The only randomness (hairline splinters, tiny lean jitter) comes from a fixed
per-letter seed and is baked into the table, so the shapes never change.

Coordinates: x right, y DOWN, cap top y=0, baseline y=100, spikes reach about
y=-34 (above) and y=134 (below).  Original lettering, not a copy of any logo/font.
"""
import math, random, sys

def strokepoly(pts, ws, miter=2.2):
    n = len(pts)
    segs = []
    for i in range(n - 1):
        dx, dy = pts[i+1][0]-pts[i][0], pts[i+1][1]-pts[i][1]
        L = math.hypot(dx, dy) or 1
        segs.append((dx/L, dy/L))
    left, right = [], []
    for i in range(n):
        if i == 0: t = segs[0]
        elif i == n-1: t = segs[-1]
        else:
            a, b = segs[i-1], segs[i]
            t = (a[0]+b[0], a[1]+b[1]); L = math.hypot(*t) or 1; t = (t[0]/L, t[1]/L)
        nx, ny = -t[1], t[0]
        s = 1.0
        if 0 < i < n-1:
            a = segs[i-1]; c = a[0]*t[0] + a[1]*t[1]
            s = min(miter, 1.0 / max(c, 1e-3))
        h = ws[i] * 0.5 * s
        left.append((pts[i][0]+nx*h, pts[i][1]+ny*h))
        right.append((pts[i][0]-nx*h, pts[i][1]-ny*h))
    return left + right[::-1]

def area(p):
    return 0.5*sum(p[i][0]*p[(i+1) % len(p)][1]-p[(i+1) % len(p)][0]*p[i][1] for i in range(len(p)))

def S(pts, ws=None, w=13.0):
    """pts: [(x,y)] ; ws: widths per vertex or None -> w everywhere, tips (None) -> 0"""
    if ws is None: ws = [w]*len(pts)
    ws = [0.0 if v is None else v for v in ws]
    return ('stroke', pts, ws)

def tip(x, y): return (x, y, 0.0)

def glyph_def(c):
    W = 13.0
    if c == 'I':
        return 34, [
            S([(17,-36),(17,2),(17,98),(17,136)], [0,W,W,0]),
            S([(2,-4),(17,8)], [0,7]),            # little thorn at the top left
            S([(32,104),(17,92)], [0,7]),
        ]
    if c == 'J':
        return 62, [
            S([(44,-36),(44,2),(44,72),(38,92),(24,100),(9,92),(0,70)], [0,W,W,12,12,11,0]),
            S([(30,6),(44,-4)], [0,7]),
            S([(28,104),(30,136)], [7,0]),
        ]
    if c == 'U':
        return 64, [
            S([(10,-36),(10,2),(10,76),(22,96),(42,96),(54,76),(54,2),(54,-36)], [0,W,W,13,13,W,W,0]),
            S([(32,96),(31,136)], [12,0]),
            S([(-6,10),(10,22)], [0,7]),
            S([(70,10),(54,22)], [0,7]),
        ]
    if c == 'C':
        return 68, [
            S([(62,-34),(52,4),(18,12),(10,50),(18,88),(52,96),(62,134)], [0,W,W+1,W+2,W+1,W,0]),
            S([(14,50),(0,46),(-6,43)], [8,3,0]),
            S([(52,4),(34,-14)], [7,0]),
            S([(52,96),(34,116)], [7,0]),
        ]
    if c == 'Y':
        return 66, [
            S([(0,-34),(8,2),(33,50),(33,98),(33,136)], [0,W,W+1,W,0]),
            S([(66,-34),(58,2),(33,50)], [0,W,W+1]),
            S([(33,50),(33,98),(33,136)], [W+1,W,0]),
            S([(14,112),(33,98)], [0,6]),
        ]
    if c == 'B':
        return 76, [
            S([(12,-36),(12,2),(12,98),(12,136)], [0,W+1,W+1,0]),
            S([(12,6),(44,6),(55,18),(55,34),(44,48),(12,48)], None, 11.5),
            S([(12,50),(48,50),(60,64),(60,82),(48,94),(12,94)], None, 11.5),
            S([(55,18),(74,2),(80,-14)], [8,3,0]),     # thorn off the upper bowl
            S([(60,82),(76,98),(82,118)], [8,3,0]),    # thorn off the lower bowl
        ]
    if c == 'O':
        return 66, [
            ('poly', [(33,-8),(60,16),(60,84),(33,110),(6,84),(6,16)]),
            ('hole', [(33,16),(46,30),(46,70),(33,84),(20,70),(20,30)]),
            S([(33,-8),(40,-24),(44,-40)], [10,4,0]),
            S([(27,108),(22,124),(18,140)], [10,4,0]),
        ]
    raise KeyError(c)

LETTERS = 'IJUCYBO'

def build(c, lean=0.10):
    adv, items = glyph_def(c)
    rnd = random.Random(0x5150 + ord(c))
    polys = []
    def shear(p): return [(x + (50 - y) * lean, y) for x, y in p]
    strokes = []
    for it in items:
        if it[0] == 'stroke':
            strokes.append(it)
            p = strokepoly(it[1], it[2])
            if area(p) < 0: p = p[::-1]
            polys.append(('p', shear(p)))
        elif it[0] == 'poly':
            p = it[1]; p = p if area(p) > 0 else p[::-1]
            polys.append(('p', shear(p)))
        else:
            p = it[1]; p = p if area(p) < 0 else p[::-1]
            polys.append(('h', shear(p)))
    # hairline splinters alongside the long main strokes (fixed seed)
    for it in strokes:
        pts, ws = it[1], it[2]
        for i in range(len(pts)-1):
            (x0,y0),(x1,y1) = pts[i], pts[i+1]
            L = math.hypot(x1-x0, y1-y0)
            if L < 40 or min(ws[i], ws[i+1]) < 10: continue
            ux, uy = (x1-x0)/L, (y1-y0)/L; nx, ny = -uy, ux
            side = rnd.choice((-1, 1)); off = ws[i]*0.5 + 2.6 + rnd.random()*2.0
            a = rnd.uniform(0.05, 0.3); b = rnd.uniform(0.55, 0.95)
            lo = ((x0+ux*L*a)+nx*side*off, (y0+uy*L*a)+ny*side*off)
            hi = ((x0+ux*L*b)+nx*side*off, (y0+uy*L*b)+ny*side*off)
            ext = rnd.uniform(10, 26)
            end = (hi[0]+ux*ext, hi[1]+uy*ext)
            p = strokepoly([lo, hi, end], [0, 1.9, 0])
            if area(p) < 0: p = p[::-1]
            polys.append(('p', shear(p)))
    return adv + int(abs(lean)*50), polys

def emit(path):
    out = ["// GENERATED by tools/metal_glyphs.py - do not edit. Deterministic baked outlines.\n",
           "// Each glyph: advance, then contours as {count, x0,y0, x1,y1 ...}; a negative count is a hole.\n",
           "// Coordinates: cap top y=0, baseline y=100, y down; spikes reach about y=-40 / y=140.\n"]
    for c in LETTERS:
        adv, polys = build(c)
        out.append("static const float kGlyph_%s[] = { %d.0f,\n" % (c, adv))
        for kind, p in polys:
            n = len(p) * (1 if kind == 'p' else -1)
            out.append("    %d.0f, " % n + ", ".join("%.2ff, %.2ff" % (round(x,2), round(y,2)) for x, y in p) + ",\n")
        out.append("    0.0f };\n")
    out.append("static const char kGlyphLetters[] = \"%s\";\n" % LETTERS)
    out.append("static const float* const kGlyphTable[] = { %s };\n" % ", ".join("kGlyph_"+c for c in LETTERS))
    open(path, 'w').write("".join(out))

def preview(path, word='JUICYBOI', scale=4):
    from PIL import Image, ImageDraw
    x = 20; polys_all = []
    for ch in word:
        adv, polys = build(ch)
        polys_all.append((x, polys)); x += adv - 1
    Wd, Ht = int((x+60)*scale), int(210*scale)
    ss = 2
    im = Image.new('L', (Wd*ss, Ht*ss), 0)
    # nonzero winding rendering: draw polygons additively, holes subtract (approximation fine for preview)
    for ox, polys in polys_all:
        for kind, p in polys:
            d = ImageDraw.Draw(im)
            pts = [((ox+px)*scale*ss, (py+50)*scale*ss) for px, py in p]
            d.polygon(pts, fill=255 if kind == 'p' else 0)
    im = im.resize((Wd, Ht), Image.LANCZOS)
    bg = Image.new('RGB', im.size, (245, 215, 222))
    fg = Image.new('RGB', im.size, (70, 70, 74))
    bg.paste(fg, mask=im); bg.save(path)

if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'preview': preview(sys.argv[2])
    else: emit(sys.argv[1])
