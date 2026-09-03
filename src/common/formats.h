/*
 *  formats.h -- what this build can actually open
 *
 *  The answer is spread across four places: the decoders script/build_ffmpeg.sh
 *  turns on, the archives src/onsjh/CMakeLists.txt links SDL_mixer and
 *  SDL_image against, what sceAvPlayer takes, and what the engine's own
 *  loaders handle.  Nobody copying a game onto a card can be expected to read
 *  all four, and the usual way to find out is a scene that plays silently or
 *  a picture that never appears.
 *
 *  So the list lives here once, as data, and both the launcher's reference
 *  screen and the README table come from it.  When a decoder is added or
 *  dropped, this table is the thing to change.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef FORMATS_H
#define FORMATS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FORMAT_CATEGORY_VIDEO = 0,
    FORMAT_CATEGORY_AUDIO,
    FORMAT_CATEGORY_IMAGE,
    FORMAT_CATEGORY_COUNT
} FormatCategory;

typedef enum {
    /* Plays as it is, with nothing to do. */
    FORMAT_PLAYS = 0,
    /* Opens, but with a cost worth knowing about before a long scene:
     * software decoding, or a stream that only half survives. */
    FORMAT_SLOW,
    /* Will not open on the console; convert it on a PC first. */
    FORMAT_CONVERT
} FormatSupport;

typedef struct {
    FormatCategory category;
    const char    *name;        /* "MPEG-1/2", "Ogg Vorbis", ... */
    const char    *extensions;  /* ".mpg .mpeg", lower case, space separated */
    FormatSupport  support;
    const char    *note;        /* short, always present */
} FormatEntry;

/* The table, in the order it should be shown: entries of one category are
 * contiguous, and within a category the ones that just work come first. */
int                formats_count(void);
const FormatEntry *formats_get(int index);

/* Entries of one category, as a range into the table.  Returns 0 and leaves
 * the outputs alone for a category that is not in it. */
int formats_category_range(FormatCategory category, int *first, int *count);

/* The entry whose extension list covers this file name, or NULL.  The name
 * may be a full path; matching is on the last extension and case-insensitive,
 * because scripts written on Windows are inconsistent about both. */
const FormatEntry *formats_lookup(const char *filename);

/* Display words: "Video"/"Audio"/"Images" and "plays"/"slow"/"convert". */
const char *formats_category_name(FormatCategory category);
const char *formats_support_word(FormatSupport support);

#ifdef __cplusplus
}
#endif

#endif
