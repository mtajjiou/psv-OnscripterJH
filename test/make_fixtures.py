#!/usr/bin/env python3
"""Build the ZIP fixtures used by test_zipreader.c.

Written out at test time rather than committed so the repository does not
carry binary blobs that are hard to review.
"""
import os
import sys
import zipfile

def build(out_dir):
    os.makedirs(out_dir, exist_ok=True)

    # Game files at the archive root.
    with zipfile.ZipFile(os.path.join(out_dir, "flat.zip"), "w",
                         zipfile.ZIP_DEFLATED) as z:
        z.writestr("nscript.dat", "flat game script\n")
        z.writestr("arc.nsa", "x" * 100)

    # The common layout: one folder wrapping everything.
    with zipfile.ZipFile(os.path.join(out_dir, "nested.zip"), "w",
                         zipfile.ZIP_DEFLATED) as z:
        z.writestr("MyGame/", "")
        z.writestr("MyGame/nscript.dat", "nested game script\n")
        z.writestr("MyGame/caption.txt", "My Game Title\n")
        # Big and repetitive: spans several inflate output chunks.
        z.writestr("MyGame/arc.nsa", "abcdefgh" * 40000)
        # Stored rather than deflated, to cover that path too.
        z.writestr("MyGame/icon.png", "PNGDATA" * 10,
                   compress_type=zipfile.ZIP_STORED)

    # Two levels deep, with a decoy script in a backup folder that must not
    # be picked over the real one.
    with zipfile.ZipFile(os.path.join(out_dir, "deep.zip"), "w",
                         zipfile.ZIP_DEFLATED) as z:
        z.writestr("outer/inner/0.txt", "deep game script\n")
        z.writestr("outer/inner/arc.nsa", "y" * 500)
        z.writestr("outer/inner/backup/old/nscript.dat", "decoy\n")

    # No recognisable script anywhere.
    with zipfile.ZipFile(os.path.join(out_dir, "noscript.zip"), "w",
                         zipfile.ZIP_DEFLATED) as z:
        z.writestr("readme.txt", "nothing to see\n")
        z.writestr("data/arc.nsa", "z" * 50)

    with open(os.path.join(out_dir, "notazip.bin"), "wb") as f:
        f.write(b"this is definitely not a zip archive" * 10)

if __name__ == "__main__":
    build(sys.argv[1] if len(sys.argv) > 1 else ".")
