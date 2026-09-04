/* -*- C -*-
 *
 *  sevenzip.h -- minimal read-only .7z archive reader
 *
 *  The same shape as zipreader.h -- open, count, name, size, extract one
 *  entry through a callback -- over Igor Pavlov's public-domain LZMA SDK,
 *  vendored in src/common/lzma/.  Everything above it (the installer, the
 *  patch decisions, the game-root search) works on either kind of archive
 *  through archive.h and does not know which it has.
 *
 *  Why at all: mods and translation patches are distributed as .7z at
 *  least as often as .zip, and an archive the launcher cannot open is a
 *  mod the player has to unpack on a PC first -- which is the step this
 *  fork exists to remove.
 *
 *  One difference from the zip reader worth knowing: 7z compresses groups
 *  of files together, so extracting one entry can mean decoding the block
 *  it sits in.  The SDK caches that block, so extracting a whole archive
 *  in order costs one decode per block rather than one per file, but a
 *  single small file out of a large solid block still costs the block.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef __SEVENZIP_H__
#define __SEVENZIP_H__

#include <stdint.h>
#include <stddef.h>

#include "zipreader.h"   /* the ZIP_* error codes and zip_write_cb */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sevenzip_reader sevenzip_reader;

/* Open/close.  On failure returns NULL and, if err is non-NULL, stores one
 * of the ZIP_ERR_* codes in it -- the same codes the zip reader uses, so a
 * caller reports either archive the same way. */
sevenzip_reader *sevenzip_open(const char *path, int *err);
void             sevenzip_close(sevenzip_reader *z);

int         sevenzip_count(const sevenzip_reader *z);
const char *sevenzip_entry_name(const sevenzip_reader *z, int i);
uint64_t    sevenzip_entry_size(const sevenzip_reader *z, int i);
int         sevenzip_entry_is_dir(const sevenzip_reader *z, int i);
uint64_t    sevenzip_total_size(const sevenzip_reader *z);
int         sevenzip_skipped_names(const sevenzip_reader *z);

/* Decode entry i, handing the plain bytes to cb in order. */
int sevenzip_extract_entry(sevenzip_reader *z, int i,
                           zip_write_cb cb, void *user);

/* True when the file at this path begins with the 7z signature.  Read from
 * the file rather than its name: an archive renamed .zip is still a 7z,
 * and telling the player "this is not a zip" would be unhelpful. */
int sevenzip_is_sevenzip(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* __SEVENZIP_H__ */
