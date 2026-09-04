# Converting videos

**You probably do not need to.** This build decodes MPEG-1/2, MPEG-4/DivX,
WMV/VC-1, H.264, VP8/VP9, Theora and RealVideo in software, so a game's
`.mpg` or `.avi` plays as it is. See the Formats table in the
[README](../README.md#formats), or the second page of the launcher's help
screen (R2, then left or right).

What converting buys is the **hardware** decoder. `sceAvPlayer` takes H.264
with AAC in an MP4 and nothing else, and plays such a file at full frame
rate for a fraction of the battery a software decode costs. Convert if a
video stutters, or if a long session on battery matters.

## The easy way

    sh script/convert_videos.sh ~/path/to/GameFolder --dry-run
    sh script/convert_videos.sh ~/path/to/GameFolder

Run it on a PC with `ffmpeg` installed — not on the console; software
encoding on a 444 MHz Cortex-A9 runs far below realtime, which is why the
Vita build has no encoders in it at all.

It writes `<name>.mp4` beside each video and **leaves the original alone**.
The engine prefers the `.mp4` when both are there, so nothing in the game's
script changes and you can delete the originals whenever you are satisfied.

## The command it runs

If you would rather do it by hand, or script it yourself:

    ffmpeg -i input.mpg \
        -c:v libx264 -profile:v baseline -level 3.1 -pix_fmt yuv420p \
        -vf "scale='min(960,iw)':'min(544,ih)':force_original_aspect_ratio=decrease:force_divisible_by=2" \
        -crf 23 -preset slow \
        -c:a aac -b:a 128k -ar 44100 -ac 2 \
        -movflags +faststart \
        output.mp4

Every flag is there for a reason on this console:

| Flag | Why |
|---|---|
| `-profile:v baseline -level 3.1` | What the Vita's decoder accepts. Main and High profiles fail to open, usually silently. |
| `-pix_fmt yuv420p` | The only chroma format the decoder handles. |
| the `scale` filter | The screen is 960×544; anything larger is decoded and then thrown away. It fits inside that box, never enlarges a smaller video, keeps the aspect ratio, and rounds to even numbers — H.264 with `yuv420p` cannot encode an odd width or height. |
| `-crf 23 -preset slow` | Visually transparent for the material visual novels ship, at a size a memory card can live with. Lower is bigger and better; 18 is near-lossless. |
| `-c:a aac -ar 44100 -ac 2` | AAC is the only audio `sceAvPlayer` takes, and the mixer runs at 44100 Hz stereo — resampling elsewhere costs quality for nothing. |
| `-movflags +faststart` | Puts the index at the front of the file, so playback starts without seeking to the end of it first. |

## Checking the result

    ffprobe -v error -select_streams v:0 \
        -show_entries stream=codec_name,profile,level,width,height \
        -of default=noprint_wrappers=1 output.mp4

You want `codec_name=h264`, `profile=Constrained Baseline` or `Baseline`,
`level` at most `31`, and a width no greater than 960.

## When a video still does not play

Turn on **Write a debug log** in the launcher's settings, play the scene,
then read `ux0:data/onsemu/onsjh.log` — the log viewer in the settings screen
shows it on the console. The engine says which decoder it used and why it
gave up, which is usually one of:

- **the container is fine but the codec is not** — the file opens for its
  sound and plays that over the scene, which is the audio-only fallback;
- **the file is not a video at all** — a renamed archive, or a download that
  stopped early;
- **it plays but stutters** — software decoding a resolution larger than the
  screen. Convert it.
