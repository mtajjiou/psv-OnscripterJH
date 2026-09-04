/* -*- C -*-
 *
 *  pathmatch.c -- see pathmatch.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pathmatch.h"

#define PATH_MATCH_MAX 1024

/* How many folders are remembered.  The game root and the two or three a
 * game keeps art in are what a lookup asks about; a fifth pushes out the
 * oldest, which costs one directory read. */
#define CACHE_SLOTS 4
/* A folder with more names than this is read live each time: this exists
 * to save time, not to hold a copy of somebody's collection. */
#define CACHE_MAX_NAMES 4096

struct slot {
    char  *dir;
    char **names;
    int    count;
};

static struct slot g_cache[CACHE_SLOTS];
static int         g_next;

static char *copy_of(const char *text) {
    char *copy = (char *)malloc(strlen(text) + 1);
    if (copy) strcpy(copy, text);
    return copy;
}

static void clear_slot(struct slot *s) {
    int i;

    if (s->names) {
        for (i = 0; i < s->count; i++) free(s->names[i]);
        free(s->names);
    }
    free(s->dir);
    s->dir   = NULL;
    s->names = NULL;
    s->count = 0;
}

void path_match_forget(void) {
    int i;
    for (i = 0; i < CACHE_SLOTS; i++) clear_slot(&g_cache[i]);
}

static int same_ci(const char *a, const char *b) {
    /* Spelled out rather than using strcasecmp, which the vitasdk
     * toolchain does not declare through <string.h>. */
    while (*a && *b) {
        char ca = *a++, cb = *b++;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
    }
    return *a == '\0' && *b == '\0';
}

static struct slot *load(const char *dir) {
    DIR *dp = opendir(dir);
    struct dirent *entry;
    struct slot *s;
    char **names;
    int count = 0;

    if (dp == NULL) return NULL;

    names = (char **)malloc(sizeof(char *) * CACHE_MAX_NAMES);
    if (names == NULL) {
        closedir(dp);
        return NULL;
    }

    while ((entry = readdir(dp)) != NULL) {
        if (count == CACHE_MAX_NAMES) break;
        names[count] = copy_of(entry->d_name);
        if (names[count] == NULL) break;
        count++;
    }
    closedir(dp);

    s = &g_cache[g_next];
    g_next = (g_next + 1) % CACHE_SLOTS;

    clear_slot(s);
    s->dir   = copy_of(dir);
    s->names = names;
    s->count = count;
    return s;
}

/* The real name of `want` inside dir, or NULL.  The answer is always the
 * same length as what was asked for, since case is all that differs. */
static const char *match(const char *dir, const char *want) {
    struct slot *s = NULL;
    int i;

    for (i = 0; i < CACHE_SLOTS; i++)
        if (g_cache[i].dir && strcmp(g_cache[i].dir, dir) == 0) s = &g_cache[i];

    if (s == NULL) s = load(dir);
    if (s == NULL) return NULL;

    for (i = 0; i < s->count; i++)
        if (same_ci(want, s->names[i])) return s->names[i];
    return NULL;
}

static int writing(const char *mode) {
    const char *m;
    for (m = mode; m && *m; m++)
        if (*m == 'w' || *m == 'a' || *m == '+') return 1;
    return 0;
}

FILE *path_open_ci(const char *base, const char *path, const char *mode) {
    char full[PATH_MATCH_MAX];
    char component[PATH_MATCH_MAX];
    size_t base_len;
    char *at;
    FILE *fp;

    if (path == NULL || mode == NULL) return NULL;
    if (base == NULL) base = "";
    if (strlen(base) + strlen(path) + 1 > sizeof(full)) return NULL;

    strcpy(full, base);
    strcat(full, path);

    /* Anything written through here changes what a folder holds, so what
     * was remembered about one is no longer true. */
    if (writing(mode)) path_match_forget();

    fp = fopen(full, mode);
    if (fp) return fp;

    /* Not there under that spelling.  Walk the path a component at a time,
     * matching each against what its folder actually holds. */
    base_len = strlen(base);
    at = full + base_len;

    while (1) {
        char *slash;
        const char *real;
        size_t len, dir_len;
        char saved;

        while (*at == '/') at++;

        slash = strchr(at, '/');
        len = slash ? (size_t)(slash - at) : strlen(at);
        if (len == 0 || len >= sizeof(component)) return NULL;

        memcpy(component, at, len);
        component[len] = '\0';

        /* The folder this component lives in: everything before it, which
         * is this path with a temporary end put on it. */
        dir_len = (size_t)(at - full);
        saved = full[dir_len];
        full[dir_len] = '\0';
        real = match(dir_len > 0 ? full : ".", component);
        full[dir_len] = saved;

        if (real == NULL) return NULL;
        memcpy(at, real, len);

        if (slash == NULL) break;
        at = slash + 1;
    }

    return fopen(full, mode);
}
