# Common problems

Everything here is something that has actually gone wrong, with what it
looks like from the outside first — that is what you have when you hit it.

## The launcher

### My game does not appear in the list

A game is a **folder** in `ux0:onsemu/` containing the script:
`nscript.dat`, `0.txt`, `00.txt`, `nscr_sec.dat`, `nscript.___`,
`onscript.nt2` or `onscript.nt3`. Both folders the
launcher uses are created for you at startup, and the help screen's third
page (R2, then left or right) names them.

Two things that look like this but are not:

- **The script is one level deeper.** A `.zip` often unpacks to
  `Game/Game/nscript.dat`. The launcher looks two levels down, so this
  normally works — but a third level does not. Move the folder with the
  script in it directly into `ux0:onsemu/`.
- **The folder is a half-finished install.** It is listed, but marked, and
  refuses to launch. Install its `.zip` again to finish it, or delete it.

### The list is stale — a game I deleted is still there

The launcher remembers what it learned about each folder in
`ux0:data/onsemu/game_manifest.json`. It notices a folder that changed, but
if you are ever in doubt, **Clear temporary files** in the settings removes
the cache and the next start rebuilds it from the card.

### Installing says "not enough space"

The dialog offers to clear temporary files and retry. That sweeps the
bubble installer's folder, leftover `tmp.mus` files and the scan cache. If
it is still short, the archive genuinely does not fit: the installed game is
roughly the uncompressed size, which the game panel shows before you start.

### An install stopped part way

Start it again. An interrupted install leaves a journal, so it **resumes**
from where it stopped rather than re-extracting the whole archive — the
dialog says how much is already there and the button reads *resume*.

### The archive is rejected

Three archives are refused, and they refuse the same way however many times
you try:

- **zip64** — an archive over 4 GB, or with more than 65535 entries. Repack
  it as a normal zip, or split it.
- **encrypted** — password-protected entries. Unpack it on a PC and copy the
  folder across.
- **no script inside** — the archive contains none of the script names
  above, so there is no game in it.

## Fonts and text

### The text is missing, or every character is a box

The game has no `default.ttf` and the engine had nothing to draw with. The
launcher now points the engine at its own font when a game ships none, so
this should not happen — if it does, put a `default.ttf` in the game folder.

### The text is mojibake

The engine detects Shift-JIS and GBK from the script itself, so this is
rare. If a script is misread, force it: the game's own settings screen has
**Script encoding**, with *japanese* and *chinese* beside *auto*.

### The system menu is in English on a Japanese game

Deliberate. The engine's built-in save/load menu uses Japanese glyphs; when
the game's font has none of them, the menu would draw as empty boxes, so it
falls back to English wording.

## In a game

### Which button does what

Hold **SELECT** during a game for the list. The short version: circle
advances, cross held is fast-forward, square is auto, triangle is the menu,
L skips the page, R toggles skipping.

### Text speed does nothing

Fixed, but worth knowing why it happened: most scripts set their own speed,
which used to win over yours for the whole game. It no longer does.

Note that speed is **per game**: the launcher's setting seeds a game the
first time it is played, and a game you have already played keeps what it
saved. To change one you have played, use that game's own settings screen —
the same is true of the three volume settings.

### A video plays no picture, only sound

The build has no decoder for that video's codec, so it plays what it can
rather than skipping the scene. Convert the file — see
[VIDEO.md](VIDEO.md).

### It crashes

Turn on **Write a debug log** in settings and reproduce it. Then:

- `ux0:data/onsemu/crash.txt` says which game, which label and line of its
  script, and the last file the engine opened;
- `ux0:data/onsemu/onsjh.log` has everything the engine printed.

Both are readable on the console — **View the log** in the settings screen,
with SQUARE cycling engine, launcher and last crash. That is what to attach
to a bug report.

## Files and folders

| Path | What it is |
|---|---|
| `ux0:onsemu/` | installed games, one folder each |
| `ux0:data/game_zips/` | archives waiting to be installed |
| `ux0:data/onsemu/onsjh.log` | what the engine printed, when logging is on |
| `ux0:data/onsemu/launcher.log` | what the launcher printed |
| `ux0:data/onsemu/crash.txt` | where the engine was when it last stopped |
| `ux0:data/onsemu/saves/<game>/` | save backups |
| `ux0:data/onsemu/game_manifest.json` | the launcher's scan cache |
| `<game>/sittings.txt` | that game's own settings |
| `<game>/caption.txt` | a name for the list, if you want one that is not the folder's |
| `<game>/ons_args` | extra engine arguments for that game, separated by spaces or newlines (up to 16, 63 characters each) |
