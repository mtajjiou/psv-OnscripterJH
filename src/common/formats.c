/*
 *  formats.c -- the table itself
 *
 *  Every row here is claimed on the strength of something in the tree:
 *  the video rows against the decoder and demuxer lists in
 *  script/build_ffmpeg.sh, the audio and image rows against the archives
 *  linked in src/onsjh/CMakeLists.txt.  A row nobody can point at is worse
 *  than no row, because a wrong "plays" sends someone looking for a fault
 *  in their game.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "formats.h"

#include <stddef.h>
#include <string.h>
#include <ctype.h>

static const FormatEntry table[] = {
    /* Video.  MP4/H.264 is the one the hardware decoder takes; everything
     * else goes through libavcodec on the CPU, which is fine for the small
     * clips a visual novel ships and not fine for a feature-length opening
     * at 720p. */
    { FORMAT_CATEGORY_VIDEO, "H.264 in MP4",   ".mp4 .m4v .mov", FORMAT_PLAYS,
      "hardware decoded" },
    { FORMAT_CATEGORY_VIDEO, "MPEG-1/2",       ".mpg .mpeg",     FORMAT_SLOW,
      "software decoded" },
    { FORMAT_CATEGORY_VIDEO, "MPEG-4 / DivX",  ".avi",           FORMAT_SLOW,
      "software decoded" },
    { FORMAT_CATEGORY_VIDEO, "WMV / VC-1",     ".wmv .asf",      FORMAT_SLOW,
      "software decoded" },
    { FORMAT_CATEGORY_VIDEO, "VP8/VP9, Theora",".webm .mkv .ogv",FORMAT_SLOW,
      "software decoded" },
    { FORMAT_CATEGORY_VIDEO, "RealVideo",      ".rm .rmvb",      FORMAT_SLOW,
      "software decoded" },

    /* Audio.  SDL_mixer is linked against vorbis, opus, mpg123, flac and
     * three module players; MIDI has no soundfont on the console, so a
     * script that asks for one gets silence. */
    { FORMAT_CATEGORY_AUDIO, "Ogg Vorbis",     ".ogg",           FORMAT_PLAYS,
      "" },
    { FORMAT_CATEGORY_AUDIO, "MP3",            ".mp3",           FORMAT_PLAYS,
      "" },
    { FORMAT_CATEGORY_AUDIO, "WAV / PCM",      ".wav",           FORMAT_PLAYS,
      "" },
    { FORMAT_CATEGORY_AUDIO, "Opus",           ".opus",          FORMAT_PLAYS,
      "" },
    { FORMAT_CATEGORY_AUDIO, "FLAC",           ".flac",          FORMAT_PLAYS,
      "" },
    { FORMAT_CATEGORY_AUDIO, "Modules",        ".mod .xm .it .s3m", FORMAT_PLAYS,
      "" },
    { FORMAT_CATEGORY_AUDIO, "MIDI",           ".mid .midi",     FORMAT_CONVERT,
      "no soundfont on the Vita; convert to Ogg" },

    /* Images.  png, jpeg and webp are linked; bmp and gif are SDL_image's
     * own.  The engine's .nbz and cut-up pictures are handled before any of
     * these and are not files a user chooses. */
    { FORMAT_CATEGORY_IMAGE, "PNG",            ".png",           FORMAT_PLAYS,
      "" },
    { FORMAT_CATEGORY_IMAGE, "JPEG",           ".jpg .jpeg",     FORMAT_PLAYS,
      "" },
    { FORMAT_CATEGORY_IMAGE, "BMP",            ".bmp",           FORMAT_PLAYS,
      "" },
    { FORMAT_CATEGORY_IMAGE, "GIF",            ".gif",           FORMAT_PLAYS,
      "first frame only" },
    { FORMAT_CATEGORY_IMAGE, "WebP",           ".webp",          FORMAT_PLAYS,
      "" },
    { FORMAT_CATEGORY_IMAGE, "TGA, TIFF",      ".tga .tif .tiff",FORMAT_CONVERT,
      "not linked in; convert to PNG" }
};

static const int table_len = (int)(sizeof(table) / sizeof(table[0]));

int formats_count(void)
{
    return table_len;
}

const FormatEntry *formats_get(int index)
{
    if (index < 0 || index >= table_len) return NULL;
    return &table[index];
}

int formats_category_range(FormatCategory category, int *first, int *count)
{
    int i, start = -1, n = 0;

    for (i = 0; i < table_len; i++) {
        if (table[i].category != category) continue;
        if (start < 0) start = i;
        n++;
    }
    if (start < 0) return 0;

    if (first) *first = start;
    if (count) *count = n;
    return 1;
}

/* Whether ext -- a lower-cased ".xyz" -- appears as a whole word in list. */
static int extension_listed(const char *list, const char *ext)
{
    const char *p = list;
    const size_t len = strlen(ext);

    while (*p) {
        while (*p == ' ') p++;
        if (strncmp(p, ext, len) == 0 && (p[len] == '\0' || p[len] == ' '))
            return 1;
        while (*p && *p != ' ') p++;
    }
    return 0;
}

const FormatEntry *formats_lookup(const char *filename)
{
    const char *dot = NULL;
    const char *p;
    char ext[16];
    size_t i, len;

    if (filename == NULL) return NULL;

    /* The last dot after the last separator: a folder with a dot in its name
     * must not lend its extension to a file that has none. */
    for (p = filename; *p; p++) {
        if (*p == '/' || *p == '\\') dot = NULL;
        else if (*p == '.') dot = p;
    }
    if (dot == NULL) return NULL;

    len = strlen(dot);
    if (len < 2 || len >= sizeof(ext)) return NULL;
    for (i = 0; i < len; i++)
        ext[i] = (char)tolower((unsigned char)dot[i]);
    ext[len] = '\0';

    for (i = 0; i < (size_t)table_len; i++)
        if (extension_listed(table[i].extensions, ext)) return &table[i];

    return NULL;
}

const char *formats_category_name(FormatCategory category)
{
    switch (category) {
    case FORMAT_CATEGORY_VIDEO: return "Video";
    case FORMAT_CATEGORY_AUDIO: return "Audio";
    case FORMAT_CATEGORY_IMAGE: return "Images";
    default:                    return "";
    }
}

const char *formats_support_word(FormatSupport support)
{
    switch (support) {
    case FORMAT_PLAYS:   return "plays";
    case FORMAT_SLOW:    return "slow";
    case FORMAT_CONVERT: return "convert";
    default:             return "";
    }
}
