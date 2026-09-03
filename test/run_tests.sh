#!/bin/sh
# Host-side tests for the portable pieces of the easy-setup work.
# Needs a host compiler, zlib headers and python3 (to build the fixtures).
#
#   sh test/run_tests.sh
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
work=${TMPDIR:-/tmp}/onsjh-tests.$$
mkdir -p "$work"
trap 'rm -rf "$work"' EXIT

python3 "$root/test/make_fixtures.py" "$work"

${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_zipreader.c" "$root/src/common/zipreader.c" \
  -lz -o "$work/test_zipreader"

"$work/test_zipreader" "$work"

${CXX:-c++} -std=c++11 -Wall -Wextra -g \
  -I"$root/src/onsjh" \
  "$root/test/test_encoding_detect.cpp" "$root/src/onsjh/encoding_detect.cpp" \
  -o "$work/test_encoding_detect"

"$work/test_encoding_detect" "$work"

${CC:-cc} -std=c99 -Wall -Wextra -g \
  -I"$root/src/common" \
  "$root/test/test_videofmt.c" "$root/src/common/videofmt.c" \
  -o "$work/test_videofmt"

"$work/test_videofmt"
