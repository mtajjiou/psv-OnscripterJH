/* -*- C++ -*-
 *
 *  PluginManager.cpp -- see PluginManager.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PluginManager.h"
#include "ZipHandler.h"
#include "filesystem.h"

extern "C" {
#include "patchplan.h"
}

namespace {

/* Where a game keeps the list of plugins it has turned on. */
const char *ENABLED_FILE = "plugins.txt";

std::string enabledPath(const std::string &game_folder) {
    return game_folder + "/" + ENABLED_FILE;
}

std::string readEnabled(const std::string &game_folder) {
    SceUID fd = sceIoOpen(enabledPath(game_folder).c_str(), SCE_O_RDONLY, 0777);
    if (fd < 0) return std::string();

    char buffer[512];
    int got = sceIoRead(fd, buffer, sizeof(buffer) - 1);
    sceIoClose(fd);
    if (got <= 0) return std::string();

    buffer[got] = '\0';
    /* One line, whatever else the file has in it. */
    char *newline = strchr(buffer, '\n');
    if (newline) *newline = '\0';
    return std::string(buffer);
}

void writeEnabled(const std::string &game_folder, const std::string &list) {
    if (list.empty()) {
        sceIoRemove(enabledPath(game_folder).c_str());
        return;
    }
    SceUID fd = sceIoOpen(enabledPath(game_folder).c_str(),
                          SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) return;
    sceIoWrite(fd, list.c_str(), list.size());
    sceIoWrite(fd, "\n", 1);
    sceIoClose(fd);
}

std::string pluginFolder(const std::string &id) {
    return std::string(PLUGIN_FOLDER) + "/" + id;
}

/* The record a plugin's files leave in the game, in the same format and
 * the same place as a patch's, so one piece of code takes either off. */
std::string recordName(const std::string &id) {
    return "plugin-" + id + PATCH_RECORD_SUFFIX;
}

/* Copies one plugin's files/ tree over the game, recording what it
 * replaced so it can be put back.  Returns false having undone what it
 * did, rather than leaving a game half covered. */
bool applyFiles(const std::string &game_folder, const std::string &id);

bool copyTree(const std::string &from, const std::string &to,
              const std::string &backups, const std::string &record,
              const std::string &prefix) {
    SceUID dfd = sceIoDopen(from.c_str());
    if (dfd < 0) return false;

    bool ok = true;
    int res = 0;
    do {
        SceIoDirent entry;
        memset(&entry, 0, sizeof(entry));
        res = sceIoDread(dfd, &entry);
        if (res <= 0) break;
        if (entry.d_name[0] == '.') continue;

        const std::string relative = prefix.empty()
            ? std::string(entry.d_name) : prefix + "/" + entry.d_name;
        const std::string source = from + "/" + entry.d_name;
        const std::string target = to + "/" + entry.d_name;

        if (SCE_S_ISDIR(entry.d_stat.st_mode)) {
            sceIoMkdir(target.c_str(), 0777);
            if (!copyTree(source, target, backups, record, relative))
                ok = false;
            continue;
        }

        const bool replacing = checkFileExist(target.c_str()) != 0;
        if (replacing) {
            const std::string saved = backups + "/" + relative;
            /* The folders the backup needs, made as they are needed. */
            size_t slash = saved.find_last_of('/');
            if (slash != std::string::npos) {
                std::string parent = saved.substr(0, slash);
                size_t at = backups.size();
                while (at < parent.size()) {
                    size_t next = parent.find('/', at + 1);
                    sceIoMkdir(parent.substr(0, next == std::string::npos
                                                ? parent.size() : next).c_str(),
                               0777);
                    if (next == std::string::npos) break;
                    at = next;
                }
            }
            if (copyFile(target.c_str(), saved.c_str()) < 0) { ok = false; break; }
        }

        /* Recorded before the copy: a file half written is still one that
         * has to be put back. */
        SceUID fd = sceIoOpen(record.c_str(),
                              SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
        if (fd >= 0) {
            std::string line;
            line += replacing ? PATCH_LINE_REPLACED : PATCH_LINE_NEW;
            line += ' ';
            line += relative;
            line += '\n';
            sceIoWrite(fd, line.c_str(), line.size());
            sceIoClose(fd);
        }

        if (copyFile(source.c_str(), target.c_str()) < 0) { ok = false; break; }
    } while (res > 0);

    sceIoDclose(dfd);
    return ok;
}

bool applyFiles(const std::string &game_folder, const std::string &id) {
    const std::string files = pluginFolder(id) + "/" + PLUGIN_FILES;
    if (!checkFolderExist(files.c_str())) return true;   /* nothing to lay on */

    const std::string mods    = game_folder + "/" + PATCH_FOLDER;
    const std::string record  = mods + "/" + recordName(id);
    const std::string backups = mods + "/plugin-" + id;

    sceIoMkdir(mods.c_str(), 0777);
    sceIoMkdir(backups.c_str(), 0777);

    if (!copyTree(files, game_folder, backups, record, "")) {
        ZipHandler::removePatch(game_folder, recordName(id));
        return false;
    }
    return true;
}

} /* namespace */

