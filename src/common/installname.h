/*
 *  installname.h -- what an archive will be installed as
 *
 *  Three small decisions with one thing in common: they are made before
 *  anything is written, they are visible to the player afterwards -- the
 *  folder's name is what the list shows and what the engine opens -- and
 *  they are pure functions of a string, so they can be checked on a
 *  normal machine rather than only by installing something on a console.
 *
 *  They lived inside the Vita-only installer until an integration test
 *  wanted to ask what an archive would install as without installing it.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef INSTALLNAME_H
#define INSTALLNAME_H

#include <string>

/* Case-insensitive test for a suffix the launcher can open: ".zip" or
 * ".7z".  Spelled out rather than using strcasecmp, which the vitasdk
 * toolchain does not declare via <string.h>. */
bool install_has_zip_suffix(const char *name);

/* The last component of a path, with either kind of separator. */
std::string install_base_name(const std::string &path);

/* A folder name the console and the engine can both open.
 *
 * The engine opens game paths as plain byte strings and chokes on names
 * carrying non-ASCII bytes, so anything unusual folds down to '_'.  The
 * result is never empty, never ends in a separator or a dot, and is short
 * enough to be part of a path. */
std::string install_safe_folder_name(const std::string &raw);

/* The folder an archive installs into, from its file name: the name the
 * player typed and sees in the list, rather than the generic "Game" or
 * "data" a folder inside the archive is so often called. */
std::string install_destination_name(const std::string &zip_path);

#endif
