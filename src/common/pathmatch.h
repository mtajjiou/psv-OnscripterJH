/* -*- C -*-
 *
 *  pathmatch.h -- opening a file whose name is spelled differently
 *
 *  A script written on Windows asks for "BG\Title.PNG"; the card holds
 *  "bg/title.png".  The engine's answer has always been to walk the path a
 *  component at a time and match each against what the folder actually
 *  holds, ignoring case.
 *
 *  The cost of that answer is a directory read per attempt, and the attempt
 *  happens for nearly every file a game opens: art and audio live inside
 *  arc.nsa, so the loose-file lookup misses first, every time, and twice
 *  per file, because the engine asks for a file's length before its
 *  contents.  The save menu alone asks for twenty files that are not there.
 *  On a memory card that is what "laggy after every click" is made of.
 *
 *  So a folder's listing is read once and kept.  What is cached is what a
 *  folder holds -- not whether a file exists -- and anything written
 *  through here drops the lot, so a listing is only stale for as long as
 *  nothing has changed.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef __PATHMATCH_H__
#define __PATHMATCH_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Open base+path, and if that name is not there, again with each component
 * matched against its folder without case.  Returns NULL when there is no
 * such file under any spelling.
 *
 * base is a prefix, not a folder: it is pasted straight onto path, so it
 * ends with a separator when it names one.  path is separated by '/'.
 * A mode that can create or change a file clears what was remembered. */
FILE *path_open_ci(const char *base, const char *path, const char *mode);

/* Forget every listing.  Called by path_open_ci() itself for a write; a
 * caller needs it only when something else has changed a folder. */
void path_match_forget(void);

#ifdef __cplusplus
}
#endif

#endif /* __PATHMATCH_H__ */
