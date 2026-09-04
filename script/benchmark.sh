#!/bin/sh
# Times the portable half of the launcher: inflating archives, the scan
# cache, and reading the end of a log.
#
#   sh script/benchmark.sh
#
# What it is for: telling "the code got slower" from "the card is slow".
# A Vita install is bound by writes to the memory card, which no host
# measurement can stand in for -- so run this before and after a change and
# compare the two runs, rather than comparing a number here to the console.
#
# For the console's own numbers, turn on the debug log and play a game: the
# engine reports what a screen flush costs and what a hundred characters of
# text actually took, and the launcher learns an install's throughput and
# uses it for its own estimates.
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
work=${TMPDIR:-/tmp}/onsjh-bench.$$
mkdir -p "$work"
trap 'rm -rf "$work"' EXIT

python3 "$root/test/make_fixtures.py" "$work" >/dev/null 2>&1

${CC:-cc} -std=c99 -O2 -Wall -Wextra \
  -I"$root/src/common" \
  "$root/test/bench.c" \
  "$root/src/common/zipreader.c" \
  "$root/src/common/manifest.c" \
  "$root/src/common/logtail.c" \
  -lz -o "$work/bench"

"$work/bench" "$work"
