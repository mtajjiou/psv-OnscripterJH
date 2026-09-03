/* -*- C++ -*-
 *
 *  VndbCovers.h -- cover art for the game list, from vndb.org
 *
 *  A folder full of games all showing the same placeholder icon is hard to
 *  read at a glance.  The games are visual novels and vndb.org catalogues
 *  them, so their covers can be fetched over its public API and saved beside
 *  the game, where the launcher already looks for an image.
 *
 *  Fetching is never automatic: it happens when the player asks for it on a
 *  particular game.  Nothing here runs unless that button is pressed.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef VNDB_COVERS_H
#define VNDB_COVERS_H

#include <stddef.h>

enum VndbResult {
    VNDB_OK = 0,
    VNDB_NO_NETWORK,     /* no connection, or the net stack would not start */
    VNDB_NOT_FOUND,      /* the search came back with nothing */
    VNDB_HTTP_ERROR,     /* reached vndb, but it did not answer with a cover */
    VNDB_WRITE_ERROR     /* the cover arrived and could not be saved */
};

/* Looks `title` up on vndb and writes its cover into `game_dir`.
 *
 * On success saved_path receives the file that was written.  The call blocks
 * for as long as the request takes; the launcher shows a message while it
 * runs. */
VndbResult vndb_fetch_cover(const char *title, const char *game_dir,
                            char *saved_path, size_t saved_len);

/* A sentence for the player, in the interface language. */
const char *vndb_result_text(VndbResult result);

#endif
