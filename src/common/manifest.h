/*
 *  manifest.h -- what the launcher already knows about the games on a card
 *
 *  Building the game list means walking directories: finding which folder
 *  actually holds the script, reading each game's caption, measuring folders
 *  when the list is ordered by size.  None of that changes between one start
 *  and the next unless the folder itself changed, so it is written down and
 *  read back.
 *
 *  The format is JSON because the file is meant to be readable by whoever
 *  opens it on a PC, and small enough that a parser for the shape written
 *  here is a page of code rather than a dependency.  Anything it cannot
 *  parse is treated as no cache at all: a wrong answer here would be a game
 *  that will not start, and rebuilding costs a second.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef MANIFEST_H
#define MANIFEST_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MANIFEST_PATH_MAX 512
#define MANIFEST_NAME_MAX 256
#define MANIFEST_STAMP_MAX 32

typedef struct manifest_entry {
    char     folder[MANIFEST_PATH_MAX];  /* the folder as the card lists it */
    char     root[MANIFEST_PATH_MAX];    /* where the script actually is    */
    char     name[MANIFEST_NAME_MAX];    /* caption, or the folder's name   */
    char     stamp[MANIFEST_STAMP_MAX];  /* what says the answer is stale   */
    uint64_t size;                       /* 0 when it has not been measured */
} manifest_entry;

typedef struct manifest {
    manifest_entry *entries;
    int             count;
    int             capacity;
} manifest;

void manifest_init(manifest *m);
void manifest_free(manifest *m);

/* Reads a manifest.  A missing, empty or unparseable file leaves an empty
 * manifest and returns 0 -- the caller rebuilds, which is always correct. */
int manifest_load(manifest *m, const char *path);

/* Writes it. Returns 1 on success. */
int manifest_save(const manifest *m, const char *path);

/* The entry for a folder, or NULL.  Only matches when the stamp agrees, so
 * a folder that changed since it was written is a miss rather than a lie. */
const manifest_entry *manifest_find(const manifest *m, const char *folder,
                                    const char *stamp);

/* Adds or replaces the entry for a folder. */
int manifest_put(manifest *m, const manifest_entry *entry);

#ifdef __cplusplus
}
#endif

#endif
