/* -*- C -*-
 *
 *  zipfs.c -- see zipfs.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include <stdlib.h>
#include <string.h>

#include "zipfs.h"
#include "zipreader.h"

struct zipfs_entry {
    char     name[ZIP_MAX_NAME];  /* relative to the game root, folded */
    int      index;               /* entry in the archive */
    uint32_t size;
};

struct zipfs {
    zip_reader         *zip;
    struct zipfs_entry *entries;
    int                 count;
};

/* One comparable spelling of a path: lowercase, '/' as the separator, no
 * leading "./" or '/'.  A script asking for "BG\Title.PNG" and an archive
 * holding "bg/title.png" name the same file. */
static void fold(const char *name, char *out, size_t n) {
    size_t o = 0;

    if (n == 0) return;
    if (name == NULL) { out[0] = '\0'; return; }

    while (*name == '/' || *name == '\\') name++;
    if (name[0] == '.' && (name[1] == '/' || name[1] == '\\')) name += 2;

    for (; *name && o + 1 < n; name++) {
        char c = *name;
        if (c == '\\') c = '/';
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        out[o++] = c;
    }
    out[o] = '\0';
}

zipfs *zipfs_open(const char *path) {
    int err = 0;
    char root[ZIP_MAX_NAME];
    size_t prefix_len;
    int i, count, files = 0;
    zipfs *fs;
    zip_reader *z = zip_open(path, &err);

    if (z == NULL) return NULL;

    fs = (zipfs *)calloc(1, sizeof(zipfs));
    if (fs == NULL) {
        zip_close(z);
        return NULL;
    }
    fs->zip = z;

    /* Addressed relative to the folder holding the script, which is what
     * the game folder would have been had the archive been extracted. */
    if (!zip_find_game_root(z, root, sizeof(root))) root[0] = '\0';
    prefix_len = strlen(root);

    count = zip_count(z);
    if (count > 0) {
        fs->entries = (struct zipfs_entry *)calloc((size_t)count,
                                                   sizeof(struct zipfs_entry));
        if (fs->entries == NULL) {
            zipfs_close(fs);
            return NULL;
        }
    }

    for (i = 0; i < count; i++) {
        char clean[ZIP_MAX_NAME];
        const char *relative;

        if (zip_entry_is_dir(z, i)) continue;
        if (zip_sanitize_name(zip_entry_name(z, i), clean, sizeof(clean)) != ZIP_OK)
            continue;

        relative = clean;
        if (prefix_len > 0) {
            if (strncmp(clean, root, prefix_len) != 0 ||
                clean[prefix_len] != '/')
                continue;   /* beside the game, not in it */
            relative = clean + prefix_len + 1;
        }
        if (relative[0] == '\0') continue;

        fold(relative, fs->entries[files].name, ZIP_MAX_NAME);
        fs->entries[files].index = i;
        fs->entries[files].size  = zip_entry_size(z, i);
        files++;
    }
    fs->count = files;

    return fs;
}

void zipfs_close(zipfs *fs) {
    if (fs == NULL) return;
    if (fs->zip) zip_close(fs->zip);
    free(fs->entries);
    free(fs);
}

int zipfs_count(const zipfs *fs) {
    return fs ? fs->count : 0;
}

static struct zipfs_entry *find(zipfs *fs, const char *name) {
    char wanted[ZIP_MAX_NAME];
    int i;

    if (fs == NULL || name == NULL) return NULL;
    fold(name, wanted, sizeof(wanted));

    for (i = 0; i < fs->count; i++)
        if (strcmp(fs->entries[i].name, wanted) == 0) return &fs->entries[i];
    return NULL;
}

long zipfs_size(zipfs *fs, const char *name) {
    struct zipfs_entry *e = find(fs, name);
    return e ? (long)e->size : -1;
}

/* Writes what is inflated straight into the caller's buffer, stopping if
 * the archive turns out to hold more than its directory promised. */
struct sink {
    unsigned char *buffer;
    size_t         written;
    size_t         limit;
};

static int write_sink(void *user, const void *data, size_t len) {
    struct sink *s = (struct sink *)user;

    if (s->written + len > s->limit) return -1;
    memcpy(s->buffer + s->written, data, len);
    s->written += len;
    return 0;
}

size_t zipfs_read(zipfs *fs, const char *name, unsigned char *buffer) {
    struct zipfs_entry *e = find(fs, name);
    struct sink s;

    if (e == NULL || buffer == NULL) return 0;

    s.buffer  = buffer;
    s.written = 0;
    s.limit   = e->size;

    if (zip_extract_entry(fs->zip, e->index, write_sink, &s) != ZIP_OK)
        return 0;
    return s.written;
}

int zipfs_script_name(zipfs *fs, char *out, size_t n) {
    int i;

    if (out == NULL || n == 0) return 0;
    out[0] = '\0';
    if (fs == NULL) return 0;

    for (i = 0; i < fs->count; i++) {
        /* At the game root: a script one folder down belongs to something
         * else -- a backup, or another game bundled beside this one. */
        if (strchr(fs->entries[i].name, '/') != NULL) continue;
        if (!zip_is_script_name(fs->entries[i].name)) continue;
        if (strlen(fs->entries[i].name) >= n) return 0;

        strcpy(out, fs->entries[i].name);
        return 1;
    }
    return 0;
}

int zipfs_needs_disk(const char *name) {
    /* Everything the engine opens as a file and seeks around in, plus the
     * formats that are already compressed and would gain nothing. */
    static const char *extensions[] = {
        ".nsa", ".sar", ".ns2", ".nsaq",
        ".mp4", ".m4v", ".mov", ".avi", ".mpg", ".mpeg", ".mpv",
        ".wmv", ".mkv", ".webm", ".ogv", ".rmvb", ".flv",
        ".ttf", ".otf", ".ttc"
    };
    const char *base;
    size_t len, i;

    if (name == NULL) return 0;

    /* The script is opened by name with fopen() before the mount exists,
     * so it has to be a file on the card whatever else is not. */
    base = name;
    for (i = 0; name[i]; i++)
        if (name[i] == '/' || name[i] == '\\') base = name + i + 1;
    if (zip_is_script_name(base)) return 1;

    len = strlen(name);

    for (i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
        const size_t el = strlen(extensions[i]);
        size_t k;
        if (len < el) continue;
        for (k = 0; k < el; k++) {
            char c = name[len - el + k];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            if (c != extensions[i][k]) break;
        }
        if (k == el) return 1;
    }
    return 0;
}
