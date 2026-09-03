# psv-OnscripterJH (Easy Setup Edition)

**Plug & Play ONScripter for PSVita with automatic game installation**

This is an enhanced fork of [YuriSizuku/psv-OnscripterJH](https://github.com/YuriSizuku/psv-OnscripterJH) with a focus on **ease of use** and **automatic game setup**.

## Features

### ✨ New Easy Setup Features
- **ZIP Auto-Extraction** — Drop a `.zip` file and the launcher handles extraction automatically
- **Unified Game Manager** — Browse and play extracted games and ZIP files from one interface
- **Auto-Game Detection** — Intelligently finds game scripts (`.ons`, `.txt`, `nscript.dat`, etc.)
- **Smart Video Handling** — Detects missing/incompatible videos and gracefully degrades
- **One-Click Launch** — Select ZIP or folder → Auto-extract → Play
- **Game Metadata** — Auto-reads game info from config files (caption.txt, ons_args)
- **Touch Mode Auto-Config** — Sets optimal touch controls per game
- **Memory Optimizer** — Auto-selects best settings for PSVita's constraints

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

### Quick Start

1. **Place game ZIPs in `ux0:data/game_zips/`**
   ```
   ux0:/data/game_zips/
   ├── game1.zip
   ├── game2.zip
   └── game3.zip
   ```

2. **Or extract games manually to `ux0:onsemu/`**
   ```
   ux0:/onsemu/
   ├── game1/
   │   ├── nscript.dat
   │   ├── script.txt
   │   └── ...
   └── game2/
       └── ...
   ```

3. **Launch the app** — Menu shows all available games
4. **Select a ZIP** → Auto-extracts to `ux0:onsemu/` (if space available)
5. **Select a folder** → Launches immediately
6. **Play!**

---

## What's Improved (Detailed Checklist)

### Launcher UI/UX
- [ ] **Game List Display** — Shows both extracted games and pending ZIPs
- [ ] **Game Icons** — Auto-loads `icon.png` from game folder/ZIP
- [ ] **Game Info Panel** — Displays game name, size, last played date
- [ ] **ZIP Info Tooltip** — Shows ZIP size and estimated extract time
- [ ] **Search/Filter** — Quick find games by name
- [ ] **Sort Options** — By name, date, size
- [ ] **Controller Navigation** — D-pad to navigate, X to select, O to cancel
- [ ] **Touch Support** — Tap to select, long-press for options

### ZIP Extraction System
- [ ] **ZIP Handler Library** — Wrapper for PSVita's archive extraction
- [ ] **Smart Path Detection** — Finds game root in nested archives
- [ ] **Progress Indicator** — Shows extraction progress (%)
- [ ] **Space Check** — Warns if insufficient storage before extracting
- [ ] **Resume Extraction** — Can resume interrupted extractions
- [ ] **Auto-Cleanup** — Removes failed partial extractions
- [ ] **Symlink Support** — Fallback for copy-on-write if extraction fails

### Game Detection & Config
- [ ] **Script Finder** — Searches for `.ons`, `.txt`, `nscript.dat`, `.scr`
- [ ] **Nested Structure Handler** — Finds game in subdirectories
- [ ] **Config Parser** — Reads `ons_args`, `caption.txt`, config files
- [ ] **Game Metadata Cache** — Stores game info in `game_manifest.json`
- [ ] **Auto-Config Generator** — Creates optimal `ons_args` per game

### Video & Media Handling
- [ ] **Video Format Detector** — Checks for incompatible video formats
- [ ] **Graceful Degradation** — Skips missing/broken videos instead of crashing
- [ ] **Video Conversion Helper** — Bundles `ffmpeg` instructions or helper script
- [ ] **Audio-Only Fallback** — Extracts audio if video fails
- [ ] **Format Support List** — Shows which formats are playable

### Touch & Input Optimization
- [ ] **Auto-Detect Touch Needs** — Analyzes scripts for touch-dependent commands
- [ ] **Touch Mode Presets** — `front_only`, `front_rear`, `rear_only`, `disabled`
- [ ] **Per-Game Touch Settings** — Remembers last chosen mode
- [ ] **Vibration Control** — Enable/disable per game
- [ ] **Button Mapping UI** — Visual controller layout reference

### Storage & Memory Management
- [ ] **Storage Monitor** — Shows free space on `ux0:` partition
- [ ] **Compression Option** — Optional ZIP caching instead of full extraction
- [ ] **Cleanup Tool** — Remove extracted games from menu, free space
- [ ] **Save File Manager** — Backup/restore game saves
- [ ] **Cache Cleaner** — Clear font cache, temp files

### Settings & Preferences
- [ ] **Global Settings Menu** — Accessible from launcher
- [ ] **Language Selection** — EN, JP, Chinese, etc.
- [ ] **Text Speed Default** — Apply to all games
- [ ] **Volume Presets** — BGM, SE, Voice level defaults
- [ ] **Debug Mode** — Enable logging for troubleshooting
- [ ] **Theme Support** — Dark/light mode for GUI

### Error Handling & Recovery
- [ ] **Detailed Error Messages** — Specific hints on what went wrong
- [ ] **Log Viewer** — In-app logs accessible from menu
- [ ] **Recovery Options** — Retry, skip, or fallback actions
- [ ] **Corruption Detection** — Checks ZIP integrity before extraction
- [ ] **Crash Reporter** — Saves error logs for debugging

### Performance & Optimization
- [ ] **Lazy Loading** — Loads game list asynchronously
- [ ] **Icon Caching** — Pre-cache game icons for faster UI
- [ ] **Background Tasks** — Extraction happens without blocking UI
- [ ] **Memory Pooling** — Pre-allocate buffers for ZIP operations
- [ ] **Parallel Extraction** — Multi-threaded ZIP if PSVita allows

### Documentation & Help
- [ ] **In-App Help** — Press SELECT for quick tips
- [ ] **Setup Wizard** — First-run configuration
- [ ] **Game Compatibility List** — Integrated or web-linked
- [ ] **FAQ Section** — Common issues and solutions
- [ ] **Video Conversion Guide** — Built-in ffmpeg instructions

### Testing & QA
- [ ] **Unit Tests** — ZIP extraction, path parsing, config reading
- [ ] **Integration Tests** — Full game launch workflows
- [ ] **Compatibility Matrix** — Games tested and verified
- [ ] **Performance Benchmarks** — Launch time, extraction speed
- [ ] **Edge Cases** — Symlinks, special characters, large files, etc.

---

## Build Instructions

### Setup
```bash
# Install vitasdk
sh ./script/install_vitasdk.sh [vitasdkdir]

# Install additional libraries for ZIP support
sh ./script/setup_dependencies.sh
```

### Build
```bash
# Build the enhanced version
sh ./script/build_vitavpk.sh vpk [vitasdkdir]

# Output: build/vita_onscripter.vpk
```

### Deploy
```bash
# Send to PSVita over FTP
sh ./script/send_vitavpk.sh ./../build/vita_onscripter.vpk 10.2.12.6 VITAONSJH
```

---

## Project Structure

```
src/
├── onsjh/                      # Core engine (unchanged)
│   ├── ONScripter.cpp
│   ├── ONScripter_*.cpp
│   └── ...
├── onsjh_vitagui/              # Launcher GUI (enhanced)
│   ├── GUI_Main.cpp            # Main entry point
│   ├── GUI_Utils.cpp
│   ├── GameManager.cpp         # NEW: Game discovery & launch
│   ├── GameManager.h
│   ├── ZipHandler.cpp          # NEW: ZIP extraction
│   ├── ZipHandler.h
│   ├── GameMetadata.cpp        # NEW: Config parsing
│   ├── GameMetadata.h
│   ├── VideoDetector.cpp       # NEW: Video validation
│   ├── VideoDetector.h
│   ├── StorageManager.cpp      # NEW: Space management
│   ├── StorageManager.h
│   ├── SettingsManager.cpp     # NEW: Persistent settings
│   ├── SettingsManager.h
│   ├── ErrorHandler.cpp        # NEW: Error & logging
│   ├── ErrorHandler.h
│   ├── UIRenderer.cpp          # NEW: Enhanced UI
│   ├── UIRenderer.h
│   └── vitaPackage.cpp         # Package installer
├── common/                     # Shared utilities
│   ├── filesystem.cpp
│   ├── dictionary.c
│   ├── iniparser.c
│   ├── sha1.cpp
│   └── unzip.c                 # NEW: ZIP library
└── CMakeLists.txt

script/
├── build_vitavpk.sh
├── send_vitavpk.sh
├── setup_dependencies.sh       # NEW: Install ZIP libs
└── setup_first_run.sh          # NEW: Create directories

doc/
├── SETUP_GUIDE.md              # Detailed user guide
├── DEVELOPER_GUIDE.md          # Code architecture
├── COMPATIBILITY.md            # Game compatibility list
└── CHANGELOG.md                # Version history
```

---

## Dependencies Added

- **minizip** — ZIP file extraction
- **zlib** — Compression support
- **vita2d_ext** — Already used, no change

---

## Roadmap

### v1.0 (MVP)
- [x] ZIP file detection
- [x] Basic extraction UI
- [x] Game auto-discovery
- [x] One-click launch

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
- [ ] Mod loader integration
- [ ] Plugin system

---

## Contributing

Please submit PRs against the `feature/easy-setup` branch. See `DEVELOPER_GUIDE.md` for code style and architecture.

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

---

Generated: 2026-09-03
