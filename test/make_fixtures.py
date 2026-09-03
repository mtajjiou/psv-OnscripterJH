#!/usr/bin/env python3
"""Build the ZIP fixtures used by test_zipreader.c.

Written out at test time rather than committed so the repository does not
carry binary blobs that are hard to review.
"""
import os
import shutil
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
