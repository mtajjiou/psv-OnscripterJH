# psv-OnscripterJH (Easy Setup Edition)

**Plug & Play ONScripter for PSVita with automatic game installation**

This is an enhanced fork of [YuriSizuku/psv-OnscripterJH](https://github.com/YuriSizuku/psv-OnscripterJH) with a focus on **ease of use** and **automatic game setup**.

## Features

### ✨ Easy Setup Features

**Working today:**
- **ZIP install from the launcher** — drop a `.zip` in `ux0:data/game_zips/`, pick it in the game list, and it extracts into `ux0:onsemu/` with a progress bar
- **Auto game detection** — finds the game folder inside the archive by locating the script (`0.txt`, `00.txt`, `nscript.dat`, `nscr_sec.dat`, `nscript.___`, `onscript.nt2`, `onscript.nt3`), so both flat and nested archive layouts install correctly
- **Safe folder names** — destination names are reduced to ASCII, since the engine cannot open paths containing non-ASCII bytes
- **Space check** — refuses to start an install that would not fit, keeping a margin free
- **Clean failures** — a failed or canceled install removes what it wrote; corrupt, zip64, encrypted and script-less archives each report what is actually wrong
- **Video playback for formats the hardware refuses** — `sceAvPlayer` decodes only H.264/AAC in an MP4, and anything else used to fail inside it with no message, so the scene silently did nothing. The engine now reads the file's own bytes to identify the container and, when the hardware will not take it, decodes it in software with libavcodec: MPEG-1/2 (`.mpg`), MPEG-4/DivX and MSMPEG4 (`.avi`), WMV/VC-1, Theora, VP8/9 and more. Audio is resampled and fed to the mixer; cross or start skips, as with a hardware video. A converted `<name>.mp4` beside the original is still preferred when present, since the hardware decoder is cheaper
- **English, Chinese and Japanese interface** — the launcher's menus, settings, prompts, help and about screens were Chinese with a bracketed English gloss; every string now exists properly in all three languages (`src/onsjh_vitagui/GUI_Text.cpp`) and the **Language** row in the config menu cycles between them, each named in its own script. English is the default; set `language` to `en`, `zh` or `ja` under `[GUI]` in `ux0:data/onsemu/ONSConfig.ini`, or flip the setting. The Japanese strings are the author's own and have not been reviewed by a native speaker — corrections welcome
- **Version you can actually read** — the app version is derived from the git commit count at build time (`01.21`, `01.22`, ...), never edited by hand, so every build differs from the one before. The launcher shows it with the commit hash in its title bar, and both binaries print `ONS Easy Setup <version> (<commit>, <date>)` at startup, so a log always says which build produced it
- **Patches over an installed game** — a translation patch or voice pack is an archive with no script in it, and the installer used to refuse it twice over: no game inside, and a destination that already exists. Selecting one now asks which installed game it belongs to, likeliest first, extracts it over that game, and keeps the original of every file it replaces under `.mods/` in the game folder. **Patches** on the game's settings screen lists what is applied and takes one back off
- **Install without extracting** — **Install mode** in the settings cycles between *extract* and *keep compressed*. A compressed install writes out only what the engine opens as a file — `.nsa`/`.sar` archives, videos, fonts — keeps the archive as `game.zip` in the game folder, and reads everything else out of it while the game runs. A game whose art and audio are loose files roughly halves what it costs on the card; one that ships everything in an `arc.nsa` saves nothing, since that file goes to the card either way
- **Automatic script encoding** — the engine reads the script and decides whether it is Shift-JIS (Japanese) or GBK (Chinese), instead of making you know your game's code page. A Japanese game read as GBK garbles every line and dies with `text cannot be displayed in define section`; that no longer happens by default. The per-game **文字编码 (encoding)** setting is `自动(auto)` out of the box and can be forced to `日文(sjis)` or `中文(gbk)`; on the command line these are `--enc:auto`, `--enc:sjis` and `--enc:gbk`

**Not implemented yet** — see the checklist below:
game icons from the archive, per-game touch presets,
settings persistence, storage/save management, in-app help.

### ✅ Original Features (Preserved)
- Hardware-accelerated AVC video playback (MP4 support)
- nt2/nt3 encrypted script format support
- Lua scripting support
- Parallel processing support
- English UI translation

---

## Installation

### Prerequisites
- PSVita with VitaShell or similar homebrew launcher
- Vitasdk for building (see Build section)

This fork installs as **ONS Easy Setup** (title id `ONSEASY01`), separate
from upstream's *Vita Ons* (`VITAONSJH`), so both can be installed at once
and there is no doubt about which one you launched.

> Downloading the CI build gives you `VitaOns-vpk.zip`, because GitHub
> always wraps artifacts in a zip. Unzip it first and install the
> `VitaOns.vpk` inside — handing the outer zip to VitaShell fails with
> "incompatible or no content found".

### Quick Start

1. **Drop game ZIPs in `ux0:data/game_zips/`** (the launcher creates this
   folder on first run) — or just put them straight in `ux0:onsemu/`
   beside your existing games; both are scanned.
   ```
   ux0:/data/game_zips/        ux0:/onsemu/
   ├── game1.zip               ├── game2.zip      <- also found
   └── ...                     └── already_installed_game/
   ```

2. **Launch the app** — the list shows installed games first, then any
   archives waiting to be installed

3. **Select an archive** — a prompt shows the destination folder, the space
   needed and the space free; confirm to extract (CIRCLE cancels)

4. **Select the installed game** to configure and play

Already-extracted games in `ux0:onsemu/` keep working as before:

```
ux0:/onsemu/
├── game1/
│   ├── nscript.dat
│   └── ...
└── game2/
```

The archive is left in `game_zips/` after installing, so delete it yourself
once the game runs.

**Videos** still need converting to MP4 (AVC) on a PC beforehand — see
"Not implemented yet" above.

---

## Videos

Most videos play as they are. The engine decodes MPEG-1/2, MPEG-4, WMV/VC-1,
H.264, VP8/9, Theora and the rest in software — see `script/build_ffmpeg.sh`
for the built decoders — so a game's `.mpg` or `.avi` needs nothing done to
it.

Converting buys the *hardware* decoder, which takes H.264 with AAC in an MP4
and nothing else, and plays it at full frame rate for a fraction of the
battery a software decode costs. Worth doing if a video stutters or a long
session matters:

    sh script/convert_videos.sh ~/path/to/GameFolder --dry-run
    sh script/convert_videos.sh ~/path/to/GameFolder

Run it on a PC with `ffmpeg` installed. It writes `<name>.mp4` beside each
video and leaves the original alone — the engine prefers the `.mp4` when both
are there, so nothing in the game's script changes and you can delete the
originals whenever you are satisfied.

## Formats

What this build can open, and what to convert before copying a game across.
The same list is in the launcher: press R2 for the help screen, then SQUARE
for its second page.

*plays* needs nothing done to it, *slow* is decoded on the CPU and is fine
for the short clips a visual novel ships, *convert* will not open at all.

### Video

| Format | Files | | Notes |
|---|---|---|---|
| H.264 in MP4 | `.mp4 .m4v .mov` | plays | hardware decoded |
| MPEG-1/2 | `.mpg .mpeg` | slow | software decoded |
| MPEG-4 / DivX | `.avi` | slow | software decoded |
| WMV / VC-1 | `.wmv .asf` | slow | software decoded |
| VP8/VP9, Theora | `.webm .mkv .ogv` | slow | software decoded |
| RealVideo | `.rm .rmvb` | slow | software decoded |

### Audio

| Format | Files | | Notes |
|---|---|---|---|
| Ogg Vorbis | `.ogg` | plays | — |
| MP3 | `.mp3` | plays | — |
| WAV / PCM | `.wav` | plays | — |
| Opus | `.opus` | plays | — |
| FLAC | `.flac` | plays | — |
| Modules | `.mod .xm .it .s3m` | plays | — |
| MIDI | `.mid .midi` | convert | no soundfont on the Vita; convert to Ogg |

### Images

| Format | Files | | Notes |
|---|---|---|---|
| PNG | `.png` | plays | — |
| JPEG | `.jpg .jpeg` | plays | — |
| BMP | `.bmp` | plays | — |
| GIF | `.gif` | plays | first frame only |
| WebP | `.webp` | plays | — |
| TGA, TIFF | `.tga .tif .tiff` | convert | not linked in; convert to PNG |

The list comes from `src/common/formats.c`, which is also what the launcher
draws; change it there and both follow.

## Documentation

- [doc/FAQ.md](doc/FAQ.md) — what goes wrong, what it looks like, and what to do
- [doc/VIDEO.md](doc/VIDEO.md) — converting videos, and the ffmpeg flags that matter here
- [doc/COMPATIBILITY.md](doc/COMPATIBILITY.md) — games tested, and how to add one

## Per-game files

Two optional files can sit in a game's folder, and neither has to be there.

`caption.txt` names the game in the list. One line; the folder's own name is
used when it is missing or empty.

`ons_args` carries arguments for the engine, for anything the launcher has no
setting for. One per line, or several separated by spaces, and a line
starting with `#` is a note rather than an argument:

    # this game ships its own font
    --font-size 22
    --window

They are passed after everything the launcher sets — including anything it
worked out for itself, such as pointing a game with no `default.ttf` at the
launcher's own font — so a game that insists on something has the final word
on it.

The launcher writes two files of its own beside a game: `sittings.txt` holds
the settings chosen for it, and `lastplayed.txt` is a timestamp, which is
what the list sorts by in "recently played" order. Deleting either is
harmless.

## What's Improved (Detailed Checklist)

### Launcher UI/UX
- [x] **Game List Display** — Shows both extracted games and pending ZIPs
- [ ] **Game Icons** — Auto-loads `icon.png` from game folder/ZIP
- [x] **Game Info Panel** — Displays game name, size, last played date
- [x] **ZIP Info Tooltip** — Shows ZIP size and estimated extract time
- [ ] **Search/Filter** — Quick find games by name
- [ ] **Sort Options** — By name, date, size
- [x] **Controller Navigation** — D-pad to navigate, X to select, O to cancel
- [x] **Touch Support** — Tap to select, long-press for options

### ZIP Extraction System
- [x] **ZIP Handler Library** — Wrapper for PSVita's archive extraction
- [x] **Smart Path Detection** — Finds game root in nested archives
- [x] **Progress Indicator** — Shows extraction progress (%)
- [x] **Space Check** — Warns if insufficient storage before extracting
- [x] **Resume Extraction** — Can resume interrupted extractions
- [x] **Auto-Cleanup** — Removes failed partial extractions
- [ ] **Symlink Support** — Fallback for copy-on-write if extraction fails

### Game Detection & Config
- [x] **Script Finder** — Searches for `0.txt`, `00.txt`, `nscript.dat`, `nscr_sec.dat`, `nscript.___`, `onscript.nt2`, `onscript.nt3`
- [x] **Nested Structure Handler** — Finds game in subdirectories
- [x] **Config Parser** — Reads `ons_args`, `caption.txt`, config files
- [x] **Game Metadata Cache** — Stores game info in `game_manifest.json`
- [x] **Auto-Config Generator** — Works out per-game arguments at launch

### Video & Media Handling
- [ ] **Video Format Detector** — Checks for incompatible video formats
- [ ] **Graceful Degradation** — Skips missing/broken videos instead of crashing
- [x] **Video Conversion Helper** — Bundles `ffmpeg` instructions or helper script
- [ ] **Audio-Only Fallback** — Extracts audio if video fails
- [x] **Format Support List** — Shows which formats are playable

### Touch & Input Optimization
- [ ] **Auto-Detect Touch Needs** — Analyzes scripts for touch-dependent commands
- [x] **Touch Mode Presets** — `front_only`, `front_rear`, `rear_only`, `disabled`
- [x] **Per-Game Touch Settings** — Remembers last chosen mode
- [ ] **Vibration Control** — Enable/disable per game
- [ ] **Button Mapping UI** — Visual controller layout reference

### Storage & Memory Management
- [x] **Storage Monitor** — Shows free space on `ux0:` partition
- [x] **Compression Option** — Optional ZIP caching instead of full extraction
- [x] **Cleanup Tool** — Remove extracted games from menu, free space
- [x] **Save File Manager** — Backup/restore game saves
- [x] **Cache Cleaner** — Clear font cache, temp files

### Settings & Preferences
- [x] **Global Settings Menu** — Accessible from launcher
- [x] **Language Selection** — EN, JP, Chinese, etc.
- [x] **Text Speed Default** — Apply to all games
- [x] **Volume Presets** — BGM, SE, Voice level defaults
- [x] **Debug Mode** — Enable logging for troubleshooting
- [x] **Theme Support** — Dark/light mode for GUI

### Error Handling & Recovery
- [x] **Detailed Error Messages** — Specific hints on what went wrong
- [x] **Log Viewer** — In-app logs accessible from menu
- [x] **Recovery Options** — Retry, skip, or fallback actions
- [x] **Corruption Detection** — CRC checked per entry during extraction
- [x] **Crash Reporter** — Saves error logs for debugging

### Performance & Optimization
- [x] **Lazy Loading** — Loads game list asynchronously
- [x] **Icon Caching** — Pre-cache game icons for faster UI
- [~] **Background Tasks** — Extraction happens without blocking UI *(not planned: the bar and cancel already stay responsive; a worker thread would mean synchronising the game list for a convenience — see #49)*
- [x] **Memory Pooling** — Pre-allocate buffers for ZIP operations
- [~] **Parallel Extraction** — Multi-threaded ZIP if PSVita allows *(not planned: extraction is bound by the memory card, not by inflate — see #51)*

### Documentation & Help
- [x] **In-App Help** — Press SELECT for quick tips
- [x] **Setup Wizard** — First-run configuration
- [x] **Game Compatibility List** — Integrated or web-linked
- [x] **FAQ Section** — Common issues and solutions
- [x] **Video Conversion Guide** — Built-in ffmpeg instructions

### Testing & QA
- [x] **Unit Tests** — ZIP extraction, path parsing, config reading
- [x] **Integration Tests** — Full game launch workflows
- [~] **Compatibility Matrix** — Games tested and verified *(the document exists as [doc/COMPATIBILITY.md](doc/COMPATIBILITY.md); filling it in needs people playing games — see #59)*
- [x] **Performance Benchmarks** — Launch time, extraction speed
- [x] **Edge Cases** — Symlinks, special characters, large files, etc.

---

## Build Instructions

### Setup
```bash
# Install vitasdk
sh ./script/install_vitasdk.sh [vitasdkdir]

# No extra libraries are needed: ZIP support is built on zlib,
# which the engine and launcher already link against.
```

### Build
```bash
# Build the enhanced version
sh ./script/build_vitavpk.sh vpk [vitasdkdir]

# Output: build/VitaOns.vpk
```

### Deploy
```bash
# Send to PSVita over FTP
sh ./script/send_vitavpk.sh ./../build/VitaOns.vpk 10.2.12.6 ONSEASY01
```

### Tests

The archive handling is portable C and is tested on a host machine, with no
vitasdk or Vita required:

```bash
sh test/run_tests.sh
```

### Benchmarks

```bash
sh script/benchmark.sh
```

Times the same portable half: inflating archives, the scan cache, reading
the end of a log. **Host numbers, not Vita numbers** — an install on the
console is bound by writes to the memory card, which nothing here can stand
in for. Run it before and after a change and compare the two runs; that
tells "the code got slower" from "the card is slow", which timing an install
on the console cannot.

For the console's own numbers, turn on **Write a debug log** and play a
game. The engine reports what a screen flush costs, broken into its four
steps, and what a hundred characters of text actually took against what they
asked for; the launcher learns each install's throughput and uses it for the
estimate it shows before the next one.

---

## Project Structure

```
src/
├── onsjh/                      # Core engine
├── onsjh_vitagui/              # Launcher GUI
│   ├── GUI_Main.cpp            # Screens, main loop, install flow
│   ├── GUI_Utils.cpp           # Input, config, game list
│   ├── ZipHandler.cpp/.h       # Installs a game from a .zip
│   └── vitaPackage.cpp         # Shortcut bubble installer
└── common/
    ├── zipreader.c/.h          # Portable ZIP reader (stdio + zlib)
    ├── zipfs.c/.h              # Reads a game out of its .zip at run time
    ├── patchplan.c/.h          # Is this archive a patch, and for which game
    ├── filesystem.cpp
    ├── iniparser.c
    └── ...

test/
├── run_tests.sh                # Host-side test runner
├── test_zipreader.c            # Archive parsing / path safety tests
├── test_install_flow.cpp       # The install decisions, end to end
├── test_patchplan.c            # Patch detection, matching and records
├── test_zipfs.c                # Reading files out of a mounted archive
├── bench.c                     # What script/benchmark.sh runs
└── make_fixtures.py            # Builds the test archives

script/
├── build_vitavpk.sh
├── send_vitavpk.sh
└── install_vitasdk.sh
```

---

## Dependencies

No new dependencies. ZIP reading is implemented in `src/common/zipreader.c`
on top of **zlib**, which the engine and launcher already link against.

---

## Roadmap

### v1.0 (MVP) — done
- [x] ZIP file detection
- [x] Extraction UI with progress and cancel
- [x] Game auto-discovery inside archives
- [x] Install and launch from the launcher

### v1.1 (Polish)
- [ ] Game icons in launcher
- [ ] Video format warnings
- [ ] Settings persistence
- [ ] Touch mode presets

### v1.2 (Advanced)
- [ ] Game info parsing (caption, size)
- [ ] Storage manager
- [ ] Save file backup
- [ ] Compatibility checker

### v1.3 (Optimization)
- [ ] Parallel extraction
- [ ] Lazy loading UI
- [ ] Memory profiling
- [ ] Performance tuning

### v2.0 (Community)
- [ ] Web-based game list upload
- [ ] Cloud save sync (via FTP/SMB)
- [x] Mod loader integration
- [ ] Plugin system

---

## Contributing

Match the surrounding code style. Run `sh test/run_tests.sh` before sending a
change that touches archive handling.

---

## License

GNU General Public License v2.0 (original license preserved)

---

## Credits

**Original Project:**
- [YuriSizuku/psv-OnscripterJH](https://github.com/YuriSizuku/psv-OnscripterJH)
- [wetor/ONScripter-jh-PSVita](https://github.com/wetor/ONScripter-jh-PSVita)
- jh10001 — ONScripter-jh
- Ogapee — Original ONScripter

**Easy Setup Enhancement:**
- mtajjiou (this fork)

---

## Support

For issues, please create a GitHub issue with:
- Game name and source
- PSVita firmware version
- Error message/screenshot
- Steps to reproduce