void PluginManager::ensureFolder() {
    /* Its parent exists whenever the launcher has ever written a config. */
    sceIoMkdir("ux0:data/onsemu", 0777);
    sceIoMkdir(PLUGIN_FOLDER, 0777);
}

std::vector<plugin_info> PluginManager::available(const std::string &game_folder) {
    std::vector<plugin_info> plugins;

    ensureFolder();

    SceUID dfd = sceIoDopen(PLUGIN_FOLDER);
    if (dfd < 0) return plugins;

    /* The game's folder name is what a plugin says it is for. */
    std::string name = game_folder;
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);

    int res = 0;
    do {
        SceIoDirent entry;
        memset(&entry, 0, sizeof(entry));
        res = sceIoDread(dfd, &entry);
        if (res <= 0) break;
        if (!SCE_S_ISDIR(entry.d_stat.st_mode)) continue;
        if (entry.d_name[0] == '.') continue;

        const std::string manifest =
            pluginFolder(entry.d_name) + "/" + PLUGIN_MANIFEST;
        SceUID fd = sceIoOpen(manifest.c_str(), SCE_O_RDONLY, 0777);
        if (fd < 0) continue;

        char text[4096];
        int got = sceIoRead(fd, text, sizeof(text) - 1);
        sceIoClose(fd);
        if (got <= 0) continue;
        text[got] = '\0';

        plugin_info plugin;
        if (!plugin_parse(text, entry.d_name, &plugin)) continue;
        if (!plugin_matches(&plugin, name.c_str())) continue;

        plugins.push_back(plugin);
    } while (res > 0);

    sceIoDclose(dfd);
    return plugins;
}

bool PluginManager::enabled(const std::string &game_folder,
                            const std::string &id) {
    return plugin_enabled(readEnabled(game_folder).c_str(), id.c_str()) != 0;
}

bool PluginManager::setEnabled(const std::string &game_folder,
                               const plugin_info &plugin, bool on) {
    const std::string id = plugin.id;

    if (on && plugin.overlay && !applyFiles(game_folder, id)) return false;
    if (!on && plugin.overlay)
        ZipHandler::removePatch(game_folder, recordName(id));

    char list[512];
    if (!plugin_list_set(readEnabled(game_folder).c_str(), id.c_str(),
                         on ? 1 : 0, list, sizeof(list)))
        return false;

    writeEnabled(game_folder, list);
    return true;
}

int PluginManager::appendArgs(const std::string &game_folder,
                              char **argv, int count, int max) {
    /* The engine reads this array after the call that starts the game
     * returns, so the strings have to outlive this function -- copied
     * rather than pointed at, since everything else here is a local. */

    const std::string list = readEnabled(game_folder);
    if (list.empty()) return count;

    const std::vector<plugin_info> plugins = available(game_folder);
    for (size_t i = 0; i < plugins.size(); i++) {
        if (!plugin_enabled(list.c_str(), plugins[i].id)) continue;

        for (int a = 0; a < plugins[i].arg_count && count < max; a++) {
            /* Copied by hand: this toolchain's <string.h> does not declare
             * strdup, and the copy has to outlive this call either way. */
            const size_t len = strlen(plugins[i].args[a]);
            char *argument = (char *)malloc(len + 1);
            if (argument == NULL) break;
            memcpy(argument, plugins[i].args[a], len + 1);
            argv[count++] = argument;
        }
    }
    return count;
}
