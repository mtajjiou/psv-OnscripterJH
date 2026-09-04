/* -*- C -*-
 *
 *  archive.c -- see archive.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include <stdlib.h>
#include <string.h>

#include "archive.h"
#include "sevenzip.h"

struct archive {
    int kind;
    zip_reader      *zip;
    sevenzip_reader *sevenzip;
};

static int ends_with_ci(const char *name, const char *suffix) {
    size_t n, s, i;

    if (name == NULL) return 0;
    n = strlen(name);
    s = strlen(suffix);
    /* A name that is nothing but the suffix counts: the installer already
     * has an answer for it -- the folder falls back to a plain name -- and
     * changing that here would change what such a file installs as. */
    if (n < s) return 0;

    for (i = 0; i < s; i++) {
        char c = name[n - s + i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c != suffix[i]) return 0;
    }
    return 1;
}

int archive_has_suffix(const char *name) {
    return ends_with_ci(name, ".zip") || ends_with_ci(name, ".7z");
}

int archive_suffix_length(const char *name) {
    if (ends_with_ci(name, ".zip")) return 4;
    if (ends_with_ci(name, ".7z"))  return 3;
    return 0;
}

archive *archive_open(const char *path, int *err) {
    archive *a = (archive *)calloc(1, sizeof(archive));

    if (a == NULL) {
        if (err) *err = ZIP_ERR_MEMORY;
        return NULL;
    }

    /* The file's own first bytes decide, not its name: an archive renamed
     * .zip is still a 7z, and a mod is renamed more often than most
     * things. */
    if (sevenzip_is_sevenzip(path)) {
        a->sevenzip = sevenzip_open(path, err);
        if (a->sevenzip == NULL) { free(a); return NULL; }
        a->kind = ARCHIVE_7Z;
        return a;
    }

    a->zip = zip_open(path, err);
    if (a->zip == NULL) { free(a); return NULL; }
    a->kind = ARCHIVE_ZIP;
    return a;
}

void archive_close(archive *a) {
    if (a == NULL) return;
    if (a->zip)      zip_close(a->zip);
    if (a->sevenzip) sevenzip_close(a->sevenzip);
    free(a);
}

int archive_kind_of(const archive *a) {
    return a ? a->kind : ARCHIVE_NONE;
}

int archive_count(const archive *a) {
    if (a == NULL) return 0;
    return a->zip ? zip_count(a->zip) : sevenzip_count(a->sevenzip);
}

const char *archive_entry_name(const archive *a, int i) {
    if (a == NULL) return "";
    return a->zip ? zip_entry_name(a->zip, i)
                  : sevenzip_entry_name(a->sevenzip, i);
}

uint64_t archive_entry_size(const archive *a, int i) {
    if (a == NULL) return 0;
    return a->zip ? (uint64_t)zip_entry_size(a->zip, i)
                  : sevenzip_entry_size(a->sevenzip, i);
}

int archive_entry_is_dir(const archive *a, int i) {
    if (a == NULL) return 0;
    if (a->zip) return zip_entry_is_dir(a->zip, i);

    /* A 7z records a folder as a flag rather than as a trailing slash,
     * which is what everything above expects to see. */
    return sevenzip_entry_is_dir(a->sevenzip, i);
}

uint64_t archive_total_size(const archive *a) {
    if (a == NULL) return 0;
    return a->zip ? zip_total_size(a->zip) : sevenzip_total_size(a->sevenzip);
}

int archive_skipped_names(const archive *a) {
    if (a == NULL) return 0;
    return a->zip ? zip_skipped_names(a->zip)
                  : sevenzip_skipped_names(a->sevenzip);
}

int archive_extract_entry(archive *a, int i, zip_write_cb cb, void *user) {
    if (a == NULL) return ZIP_ERR_FORMAT;
    return a->zip ? zip_extract_entry(a->zip, i, cb, user)
                  : sevenzip_extract_entry(a->sevenzip, i, cb, user);
}

/*
 * The two shape questions.  For a zip they are the reader's own; for a 7z
 * they are the same rules over this reader's names, written once here
 * rather than duplicated into the 7z side.
 */

int archive_common_root(const archive *a, char *out, size_t n) {
    int i;
    size_t root_len = 0;

    if (out == NULL || n == 0) return 0;
    out[0] = '\0';
    if (a == NULL) return 0;
    if (a->zip) return zip_common_root(a->zip, out, n);

    for (i = 0; i < archive_count(a); i++) {
        const char *name = archive_entry_name(a, i);
        const char *slash;

        if (name[0] == '\0') continue;

        slash = strchr(name, '/');
        if (slash == NULL) {
            /* A file at the archive root: there is no single folder. */
            if (!archive_entry_is_dir(a, i)) { out[0] = '\0'; return 0; }
            continue;
        }

        if (out[0] == '\0') {
            root_len = (size_t)(slash - name);
            if (root_len == 0 || root_len + 1 > n) { out[0] = '\0'; return 0; }
            memcpy(out, name, root_len);
            out[root_len] = '\0';
        }
        else if (strncmp(name, out, root_len) != 0 || name[root_len] != '/') {
            out[0] = '\0';
            return 0;
        }
    }

    return out[0] != '\0';
}

int archive_find_game_root(const archive *a, char *out, size_t n) {
    int i, best = -1;
    size_t best_depth = 0;

    if (out == NULL || n == 0) return 0;
    out[0] = '\0';
    if (a == NULL) return 0;
    if (a->zip) return zip_find_game_root(a->zip, out, n);

    /* The shallowest script wins, so a backup folder holding an old copy
     * of the script is not mistaken for the game -- the same rule the zip
     * reader follows. */
    for (i = 0; i < archive_count(a); i++) {
        const char *name = archive_entry_name(a, i);
        const char *base;
        const char *p;
        size_t depth = 0;

        if (archive_entry_is_dir(a, i) || name[0] == '\0') continue;

        base = name;
        for (p = name; *p; p++)
            if (*p == '/') { base = p + 1; depth++; }

        if (!zip_is_script_name(base)) continue;
        if (best < 0 || depth < best_depth) {
            best = i;
            best_depth = depth;
        }
    }

    if (best < 0) return 0;

    {
        const char *name = archive_entry_name(a, best);
        const char *slash = strrchr(name, '/');
        size_t len = slash ? (size_t)(slash - name) : 0;

        if (len + 1 > n) return 0;
        memcpy(out, name, len);
        out[len] = '\0';
    }
    return 1;
}
