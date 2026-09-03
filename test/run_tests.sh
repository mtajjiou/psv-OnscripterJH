#!/bin/sh
# Host-side tests for the portable pieces of the easy-setup work.
# Needs a host compiler, zlib headers and python3 (to build the fixtures).
# The video decoder tests additionally need ffmpeg's development libraries
# and the ffmpeg command line, and are skipped when those are absent.
#
#   sh test/run_tests.sh
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
work=${TMPDIR:-/tmp}/onsjh-tests.$$
mkdir -p "$work"
trap 'rm -rf "$work"' EXIT

python3 "$root/test/make_fixtures.py" "$work"

# --- zip reader -----------------------------------------------------------
${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_zipreader.c" "$root/src/common/zipreader.c" \
  -lz -o "$work/test_zipreader"

"$work/test_zipreader" "$work"

# --- script encoding detection --------------------------------------------
${CXX:-c++} -std=c++11 -Wall -Wextra -g \
  -I"$root/src/onsjh" \
  "$root/test/test_encoding_detect.cpp" "$root/src/onsjh/encoding_detect.cpp" \
  -o "$work/test_encoding_detect"

"$work/test_encoding_detect" "$work"

# --- video container sniffing ---------------------------------------------
${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_videofmt.c" "$root/src/common/videofmt.c" \
  -o "$work/test_videofmt"

"$work/test_videofmt"

# --- format support table -------------------------------------------------
${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_formats.c" "$root/src/common/formats.c" \
  -o "$work/test_formats"

"$work/test_formats" "$root/README.md"

# --- game metadata cache --------------------------------------------------
${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_manifest.c" "$root/src/common/manifest.c" \
  -o "$work/test_manifest"

"$work/test_manifest" "$work"

# --- launcher interface text ---------------------------------------------
${CXX:-c++} -std=c++11 -Wall -Wextra -g \
  -I"$root/src/onsjh_vitagui" \
  "$root/test/test_gui_text.cpp" "$root/src/onsjh_vitagui/GUI_Text.cpp" \
  -o "$work/test_gui_text"

"$work/test_gui_text"

# --- software video decoding ----------------------------------------------
# Skipped rather than failed on a machine without ffmpeg, but said out loud:
# a silent skip is how untested code ships.
if pkg-config --exists libavformat libavcodec libswscale libswresample 2>/dev/null \
   && [ -f "$work/clip_mpeg1.mpg" ]; then
  ${CC:-cc} -std=c99 -Wall -Wextra -g \
    -I"$root/src/common" \
    "$root/test/test_videodec.c" "$root/src/common/videodec.c" \
    $(pkg-config --cflags --libs libavformat libavcodec libswscale libswresample libavutil) \
    -o "$work/test_videodec"

  "$work/test_videodec" "$work"
else
  echo
  echo "SKIPPED test_videodec: needs ffmpeg development libraries and the"
  echo "ffmpeg command line to build the video fixtures."
fi
