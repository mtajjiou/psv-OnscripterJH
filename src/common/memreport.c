/* -*- C -*-
 *
 *  memreport.c -- see memreport.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include <stdio.h>
#include <string.h>

#include "memreport.h"
#include "logfile.h"

/* newlib's own accounting, which is the heap both binaries actually
 * allocate from.  Asking the kernel instead would report the block the
 * heap was carved out of, which never changes and says nothing. */
#if defined(__vita__) || defined(__linux__) || defined(__GLIBC__) || \
    defined(__NEWLIB__)
#include <malloc.h>
#define MEM_HAVE_MALLINFO 1
#endif

static size_t g_peak = 0;

static void sample(size_t *used, size_t *freed) {
    *used  = 0;
    *freed = 0;

#ifdef MEM_HAVE_MALLINFO
    {
        /* mallinfo2 where it exists (glibc 2.33+), mallinfo elsewhere:
         * the fields are the same, the widths are not. */
#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 33)
        struct mallinfo2 info = mallinfo2();
#else
        struct mallinfo info = mallinfo();
#endif
#else
        struct mallinfo info = mallinfo();
#endif
        *used  = (size_t)info.uordblks;
        *freed = (size_t)info.fordblks;
    }
#endif

    if (*used > g_peak) g_peak = *used;
}

size_t mem_used(void) {
    size_t used, freed;
    sample(&used, &freed);
    return used;
}

size_t mem_peak(void) {
    size_t used, freed;
    sample(&used, &freed);
    return g_peak;
}

int mem_format(char *out, size_t n, const char *when,
               size_t used, size_t freed, size_t peak) {
    if (out == NULL || n == 0) return 0;

    return snprintf(out, n,
                    "mem %s: %lu KB in use, %lu KB free in the heap, "
                    "%lu KB at the highest\n",
                    when ? when : "",
                    (unsigned long)(used / 1024),
                    (unsigned long)(freed / 1024),
                    (unsigned long)(peak / 1024));
}

void mem_report(const char *when) {
    char line[160];
    size_t used, freed;

    sample(&used, &freed);
    mem_format(line, sizeof(line), when, used, freed, g_peak);

    /* log_printf() is a no-op when the debug log is off, which is what
     * makes this safe to call from anywhere. */
    log_printf("%s", line);
}
