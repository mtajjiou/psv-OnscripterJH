#!/usr/bin/env python3
"""Turns a set of downloaded PS Vita button icons into the launcher's assets.

Most of these icons are drawn for light backgrounds: a dark filled body with
the symbol knocked out of it in a lighter shade.  The launcher's interface is
nearly black, so used as they are they would be a dark shape on a dark panel,
and they are inverted in luminance -- light body, dark symbol, the way a
button prompt is drawn on a dark interface.

Not all of them, though, and that matters: in the set this was written for
the shoulder buttons are already light-bodied while everything else is dark.
Inverting the whole set turns those two dark, which is precisely the bug this
paragraph exists to prevent.  So each icon is measured and inverted only if
its body is the dark kind, which leaves every one of them light-bodied
whatever it started as.

They are also trimmed to what they actually draw and scaled to a common
height, so a row of them lines up regardless of how much empty margin each
one arrived with.  Height rather than width, because the shoulder and
start/select icons are wide shapes with a word inside them: squaring those
off would shrink the word until it was a smudge.

    python3 script/import_button_icons.py <source-dir> [asset]

Nothing here is fetched: point it at files you already have.  Whatever
licence they carry is unchanged by any of this, and the console makers'
button artwork is trademarked -- see asset/README.md.
"""

import os
import struct
import sys
import zlib

# Close to the size they are drawn at -- the interface asks for roughly 15
# to 22 pixels of height -- so the console is minifying by about two rather
# than by five.  Scaling a 100 pixel icon straight down to 15 is what makes
# it look chewed.
OUT_HEIGHT = 40
MAX_WIDTH = 256

# The file each of the launcher's glyphs comes from.  Names follow the set
# these were taken from; a set that names them differently needs this table
# adjusted, and nothing else.
WANTED = {
    "btn_circle":   ("ButtonIcon-PSvita-Circle.png",       "Vita_Circle.png"),
    "btn_cross":    ("ButtonIcon-PSvita-Cross.png",        "Vita_Cross.png"),
    "btn_square":   ("ButtonIcon-PSvita-Square.png",       "Vita_Square.png"),
    "btn_triangle": ("ButtonIcon-PSvita-Triangle.png",     "Vita_Triangle.png"),
    "btn_l":        ("ButtonIcon-PSvita-Bumper_Left.png",  "Vita_Bumpter_Left.png"),
    "btn_r":        ("ButtonIcon-PSvita-Bumper_Right.png", "Vita_Bumper_Right.png"),
    "btn_start":    ("ButtonIcon-PSvita-Start.png",        "Vita_Start.png"),
    "btn_select":   ("ButtonIcon-PSvita-Select.png",       "Vita_Select.png"),
    "btn_dpad":     ("ButtonIcon-PSvita-Dpad.png",         "Vita_Dpad.png"),
    "btn_lstick":   ("ButtonIcon-PSvita-Left_Stick.png",   "Vita_Left_Stick.png"),
}


def decode_png(path):
    """Enough of a PNG reader for 8-bit RGBA, which is what these are."""
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("%s: not a png" % path)

    width, height, depth, color = struct.unpack(">IIBB", data[16:26])
    if depth != 8 or color != 6:
        raise ValueError("%s: need 8-bit rgba, got depth %d type %d"
                         % (path, depth, color))

    idat = b""
    i = 8
    while i < len(data):
        length = struct.unpack(">I", data[i:i + 4])[0]
        tag = data[i + 4:i + 8]
        if tag == b"IDAT":
            idat += data[i + 8:i + 8 + length]
        i += 12 + length

    raw = zlib.decompress(idat)
    stride = width * 4
    out = bytearray()
    prev = bytearray(stride)
    pos = 0
    for _ in range(height):
        filt = raw[pos]
        pos += 1
        line = bytearray(raw[pos:pos + stride])
        pos += stride
        for x in range(stride):
            a = line[x - 4] if x >= 4 else 0
            b = prev[x]
            c = prev[x - 4] if x >= 4 else 0
            if filt == 1:
                line[x] = (line[x] + a) & 255
            elif filt == 2:
                line[x] = (line[x] + b) & 255
            elif filt == 3:
                line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif filt == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pred) & 255
        out += line
        prev = line
    return width, height, bytes(out)


