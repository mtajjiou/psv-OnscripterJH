# What still needs a console

One thing, now. Everything else on this page has been run on hardware and
its issue is closed.

## Patches over an installed game — [#80](https://github.com/mtajjiou/psv-OnscripterJH/issues/80)

Two ways in. Selecting an archive with no script in it from the game list
asks which installed game it belongs to. Or open a game and press **Mods**,
which lists what is in `ux0:data/game_mods/` against that game and checks
whether the one you pick belongs there before writing anything. Either way
the original of every replaced file is kept under `.mods/` in the game
folder, and either screen takes one back off again: **Patches** on the
game's settings screen, or square on an applied row in the **Mods** list.

Turn on **Write a debug log** first — the launcher writes what it did to
`ux0:data/onsemu/`, and the log viewer is in the settings.

1. Put a real translation patch in `ux0:data/game_mods/` -- try a `.7z`,
   since that is what most mods ship as -- open the game it belongs to, and
   press **Mods**: it should be listed, and the prompt should say how many
   of its files are already in the game.
2. The same patch in `ux0:data/game_zips/`, selected from the game list:
   it should ask **which game**, with the right one first.
3. A patch for a *different* game, from the **Mods** button: the prompt
   should warn that it may not belong there, and applying should still be
   possible from that warning.
4. Apply it, start the game, see the patch.
5. Game settings → **Patches** → remove it; start the game, see the
   original back. The same removal from the **Mods** list: square on the
   row marked *applied* should ask the same question and, once done, drop
   the mark from the row.
6. Apply the same patch twice — the second time should say it is already
   applied rather than backing up the patch's own files over the game's.
   A mod already on the game is marked *applied* in the Mods list.
7. A patch wrapped in a folder named after itself: its *contents* should
   land in the game, not the folder.

Plugins with `overlay = yes` (#81) lay their files on through the same
record, so a failure here is worth checking against those too.

## Tested and closed

| # | Feature | Notes |
|---|---|---|
| [#65](https://github.com/mtajjiou/psv-OnscripterJH/issues/65) | Install and launch from the launcher | |
| [#32](https://github.com/mtajjiou/psv-OnscripterJH/issues/32) | Install without extracting | one bug found and fixed: the script has to be on the card, since the engine opens it before the mount exists |
| [#78](https://github.com/mtajjiou/psv-OnscripterJH/issues/78) | Send a game from a browser | |
| [#79](https://github.com/mtajjiou/psv-OnscripterJH/issues/79) | Saves to and from a server | FTP only; SMB is not implemented |
| [#81](https://github.com/mtajjiou/psv-OnscripterJH/issues/81) | Plugins | |

## What is checked without a console

`sh test/run_tests.sh` covers the portable halves: archive parsing and path
safety, the install decision chain, patch detection and matching, reading a
game out of a mounted archive, the upload page's multipart parsing (fed
whole, in packet-sized pieces, and one byte at a time), the FTP client's
reply and PASV parsing and remote-path building, plugin manifests, the heap
report, script encoding detection, video container sniffing, the format
table, the log files, and every interface string in all three languages.

`script/benchmark.sh` times the same portable half. The console's own
numbers come from the debug log.
