#!/bin/sh
# Converts a game's videos to the one format the Vita decodes in hardware.
#
#   sh script/convert_videos.sh <game folder> [--dry-run]
#
# Run this on a PC, not on the console. It writes <name>.mp4 beside each
# video it converts and leaves the original alone; the engine prefers the
# .mp4 when both are there, so nothing in the game's script has to change.
#
# You do not have to run it at all. The engine decodes MPEG-1/2, MPEG-4,
# WMV/VC-1, H.264, VP8/9, Theora and the rest in software -- see
# script/build_ffmpeg.sh for the full set -- and only falls back to a
# message when a file is something else entirely. What converting buys is
# the hardware decoder: sceAvPlayer takes H.264 with AAC in an MP4 and
# nothing else, and it plays such a file at full frame rate for a fraction
# of the battery a software decode costs.
#
# So: convert if a video stutters or the battery matters. Otherwise leave it.
set -e

usage() {
    echo "usage: $0 <game folder> [--dry-run]" >&2
    exit 2
}

[ $# -ge 1 ] || usage
GAME=$1
DRY=0
[ "$2" = "--dry-run" ] && DRY=1

[ -d "$GAME" ] || { echo "$GAME is not a folder" >&2; exit 1; }

command -v ffmpeg >/dev/null 2>&1 || {
    echo "ffmpeg is not installed, or not on PATH." >&2
    echo "  macOS:  brew install ffmpeg" >&2
    echo "  debian: sudo apt install ffmpeg" >&2
    exit 1
}

# The console's screen. Anything larger is wasted pixels and battery; the
# scale keeps the aspect ratio and never enlarges a smaller video.
SCALE="scale='min(960,iw)':'min(544,ih)':force_original_aspect_ratio=decrease"

converted=0
skipped=0
failed=0

# -print0 would be tidier, but this has to run under the shells a mac and a
# debian box both have.
find "$GAME" -type f \( \
        -iname '*.mpg' -o -iname '*.mpeg' -o -iname '*.avi' -o \
        -iname '*.wmv' -o -iname '*.mkv' -o -iname '*.webm' -o \
        -iname '*.flv' -o -iname '*.rm' -o -iname '*.rmvb' -o \
        -iname '*.mov' -o -iname '*.m4v' -o -iname '*.ogv' \
    \) | while read -r video; do

    target="${video%.*}.mp4"

    if [ -f "$target" ]; then
        echo "skip    $video (already has $(basename "$target"))"
        skipped=$((skipped + 1))
        continue
    fi

    if [ "$DRY" = "1" ]; then
        echo "would   $video -> $(basename "$target")"
        continue
    fi

    echo "convert $video"
    # baseline profile and yuv420p because that is what the hardware
    # decoder will take; faststart so it begins without reading the whole
    # file first.
    if ffmpeg -nostdin -loglevel error -y -i "$video" \
            -c:v libx264 -profile:v baseline -level 3.1 -pix_fmt yuv420p \
            -vf "$SCALE" -crf 23 -preset slow \
            -c:a aac -b:a 128k -ar 44100 -ac 2 \
            -movflags +faststart \
            "$target"; then
        converted=$((converted + 1))
    else
        echo "FAILED  $video" >&2
        rm -f "$target"          # a half-written file is worse than none
        failed=$((failed + 1))
    fi
done

echo
echo "Done. The originals are untouched -- delete them once you are happy"
echo "with the .mp4 files, or keep both: the engine prefers the .mp4."
