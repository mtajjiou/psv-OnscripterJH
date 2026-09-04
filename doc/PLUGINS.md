# Plugins

A plugin is a folder on the card that changes how a game starts, or lays
files over it, without anyone writing code for it. Put one in

    ux0:data/onsemu/plugins/<your-plugin>/

and it appears under **Plugins** on the settings screen of every game it
says it is for. Turning it on is per game.

A plugin cannot run code. There is no sandbox on the console to run it in,
and a plugin that could would be a plugin that could delete a save. What it
can do is exactly what the launcher can already be asked to do — pass an
engine argument, lay files over a game — written down instead of typed in.

## The manifest

`plugin.ini`, beside the folder's name, which is what the plugin is
addressed by:

```ini
[plugin]
name        = English font
description = Uses the font this plugin brings
match       = *
args        = --font default_en.ttf --fontcache
overlay     = yes
```

| key | meaning |
|---|---|
| `name` | what the settings row says. Defaults to the folder's name |
| `description` | one line, shown beside the name |
| `match` | `*` for every game, or part of a game folder's name (case does not matter) so a plugin for one game is offered only for it |
| `args` | arguments added when a game with this plugin on starts, space separated |
| `overlay` | `yes` when the plugin brings files — see below |

A key this launcher has never heard of is ignored rather than refused, so a
plugin written for a later version still works here minus whatever it
wanted that this version does not have.

A plugin that adds no arguments and brings no files is not offered: the row
would do nothing when it was turned on.

## Files

With `overlay = yes`, everything under the plugin's `files/` folder is
copied over the game when the plugin is turned on:

```
ux0:data/onsemu/plugins/font-en/
├── plugin.ini
└── files/
    └── default_en.ttf
```

The original of anything it replaces is kept in `.mods/` inside the game,
by the same record a translation patch leaves, so turning the plugin off
puts the game back as it was. A copy that fails part way is undone rather
than left half applied.

## What a game remembers

The plugins a game has turned on are one line in `plugins.txt` in the
game's own folder, so a game copied to another card keeps them, and
deleting the game takes them with it.

## Examples

A widescreen flag for one game:

```ini
name  = Widescreen
match = higurashi
args  = --window
```

An English font pack for every game, bringing the font with it:

```ini
name    = English font
match   = *
args    = --font default_en.ttf
overlay = yes
```
