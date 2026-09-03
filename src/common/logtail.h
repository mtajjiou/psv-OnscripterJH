/*
 *  logtail.h -- the last lines of a log file
 *
 *  The launcher can show a log on the console, which is the only way to
 *  read one without taking the memory card out.  What matters in a log is
 *  its end -- the run that just went wrong -- and a log can be a megabyte,
 *  so this reads backwards from the end into a buffer the caller owns and
 *  hands back pointers to the last lines in it.
 *
 *  Nothing here is Vita-specific and nothing allocates, so it is tested on
 *  the host against short files, long lines and files that are not there.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef LOGTAIL_H
#define LOGTAIL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reads the end of path into buffer and fills lines with pointers into it,
 * oldest first, at most max_lines of them.
 *
 * The buffer is written to and cut into strings in place, so it must stay
 * alive as long as the lines are used.  A line longer than what is left of
 * the buffer is truncated rather than dropped, and the first line is
 * dropped when the read started in the middle of it -- half a line at the
 * top reads as corruption when it is only a window.
 *
 * Returns the number of lines, 0 for an empty file and -1 when the file
 * cannot be read at all.  Trailing newlines and carriage returns are
 * removed; the lines themselves are otherwise untouched.
 */
int log_tail(const char *path, char *buffer, size_t buffer_size,
             char **lines, int max_lines);

/* Size of path in bytes, or 0 -- for showing how big a log has become
 * beside the part of it that is on screen. */
long log_size(const char *path);

#ifdef __cplusplus
}
#endif

#endif
