/*
 *  logfile.c -- see logfile.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "logfile.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

static FILE *log_fp = NULL;
static char  log_path[256];
static long  log_written = 0;

/* Moves the current log aside as "<path>.1", replacing an older one.  Keeps
 * exactly one generation: the interesting run is either the last one or the
 * one before it, and more than that is a way to fill a memory card. */
static void rotate(const char *path)
{
    char previous[sizeof(log_path) + 4];

    if (snprintf(previous, sizeof(previous), "%s.1", path) >= (int)sizeof(previous))
        return;

    remove(previous);
    rename(path, previous);
}

int log_open(const char *path)
{
    if (path == NULL || path[0] == '\0') return 0;
    if (strlen(path) >= sizeof(log_path)) return 0;

    log_close();

    rotate(path);

    log_fp = fopen(path, "w");
    if (log_fp == NULL) return 0;

    snprintf(log_path, sizeof(log_path), "%s", path);
    log_written = 0;
    return 1;
}

void log_write(const char *text)
{
    size_t len;

    if (log_fp == NULL || text == NULL) return;

    len = strlen(text);
    if (len == 0) return;

    fwrite(text, 1, len, log_fp);
    log_written += (long)len;

    /* Flushed every line: a log that is still in a buffer when the console
     * freezes is a log of everything except what went wrong. */
    fflush(log_fp);

    if (log_written >= LOG_MAX_BYTES) {
        /* Start again rather than grow without limit.  The path is kept, so
         * this reopens the same file the player was told about. */
        char path[sizeof(log_path)];
        snprintf(path, sizeof(path), "%s", log_path);
        log_open(path);
    }
}

void log_printf(const char *format, ...)
{
    char line[512];
    va_list va;

    if (log_fp == NULL || format == NULL) return;

    va_start(va, format);
    vsnprintf(line, sizeof(line), format, va);
    va_end(va);

    log_write(line);
}

int log_is_open(void)
{
    return log_fp != NULL;
}

void log_close(void)
{
    if (log_fp == NULL) return;
    fclose(log_fp);
    log_fp = NULL;
    log_written = 0;
}