def alpha_bounds(width, height, px):
    """The box the icon actually draws in, so margins do not decide its size."""
    left, top, right, bottom = width, height, -1, -1
    for y in range(height):
        for x in range(width):
            if px[(y * width + x) * 4 + 3] > 8:
                left = min(left, x)
                right = max(right, x)
                top = min(top, y)
                bottom = max(bottom, y)
    if right < 0:
        return 0, 0, width, height
    return left, top, right + 1, bottom + 1


def resample(width, height, px, box):
    """Area average to a fixed height, keeping the icon's own proportions."""
    left, top, right, bottom = box
    src_w, src_h = right - left, bottom - top

    dst_h = OUT_HEIGHT
    dst_w = max(1, min(MAX_WIDTH, int(round(src_w * float(dst_h) / src_h))))

    out = bytearray(dst_w * dst_h * 4)
    for dy in range(dst_h):
        y0 = top + int(dy * src_h / dst_h)
        y1 = max(y0 + 1, top + int((dy + 1) * src_h / dst_h))
        for dx in range(dst_w):
            x0 = left + int(dx * src_w / dst_w)
            x1 = max(x0 + 1, left + int((dx + 1) * src_w / dst_w))

            r = g = b = a = n = 0
            for sy in range(y0, y1):
                for sx in range(x0, x1):
                    i = (sy * width + sx) * 4
                    # Weight colour by coverage, or transparent pixels drag
                    # the edges toward whatever colour they happen to carry.
                    av = px[i + 3]
                    r += px[i] * av
                    g += px[i + 1] * av
                    b += px[i + 2] * av
                    a += av
                    n += 1
            o = (dy * dst_w + dx) * 4
            if a > 0:
                out[o] = min(255, r // a)
                out[o + 1] = min(255, g // a)
                out[o + 2] = min(255, b // a)
                out[o + 3] = a // n
    return dst_w, dst_h, bytes(out)


def body_is_dark(px):
    """Is the icon's body the dark kind?

    Averaged over what it actually draws, weighted by coverage, so the
    answer is about the body and not about a few bright pixels of symbol."""
    total = weight = 0
    for i in range(0, len(px), 4):
        a = px[i + 3]
        if a > 16:
            total += px[i] * a
            weight += a
    return weight > 0 and (total // weight) < 128


def invert_luminance(px):
    """Light body, dark symbol: the same drawing, read on a dark screen."""
    out = bytearray(px)
    for i in range(0, len(out), 4):
        if out[i + 3] == 0:
            continue
        out[i] = 255 - out[i]
        out[i + 1] = 255 - out[i + 1]
        out[i + 2] = 255 - out[i + 2]
    return bytes(out)


def write_png(path, width, height, px):
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw += px[y * width * 4:(y + 1) * width * 4]

    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload +
                struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    src_dir = sys.argv[1]
    out_dir = sys.argv[2] if len(sys.argv) > 2 else "asset"
    os.makedirs(out_dir, exist_ok=True)

    missing = []
    for name, candidates in sorted(WANTED.items()):
        source = None
        for candidate in candidates:
            path = os.path.join(src_dir, candidate)
            if os.path.exists(path):
                source = path
                break
        if source is None:
            missing.append("%s (looked for %s)" % (name, ", ".join(candidates)))
            continue

        width, height, px = decode_png(source)
        box = alpha_bounds(width, height, px)
        dst_w, dst_h, scaled = resample(width, height, px, box)
        dark = body_is_dark(scaled)
        if dark:
            scaled = invert_luminance(scaled)

        out_path = os.path.join(out_dir, name + ".png")
        write_png(out_path, dst_w, dst_h, scaled)
        print("%-17s %3dx%-3d %-18s <- %s"
              % (name + ".png", dst_w, dst_h,
                 "inverted" if dark else "already light",
                 os.path.basename(source)))

    for m in missing:
        print("skipped: %s" % m, file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
