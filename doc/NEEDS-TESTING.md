# What still needs a console

Everything here is written, compiles, and has whatever host tests its
portable half allows. None of it has been run on a PSVita — the vitasdk is
not available in the environment it was written in, so a build is the only
check the console side has had.

Each item is an open issue. They stay open until someone runs them on
hardware; closing them on a green build would be closing them on the half
of the work that can be checked without one.

## The six

| # | Feature | Where it is |
|---|---|---|
| [#65](https://github.com/mtajjiou/psv-OnscripterJH/issues/65) | Install and launch a game from the launcher | select a `.zip` in the game list |
| [#32](https://github.com/mtajjiou/psv-OnscripterJH/issues/32) | Install without extracting | settings → **Install mode** → *keep compressed* |
| [#78](https://github.com/mtajjiou/psv-OnscripterJH/issues/78) | Send a game from a browser | settings → **Send a game over Wi-Fi** |
| [#79](https://github.com/mtajjiou/psv-OnscripterJH/issues/79) | Saves to and from a server | settings → **Save server (FTP)**, then send/fetch |
| [#80](https://github.com/mtajjiou/psv-OnscripterJH/issues/80) | A patch over an installed game | select a patch `.zip`; **Patches** on the game's settings |
| [#81](https://github.com/mtajjiou/psv-OnscripterJH/issues/81) | Plugins | a folder in `ux0:data/onsemu/plugins/`; **Plugins** on the game's settings |

## Before testing any of it

Turn on **Write a debug log** in the settings. Every one of these writes
what it did to `ux0:data/onsemu/`, and a report saying "it did not work" is
worth much less than the same report with the log attached. The log viewer
is in the settings too, so the log can be read on the console.

## What to check, per feature

The check lists live on the issues themselves, so a tester has them in
front of them while they file what they found. In short:

- **#65** — archive to installed game to running game, without a PC.
- **#32** — a loose-file game and an `arc.nsa` game, both installed
  compressed; both should play, and the second should save no space (the
  prompt says so before it starts).
- **#78** — the address shown works from a phone; a large archive arrives
  whole; a cancelled upload leaves nothing behind.
- **#79** — against a real FTP server; a wrong password ends in a message
  rather than a hang.
- **#80** — a real translation patch on, then off, with the game started
  in between each time.
- **#81** — one plugin that only adds an argument, one that brings files.

## What has been checked without a console

`sh test/run_tests.sh` covers the portable halves: archive parsing and path
safety, the install decision chain, patch detection and matching, reading a
game out of a mounted archive, the upload page's multipart parsing (fed
whole, in packet-sized pieces, and one byte at a time), the FTP client's
reply and PASV parsing and remote-path building, plugin manifests, the heap
report, script encoding detection, video container sniffing, the format
table, the log files, and every interface string in all three languages.

`script/benchmark.sh` times the same portable half. The console's own
numbers come from the debug log.
