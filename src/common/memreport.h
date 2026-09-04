/* -*- C -*-
 *
 *  memreport.h -- what the heap is doing, in the log
 *
 *  The launcher asks for a 192MB heap and the engine spends it on images:
 *  a game that dies part way through is usually a game that asked for one
 *  bitmap more than there was room for.  From the outside that looks like
 *  "this game does not work", and there is nothing in the log to say
 *  otherwise.
 *
 *  This writes a line whenever something big has just happened -- the list
 *  was built, an install finished, a game's script was read -- saying what
 *  is in use, what the high-water mark is, and how much of it is holes
 *  rather than data.  Two lines from the same session are what turns "it
 *  crashes on chapter three" into "it was 40MB from the ceiling before
 *  chapter three started".
 *
 *  Only written when the debug log is on, so an ordinary session pays a
 *  mallinfo() call and nothing else.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef __MEMREPORT_H__
#define __MEMREPORT_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Write one line about the heap, tagged with what has just happened
 * ("list built", "install done", "script read").  Cheap enough to leave
 * in: it asks the allocator for its own totals and writes one line. */
void mem_report(const char *when);

/* Bytes in use now, and the most that have ever been in use in this
 * process.  Exposed so a screen can show them without going through the
 * log. */
size_t mem_used(void);
size_t mem_peak(void);

/* The line mem_report() writes, built from figures rather than measured,
 * so the formatting can be checked without an allocator that has anything
 * interesting in it.  Sizes are bytes; free is what has been returned to
 * the allocator but not to the system, which is the difference between a
 * heap that is full and one that is fragmented. */
int mem_format(char *out, size_t n, const char *when,
               size_t used, size_t freed, size_t peak);

#ifdef __cplusplus
}
#endif

#endif /* __MEMREPORT_H__ */
