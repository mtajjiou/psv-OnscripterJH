/* -*- C++ -*-
 *
 *  PluginManager.h -- the console side of plugins
 *
 *  Finds the plugins on the card, remembers which of them each game has
 *  turned on, lays a plugin's files over a game when it is turned on and
 *  takes them off again when it is turned off, and hands the engine the
 *  arguments the turned-on plugins add when the game starts.
 *
 *  See src/common/plugins.h for what a plugin is and why it cannot run
 *  code.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef __PLUGINMANAGER_H__
#define __PLUGINMANAGER_H__

#include <string>
#include <vector>

extern "C" {
#include "plugins.h"
}

class PluginManager {
public:
    /* The plugins on the card that are offered for this game, in the order
     * their folders are in.  An empty list is the ordinary case: nobody
     * has put anything in the plugins folder. */
    static std::vector<plugin_info> available(const std::string &game_folder);

    /* Which of them this game has turned on.  Kept in the game's own
     * folder, so a game copied to another card keeps its plugins. */
    static bool enabled(const std::string &game_folder, const std::string &id);

    /* Turn one on or off.  A plugin that brings files copies them over the
     * game here, keeping what it replaced, and puts them back when it is
     * turned off -- the same record a patch leaves, so the two cannot get
     * out of step.  False when the files could not be copied, in which
     * case the plugin stays off rather than half applied. */
    static bool setEnabled(const std::string &game_folder,
                           const plugin_info &plugin, bool on);

    /* Append the arguments of every plugin this game has turned on to an
     * argument array being built for the engine.  Returns the new count.
     * The strings live for the rest of the process, since the array is
     * read after the call that starts the game returns. */
    static int appendArgs(const std::string &game_folder,
                          char **argv, int count, int max);

    /* The folder, created if missing, so the first person to look for
     * somewhere to put a plugin finds it. */
    static void ensureFolder();
};

#endif /* __PLUGINMANAGER_H__ */
