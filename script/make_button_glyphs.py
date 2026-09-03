#!/usr/bin/env python3
"""Draws the PS Vita face-button glyphs as PNGs for the launcher.

The launcher used to write the buttons as box-drawing characters -- a
hollow circle for O, a lozenge for X -- because that is what the font had.
They read as punctuation next to the words they belong with.

These are drawn instead: the four face-button shapes, white on transparent,
so the launcher can tint them to whatever the theme wants.  They are our own
drawings of the shapes, not Sony's artwork, which is not ours to ship.

Rendered at 4x and boxed down, which is what gives them smooth edges without
a drawing library.

    python3 script/make_button_glyphs.py asset
"""

import math
import os
import struct
import sys
import zlib

SIZE = 32          # final glyph, px
SS = 4             # supersampling factor
R = SIZE * SS      # working resolution


def blank():
    return [[0.0] * R for _ in range(R)]


def stroke_circle(buf, cx, cy, radius, width):
    inner = radius - width / 2.0
    outer = radius + width / 2.0
    for y in range(R):
        for x in range(R):
            d = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
            if inner <= d <= outer:
                buf[y][x] = 1.0


def stroke_segment(buf, x0, y0, x1, y1, width):
    """Distance to a line segment, so the ends are round and joins are clean."""
    dx, dy = x1 - x0, y1 - y0
    length2 = dx * dx + dy * dy
    half = width / 2.0
    for y in range(R):
        for x in range(R):
            px, py = x + 0.5 - x0, y + 0.5 - y0
            t = 0.0 if length2 == 0 else max(0.0, min(1.0, (px * dx + py * dy) / length2))
            d = math.hypot(px - t * dx, py - t * dy)
            if d <= half:
                buf[y][x] = 1.0


def downsample(buf):
    """Box filter back to SIZE, which is where the anti-aliasing comes from."""
    out = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            total = 0.0
            for sy in range(SS):
                for sx in range(SS):
                    total += buf[y * SS + sy][x * SS + sx]
            row.append(total / (SS * SS))
        out.append(row)
    return out


def write_png(path, alpha):
    """White pixels, the shape in the alpha channel, so it can be tinted."""
    raw = bytearray()
    for row in alpha:
        raw.append(0)                      # filter: none
        for a in row:
            v = int(round(a * 255))
            raw += bytes((255, 255, 255, v))

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def glyph_circle():
    buf = blank()
    stroke_circle(buf, R / 2, R / 2, R * 0.31, R * 0.085)
    return buf


def glyph_cross():
    buf = blank()
    m = R * 0.26
    stroke_segment(buf, m, m, R - m, R - m, R * 0.085)
    stroke_segment(buf, R - m, m, m, R - m, R * 0.085)
    return buf


def glyph_square():
    buf = blank()
    m = R * 0.27
    w = R * 0.085
    stroke_segment(buf, m, m, R - m, m, w)
    stroke_segment(buf, R - m, m, R - m, R - m, w)
    stroke_segment(buf, R - m, R - m, m, R - m, w)
    stroke_segment(buf, m, R - m, m, m, w)
    return buf


def glyph_triangle():
    buf = blank()
    w = R * 0.085
    top = (R / 2, R * 0.22)
    left = (R * 0.23, R * 0.75)
    right = (R * 0.77, R * 0.75)
    stroke_segment(buf, top[0], top[1], right[0], right[1], w)
    stroke_segment(buf, right[0], right[1], left[0], left[1], w)
    stroke_segment(buf, left[0], left[1], top[0], top[1], w)
    return buf


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "asset"
    os.makedirs(out_dir, exist_ok=True)
    for name, maker in (("circle", glyph_circle), ("cross", glyph_cross),
                        ("square", glyph_square), ("triangle", glyph_triangle)):
        path = os.path.join(out_dir, "btn_%s.png" % name)
        write_png(path, downsample(maker()))
        print("wrote", path)


if __name__ == "__main__":
    main()
