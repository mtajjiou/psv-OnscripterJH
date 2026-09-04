# Game compatibility

**This list is nearly empty, and that is the honest state of it.** One game
has been played on this build; everything else below is a note about a
*kind* of game, not a report about a particular one. A compatibility list
that guesses is worse than one that admits what it does not know, because
people plan downloads around it.

If you play something, please add a row — see [Adding a
row](#adding-a-row). That is the only way this becomes useful.

## Tested

| Game | Engine | Status | Notes |
|---|---|---|---|
| Tsukihime (JP) | NScripter | plays | Shift-JIS detected automatically. Ships no `default.ttf`, so the launcher's font is used. Text speed, volumes and saves behave. Videos untested — this release has none in a format the build cannot decode. |

Status means: **plays** — played past the opening without trouble;
**partial** — runs, with something named in the notes not working;
**broken** — does not start, or stops part way.

## What generally works

These follow from what the build does rather than from testing, so treat
them as expectations, not results:

- **NScripter and ONScripter games** in Japanese (Shift-JIS) or Chinese
  (GBK). The encoding is detected from the script; the game's own settings
  screen can force it when a script is unusual.
- **Games in `.nsa`, `.sar` and `.ns2` archives**, including a game whose
  data is split across `arc.sar` and several `arc*.nsa`.
- **English fan translations**, including the ones that use backtick text —
  the build compiles with `ENABLE_1BYTE_CHAR`, which is what `clickstr` with
  a backtick needs.
- **Audio** as Ogg Vorbis, MP3, WAV, Opus, FLAC and tracker modules. MIDI
  does not play: the console has no soundfont.
- **Lua extensions** — luajit is linked in and the engine loads a game's
  `system.lua` when it has one. Untested: the one game played here has none,
  which is what the "cannot open system.lua" line in its log means.
- **Video** in MPEG-1/2, MPEG-4/DivX, WMV/VC-1, H.264, VP8/VP9, Theora and
  RealVideo, decoded in software; MP4/H.264 additionally gets the hardware
  decoder. See [VIDEO.md](VIDEO.md).

## What is known not to work

- **MIDI music** — no soundfont on the Vita. Convert to Ogg.
- **TGA and TIFF images** — not linked in. Convert to PNG.
- **zip64 and encrypted archives** — the installer refuses both. Repack, or
  unpack on a PC and copy the folder across.

## Adding a row

Play the game, then open a pull request adding one line to the table, or an
issue with the same fields. What makes a row useful:

- the **exact name and release** you played — a fan translation is not the
  same game as the original for these purposes;
- **how far you got** — "played past the prologue" is a real data point;
  "seems to work" is not;
- **what did not work**, precisely: which scene, which file, what appeared
  on screen instead;
- if something went wrong, the **crash report and log**. Turn on *Write a
  debug log* in settings, reproduce it, and attach
  `ux0:data/onsemu/crash.txt` and `onsjh.log`. Those two say which label and
  line the engine was on, which turns a report into something fixable.
