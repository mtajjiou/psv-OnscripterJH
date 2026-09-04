/* -*- C -*-
 *
 *  ftpproto.c -- see ftpproto.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ftpproto.h"

static int is_digit(char c) { return c >= '0' && c <= '9'; }

int ftp_reply_code(const char *line) {
    if (line == NULL) return -1;
    if (!is_digit(line[0]) || !is_digit(line[1]) || !is_digit(line[2]))
        return -1;
    /* A code is followed by a space or a hyphen; three digits at the start
     * of a file listing are not a reply. */
    if (line[3] != ' ' && line[3] != '-' && line[3] != '\0' &&
        line[3] != '\r' && line[3] != '\n')
        return -1;

    return (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
}

int ftp_reply_is_final(const char *line) {
    if (ftp_reply_code(line) < 0) return 0;
    return line[3] != '-';
}

int ftp_reply_ok(int code) {
    return code >= 100 && code < 400;
}

int ftp_parse_pasv(const char *line, char *host, size_t n, int *port) {
    const char *p;
    int numbers[6];
    int count = 0;

    if (line == NULL || host == NULL || port == NULL || n == 0) return 0;
    host[0] = '\0';

    p = strchr(line, '(');
    if (p == NULL) {
        /* Some servers leave the brackets off and just list the numbers. */
        p = line + 3;
    }
    else {
        p++;
    }

    while (count < 6) {
        while (*p && !is_digit(*p)) {
            if (*p == ')' || *p == '\r' || *p == '\n') break;
            p++;
        }
        if (!is_digit(*p)) break;

        numbers[count++] = (int)strtol(p, (char **)&p, 10);
    }

    if (count != 6) return 0;
    {
        int i;
        for (i = 0; i < 6; i++)
            if (numbers[i] < 0 || numbers[i] > 255) return 0;
    }

    if ((size_t)snprintf(host, n, "%d.%d.%d.%d", numbers[0], numbers[1],
                         numbers[2], numbers[3]) >= n) {
        host[0] = '\0';
        return 0;
    }
    *port = numbers[4] * 256 + numbers[5];
    return 1;
}

int ftp_parse_epsv(const char *line, int *port) {
    const char *p;

    if (line == NULL || port == NULL) return 0;

    p = strchr(line, '(');
    if (p == NULL) return 0;
    p++;

    /* (|||50000|) -- three separators, whatever they are, then the port. */
    {
        const char separator = *p;
        int seen = 0;
        if (separator == '\0') return 0;
        while (*p == separator) { p++; seen++; }
        if (seen != 3) return 0;
    }

    if (!is_digit(*p)) return 0;
    *port = (int)strtol(p, NULL, 10);
    return *port > 0 && *port < 65536;
}

int ftp_parse_size(const char *line, long *size) {
    const char *p;

    if (line == NULL || size == NULL) return 0;
    if (ftp_reply_code(line) != 213) return 0;

    p = line + 3;
    while (*p == ' ') p++;
    if (!is_digit(*p)) return 0;

    *size = strtol(p, NULL, 10);
    return 1;
}

/* One path component appended, with no slash doubled and nothing that
 * would climb out of the base allowed through. */
static int append_component(char *out, size_t n, const char *component) {
    size_t len = strlen(out);

    if (component == NULL || component[0] == '\0') return 1;
    if (strcmp(component, ".") == 0) return 1;
    if (strcmp(component, "..") == 0) return 0;
    if (strchr(component, '/') != NULL || strchr(component, '\\') != NULL)
        return 0;
    /* A reply is a line; a name carrying a newline could forge one. */
    if (strchr(component, '\r') || strchr(component, '\n')) return 0;

    if (len == 0 || out[len - 1] != '/') {
        if (len + 1 >= n) return 0;
        out[len++] = '/';
        out[len] = '\0';
    }
    if (len + strlen(component) >= n) return 0;
    strcpy(out + len, component);
    return 1;
}

int ftp_join_path(const char *base, const char *game, const char *file,
                  char *out, size_t n) {
    size_t len;

    if (out == NULL || n < 2) return 0;

    if (base == NULL || base[0] == '\0') base = "/";
    if (base[0] != '/') return 0;          /* always an absolute path */
    if (strstr(base, "..") != NULL) return 0;
    if (strlen(base) >= n) return 0;
    strcpy(out, base);

    /* A trailing slash on the base would otherwise double up. */
    len = strlen(out);
    while (len > 1 && out[len - 1] == '/') out[--len] = '\0';

    if (!append_component(out, n, game)) return 0;
    if (!append_component(out, n, file)) return 0;
    return 1;
}

int ftp_is_save_name(const char *name) {
    const char *p;

    if (name == NULL) return 0;

    if ((name[0] == 's' || name[0] == 'S') &&
        (name[1] == 'a' || name[1] == 'A') &&
        (name[2] == 'v' || name[2] == 'V') &&
        (name[3] == 'e' || name[3] == 'E')) {
        p = name + 4;
        if (!is_digit(*p)) return 0;
        while (is_digit(*p)) p++;
        return (p[0] == '.' &&
                (p[1] == 'd' || p[1] == 'D') &&
                (p[2] == 'a' || p[2] == 'A') &&
                (p[3] == 't' || p[3] == 'T') && p[4] == '\0');
    }

    {
        static const char *others[] = { "gloval.sav", "envdata", "kidoku.dat" };
        size_t i, k;
        for (i = 0; i < sizeof(others) / sizeof(others[0]); i++) {
            const size_t len = strlen(others[i]);
            if (strlen(name) != len) continue;
            for (k = 0; k < len; k++) {
                char c = name[k];
                if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
                if (c != others[i][k]) break;
            }
            if (k == len) return 1;
        }
    }
    return 0;
}

int ftp_parse_list_line(const char *line, char *name, size_t n) {
    if (line == NULL || name == NULL || n == 0) return 0;
    name[0] = '\0';

    /* Unix: the mode comes first, and a directory says so in its first
     * character.  The name is what follows the eighth field. */
    if (line[0] == '-' || line[0] == 'd' || line[0] == 'l') {
        int fields = 0;
        const char *p = line;

        if (line[0] != '-') return 0;   /* a save is a plain file */

        while (*p && fields < 8) {
            while (*p == ' ') p++;
            if (*p == '\0') return 0;
            while (*p && *p != ' ') p++;
            fields++;
        }
        while (*p == ' ') p++;
        if (*p == '\0' || *p == '\r' || *p == '\n') return 0;

        {
            size_t o = 0;
            while (*p && *p != '\r' && *p != '\n' && o + 1 < n)
                name[o++] = *p++;
            name[o] = '\0';
        }
        return name[0] != '\0';
    }

    /* DOS: "01-01-24  12:00AM      4096 name", with <DIR> where the size
     * would be for a directory. */
    if (is_digit(line[0])) {
        const char *p = line;
        int fields = 0;

        if (strstr(line, "<DIR>") != NULL) return 0;

        while (*p && fields < 3) {
            while (*p == ' ') p++;
            if (*p == '\0') return 0;
            while (*p && *p != ' ') p++;
            fields++;
        }
        while (*p == ' ') p++;
        if (*p == '\0' || *p == '\r' || *p == '\n') return 0;

        {
            size_t o = 0;
            while (*p && *p != '\r' && *p != '\n' && o + 1 < n)
                name[o++] = *p++;
            name[o] = '\0';
        }
        return name[0] != '\0';
    }

    return 0;
}
