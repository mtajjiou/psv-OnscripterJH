/* -*- C -*-
 *
 *  archive.h -- one archive, whichever kind it is
 *
 *  The installer, the patch decisions and the game-root search were
 *  written against the ZIP reader because a game arrived as a .zip.  Mods
 *  arrive as .7z at least as often, and every one of those questions --
 *  what is in here, where is the game inside it, how big is it, is this a
 *  patch -- has the same answer whichever container it is asked of.
 *
 *  So they are asked here instead.  The kind is decided by the file's own
 *  first bytes rather than its name, because an archive renamed .zip is
 *  still a 7z and "this is not a zip" is not a useful thing to tell
 *  someone.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef __ARCHIVE_H__
#define __ARCHIVE_H__

#include <stdint.h>
#include <stddef.h>

#include "zipreader.h"   /* ZIP_OK, the ZIP_ERR_* codes, zip_write_cb */

#ifdef __cplusplus
extern "C" {
#endif

enum archive_kind {
    ARCHIVE_NONE = 0,
    ARCHIVE_ZIP,
    ARCHIVE_7Z
};

typedef struct archive archive;

/* Open whatever this is.  NULL on failure, with a ZIP_ERR_* code in err. */
archive *archive_open(const char *path, int *err);
void     archive_close(archive *a);

int archive_kind_of(const archive *a);

int         archive_count(const archive *a);
const char *archive_entry_name(const archive *a, int i);
uint64_t    archive_entry_size(const archive *a, int i);
int         archive_entry_is_dir(const archive *a, int i);
uint64_t    archive_total_size(const archive *a);
int         archive_skipped_names(const archive *a);

int archive_extract_entry(archive *a, int i, zip_write_cb cb, void *user);

/* The two questions asked of an archive's shape rather than its contents,
 * both of which the zip reader already answers for a zip.  See
 * zip_common_root() and zip_find_game_root(). */
int archive_common_root(const archive *a, char *out, size_t n);
int archive_find_game_root(const archive *a, char *out, size_t n);

/* Is this a name the launcher will open at all?  ".zip" or ".7z", in any
 * case.  A pure function of the name, for scanning a folder without
 * opening everything in it. */
int archive_has_suffix(const char *name);

/* The suffix's length, so a display name can drop it: 4 for ".zip", 3 for
 * ".7z", 0 for a name that has neither. */
int archive_suffix_length(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* __ARCHIVE_H__ */
