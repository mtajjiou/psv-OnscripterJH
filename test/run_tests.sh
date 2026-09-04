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

# --- the archive readers, shared by the tests that need them ---------------
#
# The vendored LZMA SDK is third-party C compiled with warnings off: it is
# not this project's code to tidy, and its warnings would bury ours.
mkdir -p "$work/lzma"
for f in "$root"/src/common/lzma/*.c; do
  ${CC:-cc} -c -O2 -w -I"$root/src/common/lzma" "$f" -o "$work/lzma/$(basename "$f" .c).o"
done
${CC:-cc} -c -std=c99 -Wall -Wextra -g -I"$root/src/common" -I"$root/src/common/lzma" \
  "$root/src/common/sevenzip.c" -o "$work/sevenzip.o"
${CC:-cc} -c -std=c99 -Wall -Wextra -g -I"$root/src/common" -I"$root/src/common/lzma" \
  "$root/src/common/archive.c" -o "$work/archive.o"
${CC:-cc} -c -std=c99 -Wall -Wextra -g -I"$root/src/common" \
  "$root/src/common/zipreader.c" -o "$work/zipreader.o"
ar rcs "$work/libarchive.a" "$work/archive.o" "$work/sevenzip.o" \
  "$work/zipreader.o" "$work"/lzma/*.o

# --- either kind of archive, through one reader ----------------------------
${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_archive.c" "$work/libarchive.a" \
  -lz -o "$work/test_archive"

"$work/test_archive" "$work"

# --- zip reader -----------------------------------------------------------
${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_zipreader.c" "$root/src/common/zipreader.c" \
  -lz -o "$work/test_zipreader"

"$work/test_zipreader" "$work"

# --- the install decision chain, end to end -------------------------------
${CXX:-c++} -std=c++11 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_install_flow.cpp" "$root/src/common/installname.cpp" \
  "$work/libarchive.a" \
  -lz -o "$work/test_install_flow"

"$work/test_install_flow" "$work"

# --- finding a file whose name is spelled differently -----------------------
${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_pathmatch.c" "$root/src/common/pathmatch.c" \
  -o "$work/test_pathmatch"

"$work/test_pathmatch" "$work"

# --- the heap report -------------------------------------------------------
${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_memreport.c" "$root/src/common/memreport.c" \
  "$root/src/common/logfile.c" \
  -o "$work/test_memreport"

"$work/test_memreport"

# --- plugin manifests ------------------------------------------------------
${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_plugins.c" "$root/src/common/plugins.c" \
  -o "$work/test_plugins"

"$work/test_plugins"

# --- the ftp client's reading half -----------------------------------------
${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_ftpproto.c" "$root/src/common/ftpproto.c" \
  -o "$work/test_ftpproto"

"$work/test_ftpproto"

# --- the wifi upload page's parsing ----------------------------------------
${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_httpd.c" "$root/src/common/httpd.c" \
  -o "$work/test_httpd"

"$work/test_httpd"

# --- reading a game out of its archive -------------------------------------
${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_zipfs.c" "$root/src/common/zipfs.c" \
  "$root/src/common/zipreader.c" \
  -lz -o "$work/test_zipfs"

"$work/test_zipfs" "$work"

# --- patch (overlay) decisions --------------------------------------------
${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_patchplan.c" "$root/src/common/patchplan.c" \
  "$work/libarchive.a" \
  -lz -o "$work/test_patchplan"

"$work/test_patchplan" "$work"

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

# --- log file -------------------------------------------------------------
${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_logfile.c" "$root/src/common/logfile.c" \
  -o "$work/test_logfile"

"$work/test_logfile" "$work"

# --- reading the end of a log ---------------------------------------------
${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_logtail.c" "$root/src/common/logtail.c" \
  -o "$work/test_logtail"

"$work/test_logtail" "$work"

# --- crash report ---------------------------------------------------------
${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_crashreport.c" "$root/src/common/crashreport.c" \
  -o "$work/test_crashreport"

"$work/test_crashreport" "$work"

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
