/* -*- C -*-
 *
 *  patchplan.c -- see patchplan.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include <string.h>
#include <stdlib.h>

#include "patchplan.h"

int patch_archive_kind(const zip_reader *z) {
    char root[ZIP_MAX_NAME];
    int i, files = 0;

    for (i = 0; i < zip_count(z); i++)
        if (!zip_entry_is_dir(z, i)) files++;

    if (files == 0) return PATCH_KIND_EMPTY;
    if (zip_find_game_root(z, root, sizeof(root))) return PATCH_KIND_GAME;
    return PATCH_KIND_PATCH;
}

int patch_overlay_root(const zip_reader *z, char *out, size_t n) {
    int i, files = 0;

    if (n == 0) return 0;
    out[0] = '\0';

    for (i = 0; i < zip_count(z); i++)
        if (!zip_entry_is_dir(z, i)) files++;
    if (files == 0) return 0;

    /* zip_common_root() leaves out empty when the entries do not share
     * one, which is exactly the answer wanted here. */
    zip_common_root(z, out, n);
    return 1;
}

/* Words a patch's file name carries that say nothing about which game it
 * belongs to.  Removed from both sides before comparing, so the version
 * number and the language do not push a match down. */
static const char *NOISE[] = {
    "patch", "patched", "english", "eng", "translation", "translated",
    "trans", "mod", "modpack", "fanpatch", "fantranslation", "voice",
    "voicepatch", "full", "final", "install", "installer", "update",
    "japanese", "jp", "jpn", "chinese", "cn", "zip", "release", "ver",
    "version", "for", "the"
};

/* Lowercase letters and digits only, with the noise words above dropped.
 * A run of digits directly after "v" goes too: "v2", "v1.03". */
static void normalise(const char *in, char *out, size_t n) {
    char words[256];
    size_t w = 0, o = 0;
    size_t i;
    size_t len = in ? strlen(in) : 0;

    /* First pass: fold to lowercase alphanumerics, with a single space
     * wherever anything else was, so words can be recognised. */
    for (i = 0; i <= len && w + 1 < sizeof(words); i++) {
        char c = in ? in[i] : '\0';
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            words[w++] = c;
        else if (w > 0 && words[w - 1] != ' ')
            words[w++] = ' ';
    }
    while (w > 0 && words[w - 1] == ' ') w--;
    words[w] = '\0';

    /* Second pass: keep the words that carry meaning, concatenated. */
    {
        char *save = words;
        while (*save) {
            char *space = strchr(save, ' ');
            size_t wl = space ? (size_t)(space - save) : strlen(save);
            int noise = 0;
            size_t k;

            for (k = 0; k < sizeof(NOISE) / sizeof(NOISE[0]); k++)
                if (wl == strlen(NOISE[k]) && strncmp(save, NOISE[k], wl) == 0)
                    noise = 1;

            /* "v2", "v1", "v103": a version, not a name. */
            if (!noise && wl >= 2 && save[0] == 'v') {
                size_t d;
                noise = 1;
                for (d = 1; d < wl; d++)
                    if (save[d] < '0' || save[d] > '9') { noise = 0; break; }
            }
            /* A bare number on its own is a version too. */
            if (!noise && wl > 0) {
                size_t d;
                noise = 1;
                for (d = 0; d < wl; d++)
                    if (save[d] < '0' || save[d] > '9') { noise = 0; break; }
            }

            if (!noise)
                for (k = 0; k < wl && o + 1 < n; k++) out[o++] = save[k];

            if (!space) break;
            save = space + 1;
        }
    }
    if (n > 0) out[o < n ? o : n - 1] = '\0';
}

/* Length of the longest run of characters the two strings share. */
static size_t common_run(const char *a, const char *b) {
    size_t best = 0, i, j;
    size_t la = strlen(a), lb = strlen(b);

    for (i = 0; i < la; i++) {
        for (j = 0; j < lb; j++) {
            size_t k = 0;
            while (i + k < la && j + k < lb && a[i + k] == b[j + k]) k++;
            if (k > best) best = k;
        }
    }
    return best;
}

int patch_name_match(const char *patch_name, const char *game_name) {
    char p[128], g[128];
    size_t lp, lg, run, shorter;

    normalise(patch_name, p, sizeof(p));
    normalise(game_name, g, sizeof(g));

    lp = strlen(p);
    lg = strlen(g);
    if (lp == 0 || lg == 0) return 0;

    if (strcmp(p, g) == 0) return 100;

    run = common_run(p, g);
    shorter = lp < lg ? lp : lg;
    /* Two names that happen to share three letters are not a match; a
     * shared run has to be most of the shorter name to mean anything. */
    if (run < 4 || run * 2 < shorter) return 0;

    /* The whole of one name inside the other is the ordinary case: a patch
     * called after the game plus words that were just dropped. */
    if (run == shorter) {
        size_t longer = lp > lg ? lp : lg;
        int score = (int)(70 + (30 * shorter) / longer);
        return score > 99 ? 99 : score;
    }
    return (int)((60 * run) / shorter);
}

int patch_parse_line(const char *line, char *kind, char *path, size_t n) {
    size_t len;

    if (line == NULL || n == 0) return 0;
    if (line[0] != PATCH_LINE_REPLACED && line[0] != PATCH_LINE_NEW) return 0;
    if (line[1] != ' ') return 0;

    len = strlen(line + 2);
    while (len > 0 && (line[2 + len - 1] == '\n' || line[2 + len - 1] == '\r'))
        len--;
    if (len == 0 || len >= n) return 0;

    memcpy(path, line + 2, len);
    path[len] = '\0';
    *kind = line[0];
    return 1;
}

int patch_record_name(const char *zip_path, char *out, size_t n) {
    const char *base = zip_path, *p;
    size_t o = 0, len;

    if (out == NULL || n == 0) return 0;
    out[0] = '\0';
    if (zip_path == NULL) return 0;

    for (p = zip_path; *p; p++)
        if (*p == '/' || *p == '\\') base = p + 1;

    len = strlen(base);
    if (len > 4 && (base[len - 4] == '.') &&
        (base[len - 3] == 'z' || base[len - 3] == 'Z') &&
        (base[len - 2] == 'i' || base[len - 2] == 'I') &&
        (base[len - 1] == 'p' || base[len - 1] == 'P'))
        len -= 4;

    {
        size_t i;
        for (i = 0; i < len && o + sizeof(PATCH_RECORD_SUFFIX) + 1 < n; i++) {
            char c = base[i];
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                c == '-' || c == '_')
                out[o++] = c;
            else if (c >= 'A' && c <= 'Z')
                out[o++] = (char)(c - 'A' + 'a');
            else if (o > 0 && out[o - 1] != '_')
                out[o++] = '_';
        }
    }
    while (o > 0 && out[o - 1] == '_') o--;
    if (o == 0) {
        const char *fallback = "patch";
        if (n < strlen(fallback) + sizeof(PATCH_RECORD_SUFFIX) + 1) return 0;
        memcpy(out, fallback, strlen(fallback));
        o = strlen(fallback);
    }
    if (o + sizeof(PATCH_RECORD_SUFFIX) > n) return 0;
    memcpy(out + o, PATCH_RECORD_SUFFIX, sizeof(PATCH_RECORD_SUFFIX));
    return 1;
}
