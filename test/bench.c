/*
 *  bench.c -- how fast the portable half of the launcher is
 *
 *  Run it with script/benchmark.sh.  What this measures is the code that
 *  is the same on a PC and on the console: inflating an archive, reading
 *  and writing the scan cache, taking the tail of a log.  What it cannot
 *  measure is the memory card, which is what an install on a Vita is
 *  actually waiting for -- so a number here is an upper bound and a
 *  regression detector, not a prediction of how long a game takes to
 *  install.
 *
 *  It exists to answer one question honestly: when something gets slower,
 *  was it the code or the card?  A run before and after a change answers
 *  that in a way that timing an install on the console cannot.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "zipreader.h"
#include "manifest.h"
#include "logtail.h"

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static void report(const char *what, double ms, double units, const char *unit)
{
    printf("  %-34s %8.1f ms", what, ms);
    if (units > 0 && ms > 0)
        printf("   %8.1f %s/s", units * 1000.0 / ms, unit);
    printf("\n");
}

/* Counts bytes without keeping them: the write is the card's job, and on a
 * host it would measure the page cache rather than anything useful. */
static uint64_t sunk;
static int sink(void *user, const void *data, size_t len)
{
    (void)user; (void)data;
    sunk += len;
    return 0;
}

static void bench_extract(const char *dir, const char *file)
{
    char path[1024];
    zip_reader *z;
    int err = 0, i;
    double start;

    snprintf(path, sizeof(path), "%s/%s", dir, file);

    start = now_ms();
    z = zip_open(path, &err);
    if (!z) {
        printf("  %-34s skipped (%s)\n", file, zip_error_string(err));
        return;
    }
    report("open and read the directory", now_ms() - start, 0, NULL);

    sunk = 0;
    start = now_ms();
    for (i = 0; i < zip_count(z); i++)
        zip_extract_entry(z, i, sink, NULL);
    {
        const double ms = now_ms() - start;
        report("inflate every entry", ms,
               (double)sunk / (1024.0 * 1024.0), "MB");
    }

    zip_close(z);
}

static void bench_manifest(const char *dir, int entries)
{
    char path[1024];
    manifest m;
    double start;
    int i;

    snprintf(path, sizeof(path), "%s/bench_manifest.json", dir);

    manifest_init(&m);
    start = now_ms();
    for (i = 0; i < entries; i++) {
        manifest_entry e;
        memset(&e, 0, sizeof(e));
        snprintf(e.folder, sizeof(e.folder), "ux0:/onsemu/game%04d", i);
        snprintf(e.root, sizeof(e.root), "ux0:/onsemu/game%04d/data", i);
        snprintf(e.name, sizeof(e.name), "A Game Called Number %d", i);
        snprintf(e.stamp, sizeof(e.stamp), "%d", 1234567 + i);
        e.size = (uint64_t)i * 1000000;
        manifest_put(&m, &e);
    }
    report("build the cache in memory", now_ms() - start, entries, "entries");

    start = now_ms();
    manifest_save(&m, path);
    report("write it", now_ms() - start, entries, "entries");
    manifest_free(&m);

    manifest_init(&m);
    start = now_ms();
    manifest_load(&m, path);
    report("read it back", now_ms() - start, entries, "entries");

    start = now_ms();
    for (i = 0; i < entries; i++) {
        char folder[64], stamp[32];
        snprintf(folder, sizeof(folder), "ux0:/onsemu/game%04d", i);
        snprintf(stamp, sizeof(stamp), "%d", 1234567 + i);
        manifest_find(&m, folder, stamp);
    }
    report("look every game up in it", now_ms() - start, entries, "lookups");

    manifest_free(&m);
    remove(path);
}

static void bench_logtail(const char *dir)
{
    char path[1024];
    char buffer[8192];
    char *lines[256];
    FILE *fp;
    double start;
    int i;

    snprintf(path, sizeof(path), "%s/bench.log", dir);
    fp = fopen(path, "w");
    if (!fp) return;
    for (i = 0; i < 20000; i++)
        fprintf(fp, "line %d: something the engine had to say about a file\n", i);
    fclose(fp);

    start = now_ms();
    for (i = 0; i < 100; i++)
        log_tail(path, buffer, sizeof(buffer), lines, 256);
    report("read the end of a 1MB log (x100)", now_ms() - start, 100, "reads");

    remove(path);
}

int main(int argc, char **argv)
{
    const char *dir = (argc > 1) ? argv[1] : ".";

    printf("archive\n");
    bench_extract(dir, "sizes.zip");
    bench_extract(dir, "nested.zip");

    printf("scan cache, 200 games\n");
    bench_manifest(dir, 200);

    printf("log viewer\n");
    bench_logtail(dir);

    printf("\nThese are host numbers.  On the console the same work is behind\n"
           "a memory card: an install is bound by writes, not by inflate.\n"
           "Compare a run against another run, not against a Vita.\n");
    return 0;
}
