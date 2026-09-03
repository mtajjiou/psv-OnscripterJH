/*
 *  logfile.h -- writes what the program prints to a file on the card
 *
 *  Both binaries print freely, and on a retail console none of it goes
 *  anywhere: the engine prints through sceClibPrintf, which only a debug
 *  channel sees, and stdout is not connected to anything.  So when someone
 *  says "it shows a black screen after the prologue", there is nothing to
 *  look at.
 *
 *  This is the other end of that: when the player turns logging on, the
 *  same lines are appended to a file they can read on a PC or send with a
 *  bug report.  It is deliberately small and free of console calls, so it
 *  is tested on the host.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef LOGFILE_H
#define LOGFILE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Largest a log is allowed to get before it is rotated, so a game left
 * running overnight cannot fill the card. */
#define LOG_MAX_BYTES (1024 * 1024)

/* Starts logging to path.  An existing log is kept as "<path>.1" and a new
 * one started, so the run before the one that went wrong is still there.
 * Returns 1 on success, 0 if the file cannot be written -- in which case
 * every other call here quietly does nothing. */
int  log_open(const char *path);

/* Appends text exactly as given; no newline is added, because callers pass
 * whole printf output including its own. */
void log_write(const char *text);

/* printf-style, for callers that have not formatted yet. */
void log_printf(const char *format, ...);

int  log_is_open(void);
void log_close(void);

#ifdef __cplusplus
}
#endif

#endif
