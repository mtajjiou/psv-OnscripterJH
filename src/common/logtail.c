/*
 *  logtail.c -- see logtail.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "logtail.h"

#include <stdio.h>
#include <string.h>

long log_size(const char *path)
{
    FILE *fp;
    long size;

    if (path == NULL) return 0;
    fp = fopen(path, "rb");
    if (fp == NULL) return 0;

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fclose(fp);
    return size > 0 ? size : 0;
}

int log_tail(const char *path, char *buffer, size_t buffer_size,
             char **lines, int max_lines)
{
    FILE  *fp;
    long   size;
    size_t want, got;
    int    count = 0;
    int    partial_first;
    char  *p;

    if (path == NULL || buffer == NULL || lines == NULL ||
        buffer_size < 2 || max_lines < 1)
        return -1;

    fp = fopen(path, "rb");
    if (fp == NULL) return -1;

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    if (size <= 0) {
        fclose(fp);
        return 0;
    }

    /* The last buffer_size-1 bytes, so there is room for a terminator. */
    want = buffer_size - 1;
    if ((long)want > size) want = (size_t)size;
    fseek(fp, size - (long)want, SEEK_SET);
    got = fread(buffer, 1, want, fp);
    fclose(fp);
    if (got == 0) return 0;
    buffer[got] = '\0';

    /* Cut into lines in place. */
    p = buffer;
    while (*p) {
        char *line = p;
        char *end;

        while (*p && *p != '\n') p++;
        end = p;
        if (*p == '\n') p++;

        while (end > line && (end[-1] == '\r' || end[-1] == '\n')) end--;
        *end = '\0';

        /* Keep only the last max_lines: shuffle down rather than stopping,
         * because the end of the log is the part worth reading. */
        if (count == max_lines) {
            memmove(lines, lines + 1, (size_t)(max_lines - 1) * sizeof(char *));
            count--;
        }
        lines[count++] = line;
    }

    /* Reading a window into a longer file usually starts mid-line, and half
     * a line at the top reads as corruption rather than as a window.  It is
     * dropped only if something is left afterwards: one line longer than
     * the window would otherwise show as nothing at all, and the end of a
     * long line is still worth reading.  lines[0] == buffer says the
     * fragment is still the first line rather than having scrolled out. */
    partial_first = ((size_t)size > got);
    if (partial_first && count > 1 && lines[0] == buffer) {
        memmove(lines, lines + 1, (size_t)(count - 1) * sizeof(char *));
        count--;
    }

    return count;
}
