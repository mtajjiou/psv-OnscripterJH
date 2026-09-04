/* -*- C -*-
 *
 *  zipreader.h -- minimal read-only ZIP archive reader
 *
 *  Reads the central directory of a ZIP file and inflates individual
 *  entries.  Only stdio and zlib are used, so the same code runs on the
 *  Vita and on a host machine (which is where its tests run).
 *
 *  Supported: stored (method 0) and deflated (method 8) entries.
 *  Not supported: ZIP64, encryption, multi-disk archives.  Those are
 *  reported through distinct error codes so the UI can explain itself.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef __ZIPREADER_H__
#define __ZIPREADER_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZIP_OK              0
#define ZIP_ERR_IO         -1   /* cannot open / read the file */
#define ZIP_ERR_FORMAT     -2   /* not a zip, or central directory is broken */
#define ZIP_ERR_ZIP64      -3   /* zip64 archive, unsupported */
#define ZIP_ERR_METHOD     -4   /* compression method we cannot inflate */
#define ZIP_ERR_ENCRYPTED  -5   /* entry is password protected */
#define ZIP_ERR_MEMORY     -6
#define ZIP_ERR_DATA       -7   /* inflate failed / crc mismatch */
#define ZIP_ERR_CANCELED   -8   /* a callback asked us to stop */
#define ZIP_ERR_BADNAME    -9   /* entry name escapes the destination */

/* Longest entry name we keep.  Names above this are rejected as bad. */
#define ZIP_MAX_NAME 512

typedef struct zip_reader zip_reader;

/* Receives one chunk of decompressed entry data.  Return 0 to continue,
 * anything else to abort the extraction with ZIP_ERR_CANCELED. */
typedef int (*zip_write_cb)(void *user, const void *data, size_t len);

/* Open/close.  On failure zip_open() returns NULL and, if err is non-NULL,
 * stores one of the ZIP_ERR_* codes in it. */
zip_reader *zip_open(const char *path, int *err);
void        zip_close(zip_reader *z);

int         zip_count(const zip_reader *z);
const char *zip_entry_name(const zip_reader *z, int i);
uint32_t    zip_entry_size(const zip_reader *z, int i);      /* uncompressed */
uint32_t    zip_entry_compressed_size(const zip_reader *z, int i);
int         zip_entry_is_dir(const zip_reader *z, int i);
uint64_t    zip_total_size(const zip_reader *z);             /* sum of all entries */

/* How many entries were dropped because their names are longer than this
 * reader holds.  Normally 0; anything else means the archive installs
 * without those files, and the caller can decide whether to say so. */
int         zip_skipped_names(const zip_reader *z);

/* Inflate entry i, handing the plain bytes to cb in order.  Returns ZIP_OK
 * or a ZIP_ERR_* code.  Directory entries succeed without calling cb. */
int zip_extract_entry(zip_reader *z, int i, zip_write_cb cb, void *user);

/*
 * Name handling.  These are pure functions on strings so they can be
 * exercised without an archive.
 */

/* Normalise an entry name into out (size n): backslashes become '/',
 * duplicate and "." components collapse, and anything that would escape
 * the destination (absolute paths, drive letters, "..") is rejected.
 * Returns ZIP_OK or ZIP_ERR_BADNAME.  A trailing '/' is preserved. */
int zip_sanitize_name(const char *name, char *out, size_t n);

/* If every entry lives under a single top-level directory, copy that
 * directory's name (without trailing '/') into out and return 1.
 * Otherwise return 0 and leave out empty. */
int zip_common_root(const zip_reader *z, char *out, size_t n);

/* True if base is one of the script files ONScripter looks for
 * (0.txt, 00.txt, nscript.dat, onscript.nt2, ...). */
int zip_is_script_name(const char *base);

/* Directory inside the archive that holds the game's script, i.e. the
 * directory that should become the game folder in ux0:onsemu/.  Copies it
 * into out (no trailing '/'; empty string means the archive root) and
 * returns 1.  Returns 0 when the archive contains no recognisable script,
 * in which case out is empty. */
int zip_find_game_root(const zip_reader *z, char *out, size_t n);

/* Human readable form of a ZIP_ERR_* code. */
const char *zip_error_string(int err);

#ifdef __cplusplus
}
#endif

#endif /* __ZIPREADER_H__ */
