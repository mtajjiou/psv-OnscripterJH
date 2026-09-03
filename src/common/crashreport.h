/*
 *  crashreport.h -- what the engine was doing when it stopped
 *
 *  A crash on a Vita produces a coredump nobody can read and a message that
 *  says nothing about the game.  What a bug report actually needs is
 *  smaller: which game, which label and line of its script, and which file
 *  was last opened -- the three things that turn "it crashes after the
 *  prologue" into something reproducible.
 *
 *  Two cases are covered, because they fail differently:
 *
 *    - A fault the engine catches (a parse error, a missing file it cannot
 *      do without) calls crash_write() and the report is exact.
 *    - A hard crash takes the process with it, so nothing can be written at
 *      the time.  Instead a marker is kept up to date as the game runs, and
 *      removed on a clean exit; a marker found at the next start means the
 *      run before it did not finish, and it still holds the position it had
 *      reached.
 *
 *  The marker is rewritten when the label changes rather than every line:
 *  a label is a scene, and a write per line would cost more than the
 *  information is worth.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef CRASHREPORT_H
#define CRASHREPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Starts a session.  marker_path is rewritten as the game moves, and
 * report_path is where a report is written.  Both may be NULL to turn the
 * whole thing off. */
void crash_begin(const char *marker_path, const char *report_path,
                 const char *build, const char *game);

/* Where the game is now.  Cheap: the marker is only rewritten when the
 * label actually changes. */
void crash_set_position(const char *label, int line);

/* The last file the engine asked for.  Kept in memory only -- a write per
 * file open would be a write per image. */
void crash_set_file(const char *filename);

/* Writes the report now, for a fault the engine caught.  reason is the one
 * line at the top: "parse error", "out of memory", whatever was known. */
void crash_write(const char *reason);

/* The run finished the way it was supposed to; removes the marker. */
void crash_end(void);

/* Whether a marker from a previous run is there, meaning it did not finish.
 * Reads marker_path without changing it -- call before crash_begin(), which
 * overwrites it. */
int crash_previous_was_unclean(const char *marker_path);

/* Turns the leftover marker into a report and removes it.  Returns 1 if a
 * report was written.  Called at start-up, after crash_previous_was_unclean
 * says there is something to promote. */
int crash_promote_marker(const char *marker_path, const char *report_path);

#ifdef __cplusplus
}
#endif

#endif
