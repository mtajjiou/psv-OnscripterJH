#!/usr/bin/env python3
"""Build the ZIP fixtures used by test_zipreader.c.

Written out at test time rather than committed so the repository does not
carry binary blobs that are hard to review.
"""
import os
import shutil
import sys
import struct
import warnings
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


    # --- the awkward archives -----------------------------------------
    #
    # Everything below is something a real download has been seen to
    # contain.  They are fixtures rather than notes because each one is a
    # way the installer could go wrong quietly: a name it mangles, a file it
    # truncates, an entry it should refuse.

    # Names in the alphabets these games actually ship in, plus the
    # punctuation a release group puts in a folder name.
    with zipfile.ZipFile(os.path.join(out_dir, "names.zip"), "w",
                         zipfile.ZIP_DEFLATED) as z:
        z.writestr("\u6708\u59eb/nscript.dat", "japanese folder\n")
        z.writestr("\u6708\u59eb/\u80cc\u666f.png", "PNG")
        z.writestr("\u6708\u59eb/\u0440\u0443\u0441\u0441\u043a\u0438\u0439.txt", "cyrillic name")
        z.writestr("\u6708\u59eb/a file with spaces.txt", "spaces")
        z.writestr("\u6708\u59eb/[patch] v1.2 (final).txt", "brackets")
        z.writestr("\u6708\u59eb/caf\u00e9 & co's.txt", "accents")
        z.writestr("\u6708\u59eb/dots.in.the.name.txt", "dots")
        z.writestr("\u6708\u59eb/UPPER.TXT", "upper")
        z.writestr("\u6708\u59eb/upper.txt", "lower")

    # A name at the edge of what the reader will take, and one past it.
    with zipfile.ZipFile(os.path.join(out_dir, "longnames.zip"), "w",
                         zipfile.ZIP_DEFLATED) as z:
        z.writestr("game/nscript.dat", "long name game\n")
        z.writestr("game/" + ("a" * 400) + ".txt", "long but acceptable")
        z.writestr("game/" + ("b" * 600) + ".txt", "too long to name")

    # Sizes: an empty file, and one big enough that it is read in several
    # chunks rather than one.
    with zipfile.ZipFile(os.path.join(out_dir, "sizes.zip"), "w",
                         zipfile.ZIP_DEFLATED) as z:
        z.writestr("game/nscript.dat", "sizes game\n")
        z.writestr("game/empty.dat", "")
        z.writestr("game/big.nsa", ("compressible" * 8) * 40000)   # ~3.8MB

    # A symlink, which is what an archive made on a mac or linux can
    # contain and which the Vita's filesystem has no concept of.
    sym = zipfile.ZipFile(os.path.join(out_dir, "symlink.zip"), "w",
                          zipfile.ZIP_DEFLATED)
    sym.writestr("game/nscript.dat", "symlink game\n")
    info = zipfile.ZipInfo("game/link.txt")
    info.create_system = 3                      # unix
    info.external_attr = (0xA1FF << 16)         # S_IFLNK | 0777
    sym.writestr(info, "../../../ux0:data/somewhere")
    sym.close()

    # Two entries with the same name, which some packers produce when a
    # patch is added over a release.  python warns about it, which is the
    # point of the fixture rather than a problem with it.
    warnings.filterwarnings("ignore", message="Duplicate name")
    with zipfile.ZipFile(os.path.join(out_dir, "duplicate.zip"), "w",
                         zipfile.ZIP_DEFLATED) as z:
        z.writestr("game/nscript.dat", "the original\n")
        z.writestr("game/nscript.dat", "the patch\n")

    # zip64, which the reader refuses on purpose.  Written by hand rather
    # than by producing four gigabytes: python only emits the zip64
    # structures when an archive actually needs them, and what has to be
    # tested is that the reader recognises them, not that python writes
    # them.  Two shapes, because zip64 announces itself in two ways.
    plain = os.path.join(out_dir, "_plain64.zip")
    with zipfile.ZipFile(plain, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("game/nscript.dat", "zip64 game\n")
        z.writestr("game/data.nsa", "x" * 200)
    raw = open(plain, "rb").read()
    os.remove(plain)

    eocd = raw.rfind(b"PK\x05\x06")

    # 1. The zip64 end-of-directory record and its locator, sitting where
    #    they do in a real zip64 archive: immediately before the EOCD.
    eocd64 = (b"PK\x06\x06" + struct.pack("<QHHIIQQQQ", 44, 45, 45, 0, 0, 2, 2,
                                          eocd, 0))
    locator = b"PK\x06\x07" + struct.pack("<IQI", 0, len(raw[:eocd]), 1)
    with open(os.path.join(out_dir, "zip64.zip"), "wb") as f:
        f.write(raw[:eocd] + eocd64 + locator + raw[eocd:])

    # 2. No locator, but an entry whose sizes are the 0xFFFFFFFF sentinel
    #    that says "the real size is in the zip64 extra field".  A reader
    #    that takes the sentinel at face value tries to extract four
    #    gigabytes from a two hundred byte file.
    data = bytearray(raw)
    pos = data.find(b"PK\x01\x02")
    struct.pack_into("<II", data, pos + 20, 0xFFFFFFFF, 0xFFFFFFFF)
    with open(os.path.join(out_dir, "zip64_sentinel.zip"), "wb") as f:
        f.write(bytes(data))

    # --- script encoding fixtures -------------------------------------
    # Real prose in each code page, wrapped in the ONScripter command lines
    # that surround dialogue in a script, so the samples look like the thing
    # the detector actually reads.
    jp = ("*define\n"
          "game\n"
          "*start\n"
          "\u3053\u3093\u306b\u3061\u306f\u3001\u4e16\u754c\u3002"
          "\u5f7c\u5973\u306f\u9759\u304b\u306b\u7b11\u3063\u3066\u3044\u305f\u3002\n"
          "\u6708\u304c\u96f2\u306b\u96a0\u308c\u308b\u307e\u3067\u3001"
          "\u50d5\u306f\u305d\u3053\u306b\u7acb\u3063\u3066\u3044\u305f\u3002\n") * 20
    with open(os.path.join(out_dir, "script_sjis.bin"), "wb") as f:
        f.write(jp.encode("cp932"))

    cn = ("*define\n"
          "game\n"
          "*start\n"
          "\u4f60\u597d\uff0c\u4e16\u754c\u3002"
          "\u5979\u5b89\u9759\u5730\u5fae\u7b11\u7740\u3002\n"
          "\u76f4\u5230\u6708\u4eae\u85cf\u8fdb\u4e91\u91cc\uff0c"
          "\u6211\u4e00\u76f4\u7ad9\u5728\u90a3\u91cc\u3002\n") * 20
    with open(os.path.join(out_dir, "script_gbk.bin"), "wb") as f:
        f.write(cn.encode("gbk"))

    # An English patch: no double-byte characters at all, so neither code
    # page is a better fit and the detector must decline.
    with open(os.path.join(out_dir, "script_ascii.bin"), "wb") as f:
        f.write(b"*define\ngame\n*start\nShe smiled quietly.\n" * 40)

    with open(os.path.join(out_dir, "notazip.bin"), "wb") as f:
        f.write(b"this is definitely not a zip archive" * 10)

def build_video(out_dir):
    """Encodes one second of colour bars in each format a game might ship.

    25fps because MPEG-1 only allows the broadcast frame rates.

    Skipped when ffmpeg is unavailable; the decoder tests skip with it.
    """
    import subprocess

    if shutil.which("ffmpeg") is None:
        return False

    common = ["-y", "-loglevel", "error",
              "-f", "lavfi", "-i", "testsrc=size=160x120:rate=25:duration=1",
              "-f", "lavfi", "-i", "sine=frequency=440:duration=1"]

    jobs = [
        # MPEG-1 in a program stream: what an old visual novel's .mpg is.
        (["-c:v", "mpeg1video", "-c:a", "mp2", "-f", "mpeg"], "clip_mpeg1.mpg"),
        # DivX-era AVI.
        (["-c:v", "mpeg4", "-c:a", "mp3", "-f", "avi"], "clip_mpeg4.avi"),
        # H.264 in MP4: the one the hardware accepts.
        (["-c:v", "libx264", "-pix_fmt", "yuv420p", "-c:a", "aac",
          "-f", "mp4"], "clip_h264.mp4"),
    ]
    for args, name in jobs:
        subprocess.run(["ffmpeg"] + common + ["-shortest"] + args +
                       [os.path.join(out_dir, name)], check=True)

    # A video with no audio track at all still has to play.
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error",
                    "-f", "lavfi", "-i", "testsrc=size=160x120:rate=25:duration=1",
                    "-c:v", "mpeg1video", "-f", "mpeg",
                    os.path.join(out_dir, "clip_silent.mpg")], check=True)

    # ...and an audio-only file is not a video, which is its own error.
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error",
                    "-f", "lavfi", "-i", "sine=frequency=440:duration=1",
                    os.path.join(out_dir, "audio_only.wav")], check=True)
    return True


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    build(out)
    build_video(out)
