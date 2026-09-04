/* -*- C -*-
 *
 *  plugins.c -- see plugins.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include <string.h>

#include "plugins.h"

static void trim(char *s) {
    size_t len;
    char *start = s;

    while (*start == ' ' || *start == '\t') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

    len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
                       s[len - 1] == '\r' || s[len - 1] == '\n'))
        s[--len] = '\0';
}

static int same_key(const char *key, const char *want) {
    size_t i;
    for (i = 0; key[i] && want[i]; i++) {
        char a = key[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (a != want[i]) return 0;
    }
    return key[i] == '\0' && want[i] == '\0';
}

static void copy_into(char *out, size_t n, const char *value) {
    size_t len = strlen(value);
    if (len >= n) len = n - 1;
    memcpy(out, value, len);
    out[len] = '\0';
}

/* "--window --enc:sjis" becomes two arguments.  Quotes are not honoured:
 * an engine argument with a space in it does not exist, and pretending to
 * support one would only make the failure stranger. */
static void split_args(struct plugin_info *out, const char *value) {
    out->arg_count = 0;

    while (*value && out->arg_count < PLUGIN_MAX_ARGS) {
        size_t len = 0;

        while (*value == ' ' || *value == '\t') value++;
        if (*value == '\0') break;

        while (value[len] && value[len] != ' ' && value[len] != '\t') len++;
        if (len < PLUGIN_ARG_LEN) {
            memcpy(out->args[out->arg_count], value, len);
            out->args[out->arg_count][len] = '\0';
            out->arg_count++;
        }
        value += len;
    }
}

static int truthy(const char *value) {
    return value[0] == '1' || value[0] == 'y' || value[0] == 'Y' ||
           value[0] == 't' || value[0] == 'T' ||
           value[0] == 'o' || value[0] == 'O';   /* on */
}

int plugin_parse(const char *text, const char *id, struct plugin_info *out) {
    const char *line = text;

    if (out == NULL || id == NULL || id[0] == '\0') return 0;

    memset(out, 0, sizeof(*out));
    copy_into(out->id, sizeof(out->id), id);
    copy_into(out->name, sizeof(out->name), id);
    copy_into(out->match, sizeof(out->match), "*");

    if (text == NULL) return 0;

    while (*line) {
        char buffer[512];
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t)(end - line) : strlen(line);
        char *equals;

        if (len >= sizeof(buffer)) len = sizeof(buffer) - 1;
        memcpy(buffer, line, len);
        buffer[len] = '\0';
        line += end ? len + 1 : len;

        trim(buffer);
        if (buffer[0] == '\0' || buffer[0] == '#' || buffer[0] == ';' ||
            buffer[0] == '[')
            continue;

        equals = strchr(buffer, '=');
        if (equals == NULL) continue;
        *equals = '\0';
        trim(buffer);
        {
            char *value = equals + 1;
            trim(value);

            if      (same_key(buffer, "name") && value[0])
                copy_into(out->name, sizeof(out->name), value);
            else if (same_key(buffer, "description"))
                copy_into(out->description, sizeof(out->description), value);
            else if (same_key(buffer, "match") && value[0])
                copy_into(out->match, sizeof(out->match), value);
            else if (same_key(buffer, "args"))
                split_args(out, value);
            else if (same_key(buffer, "overlay"))
                out->overlay = truthy(value);
        }
    }

    /* A plugin that adds nothing and brings nothing is not a plugin; it is
     * a folder with a file in it, and offering it would be a row that does
     * nothing when it is turned on. */
    return out->arg_count > 0 || out->overlay;
}

int plugin_matches(const struct plugin_info *plugin, const char *game_folder) {
    size_t i, j, mlen, glen;

    if (plugin == NULL || game_folder == NULL) return 0;
    if (strcmp(plugin->match, "*") == 0) return 1;

    mlen = strlen(plugin->match);
    glen = strlen(game_folder);
    if (mlen == 0 || mlen > glen) return 0;

    for (i = 0; i + mlen <= glen; i++) {
        for (j = 0; j < mlen; j++) {
            char a = game_folder[i + j];
            char b = plugin->match[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
        }
        if (j == mlen) return 1;
    }
    return 0;
}

/* Walks the space-separated list, comparing whole entries: "font" must not
 * match the entry "font-en". */
int plugin_enabled(const char *list, const char *id) {
    size_t idlen;

    if (list == NULL || id == NULL || id[0] == '\0') return 0;
    idlen = strlen(id);

    while (*list) {
        size_t len = 0;
        while (*list == ' ' || *list == '\t') list++;
        while (list[len] && list[len] != ' ' && list[len] != '\t') len++;
        if (len == 0) break;

        if (len == idlen && memcmp(list, id, idlen) == 0) return 1;
        list += len;
    }
    return 0;
}

int plugin_list_set(const char *list, const char *id, int on,
                    char *out, size_t n) {
    /* Built aside and copied at the end: the caller usually passes the
     * same buffer as the list and as the answer, and writing into what is
     * still being read loses whatever has not been read yet. */
    char built[512];
    size_t o = 0;
    const char *p = list ? list : "";
    const size_t idlen = strlen(id ? id : "");

    if (out == NULL || n == 0 || idlen == 0) return 0;
    if (n > sizeof(built)) n = sizeof(built);

    /* Everything but this plugin, in the order it was already in. */
    while (*p) {
        size_t len = 0;
        while (*p == ' ' || *p == '\t') p++;
        while (p[len] && p[len] != ' ' && p[len] != '\t') len++;
        if (len == 0) break;

        if (!(len == idlen && memcmp(p, id, idlen) == 0)) {
            if (o > 0) {
                if (o + 1 >= n) return 0;
                built[o++] = ' ';
            }
            if (o + len >= n) return 0;
            memcpy(built + o, p, len);
            o += len;
        }
        p += len;
    }

    if (on) {
        if (o > 0) {
            if (o + 1 >= n) return 0;
            built[o++] = ' ';
        }
        if (o + idlen >= n) return 0;
        memcpy(built + o, id, idlen);
        o += idlen;
    }

    built[o] = '\0';
    memcpy(out, built, o + 1);
    return 1;
}
