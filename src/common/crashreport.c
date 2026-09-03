/*
 *  crashreport.c -- see crashreport.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "crashreport.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define CRASH_FIELD 256

static char crash_marker[CRASH_FIELD];
static char crash_report[CRASH_FIELD];
static char crash_build[64];
static char crash_game[CRASH_FIELD];
static char crash_label[96];
static char crash_file[CRASH_FIELD];
static int  crash_line;
static int  crash_on;

static void copy_field(char *dst, size_t size, const char *src)
{
    if (src == NULL) { dst[0] = '\0'; return; }
    snprintf(dst, size, "%s", src);
}

/* The body of both the marker and the report: the same facts, so a marker
 * found at the next start can be turned into a report without knowing
 * anything more than it already holds. */
static void write_body(FILE *fp, const char *reason)
{
    fprintf(fp, "reason: %s\n", (reason && reason[0]) ? reason : "unknown");
    fprintf(fp, "build: %s\n", crash_build);
    fprintf(fp, "game: %s\n", crash_game);
    fprintf(fp, "label: %s\n", crash_label[0] ? crash_label : "(none yet)");
    fprintf(fp, "line: %d\n", crash_line);
    fprintf(fp, "last file: %s\n", crash_file[0] ? crash_file : "(none yet)");
}

static void write_marker(void)
{
    FILE *fp;

    if (!crash_on || crash_marker[0] == '\0') return;

    fp = fopen(crash_marker, "w");
    if (fp == NULL) return;
    write_body(fp, "the game was still running");
    fclose(fp);
}

void crash_begin(const char *marker_path, const char *report_path,
                 const char *build, const char *game)
{
    copy_field(crash_marker, sizeof(crash_marker), marker_path);
    copy_field(crash_report, sizeof(crash_report), report_path);
    copy_field(crash_build, sizeof(crash_build), build);
    copy_field(crash_game, sizeof(crash_game), game);
    crash_label[0] = '\0';
    crash_file[0] = '\0';
    crash_line = 0;
    crash_on = (crash_marker[0] != '\0' || crash_report[0] != '\0');

    write_marker();
}

void crash_set_position(const char *label, int line)
{
    const int same_label = (label != NULL) &&
                           (strncmp(crash_label, label, sizeof(crash_label) - 1) == 0);

    crash_line = line;
    if (same_label) return;      /* a line number is not worth a write */

    copy_field(crash_label, sizeof(crash_label), label);
    write_marker();
}

void crash_set_file(const char *filename)
{
    copy_field(crash_file, sizeof(crash_file), filename);
}

void crash_write(const char *reason)
{
    FILE *fp;

    if (!crash_on || crash_report[0] == '\0') return;

    fp = fopen(crash_report, "w");
    if (fp == NULL) return;
    write_body(fp, reason);
    fclose(fp);

    /* The report is the better record, so the marker has done its job. */
    if (crash_marker[0]) remove(crash_marker);
}

void crash_end(void)
{
    if (crash_marker[0]) remove(crash_marker);
    crash_on = 0;
}

int crash_previous_was_unclean(const char *marker_path)
{
    FILE *fp;

    if (marker_path == NULL || marker_path[0] == '\0') return 0;
    fp = fopen(marker_path, "rb");
    if (fp == NULL) return 0;
    fclose(fp);
    return 1;
}

int crash_promote_marker(const char *marker_path, const char *report_path)
{
    FILE  *in, *out;
    char   line[512];
    size_t got;

    if (marker_path == NULL || report_path == NULL) return 0;

    in = fopen(marker_path, "rb");
    if (in == NULL) return 0;

    out = fopen(report_path, "w");
    if (out == NULL) { fclose(in); return 0; }

    /* Its own first line says the game was still running, which is exactly
     * what happened: it never got to say otherwise. */
    fprintf(out, "reason: the previous run did not exit cleanly\n");

    while (fgets(line, sizeof(line), in) != NULL) {
        if (strncmp(line, "reason:", 7) == 0) continue;   /* replaced above */
        got = strlen(line);
        fwrite(line, 1, got, out);
    }

    fclose(in);
    fclose(out);
    remove(marker_path);
    return 1;
}
