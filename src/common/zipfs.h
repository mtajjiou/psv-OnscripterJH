/* -*- C -*-
 *
 *  zipfs.h -- reading a game's files from its .zip, without extracting it
 *
 *  A game installed the ordinary way exists twice on the card: once as the
 *  archive it came in and once as the folder it was extracted to.  Deleting
 *  the archive is the usual answer, and on an 8GB card it is not much of
 *  one -- the folder is the bigger half.
 *
 *  This is the other answer: keep the archive, extract only the files the
 *  engine opens as files, and read the rest out of the archive as the game
 *  asks for them.  A visual novel's loose art and audio deflate well, so
 *  the saving is real; its .nsa archives do not deflate at all, which is
 *  why they go to disk rather than being inflated on every read.
 *
 *  The lookup is case-insensitive and takes either separator, because a
 *  script written on Windows asks for "BG\Title.PNG" and the archive holds
 *  "bg/title.png".
 *
 *  Portable C over zipreader.c, so it is tested on a host.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef __ZIPFS_H__
#define __ZIPFS_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The archive a compressed install keeps beside the files it extracted. */
#define ZIPFS_ARCHIVE_NAME "game.zip"

typedef struct zipfs zipfs;

/* Mount the archive at path.  Entries are addressed relative to the game
 * root inside it, so a game wrapped in a folder is read the same way as one
 * that is not.  Returns NULL if the archive cannot be read. */
zipfs *zipfs_open(const char *path);
void   zipfs_close(zipfs *fs);

/* Size of a file in the archive, or -1 when it holds no such file.
 * Read from the central directory: nothing is inflated to answer this. */
long zipfs_size(zipfs *fs, const char *name);

/* Read a whole file into buffer, which must hold zipfs_size() bytes.
 * Returns the number of bytes written, or 0 on any failure. */
size_t zipfs_read(zipfs *fs, const char *name, unsigned char *buffer);

/* How many files the mount can serve.  0 means an archive with nothing in
 * it, which is worth telling apart from a mount that failed. */
int zipfs_count(const zipfs *fs);

/* True for a file that has to exist on the card rather than in the archive:
 * the game's own archives (.nsa, .sar, .ns2), its videos and its fonts, all
 * of which the engine opens as files and seeks around in and none of which
 * deflate to anything worth having -- and the script, which the engine
 * opens by name with fopen() before any of this exists.  A compressed
 * install extracts these and leaves the rest inside the .zip.
 *
 * A pure function of the name so the launcher, the engine and the tests all
 * decide it the same way. */
int zipfs_needs_disk(const char *name);

/* The script inside the mount, if it holds one: the name it is addressed
 * by, which is also the name it has to have on the card.  Returns 1 when
 * there is one.
 *
 * Only useful for repairing a game installed compressed by a build that
 * left the script inside the archive, which the engine cannot open: it
 * finds its script with fopen() before there is a reader to ask. */
int zipfs_script_name(zipfs *fs, char *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* __ZIPFS_H__ */
