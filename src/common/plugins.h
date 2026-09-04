/* -*- C -*-
 *
 *  plugins.h -- small add-ons a player can drop onto the card
 *
 *  Everything this launcher does to a game it does because someone wrote
 *  code for it: an engine flag, a set of files laid over the game, a
 *  fixed-up font.  A plugin is that same thing without the code -- a
 *  folder under ux0:data/onsemu/plugins/ with a plugin.ini in it, saying
 *  what it is called, which games it is for, what arguments it adds when
 *  one of those games starts, and whether it brings files to lay over the
 *  game.  Turning one on for a game is a row on that game's settings
 *  screen.
 *
 *  It is deliberately not a scripting engine.  A plugin cannot run code on
 *  the console -- there is no sandbox here to run it in -- so what it can
 *  do is exactly what the launcher can already be asked to do, written
 *  down instead of typed in.  That covers the things people actually pass
 *  around: a widescreen flag for one game, an English font with the
 *  arguments that make the engine use it, a mod's files with a name and a
 *  description attached.
 *
 *  The manifest reading and the matching are here, as pure functions of
 *  text, since a plugin that silently applies to every game -- or to none
 *  -- is the failure worth catching on a host rather than on a console.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef __PLUGINS_H__
#define __PLUGINS_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Where plugins live, one folder each. */
#define PLUGIN_FOLDER   "ux0:data/onsemu/plugins"
/* What makes a folder in there a plugin. */
#define PLUGIN_MANIFEST "plugin.ini"
/* Files a plugin lays over a game live under this folder inside it, and
 * are applied by the same code that applies a patch. */
#define PLUGIN_FILES    "files"

#define PLUGIN_MAX_ARGS 8
#define PLUGIN_ARG_LEN  64

struct plugin_info {
    char id[64];            /* the folder's name: what is written down */
    char name[64];          /* what the settings row says */
    char description[160];
    char match[64];         /* "*", or part of a game folder's name */
    char args[PLUGIN_MAX_ARGS][PLUGIN_ARG_LEN];
    int  arg_count;
    int  overlay;           /* brings files to lay over the game */
};

/* Read a plugin.ini.  id is the folder's name, which is what the plugin is
 * addressed by; a manifest with no name is named after its folder.
 * Returns 1 when the text describes a plugin, 0 when it does not.
 *
 * Unknown keys are ignored rather than refused: a plugin written for a
 * later version of the launcher should still work in this one, minus
 * whatever it wanted that this one has never heard of. */
int plugin_parse(const char *text, const char *id, struct plugin_info *out);

/* Is this plugin offered for that game?  "*" is every game; anything else
 * matches when it appears in the game's folder name, ignoring case, so a
 * plugin for one game is written down as part of that game's name. */
int plugin_matches(const struct plugin_info *plugin, const char *game_folder);

/* Which plugins a game has turned on, held as one line of space-separated
 * ids -- in the game's own settings file, so a game moved to another card
 * keeps them. */
int plugin_enabled(const char *list, const char *id);

/* That line with one plugin turned on or off.  Returns 1 on success. */
int plugin_list_set(const char *list, const char *id, int on,
                    char *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* __PLUGINS_H__ */
