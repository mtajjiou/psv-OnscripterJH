/*
 *  videofmt.c -- see videofmt.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <string.h>

#include "videofmt.h"

static int matches(const unsigned char *p, size_t len, size_t off,
                   const char *sig, size_t n)
{
    if (off + n > len) return 0;
    return memcmp(p + off, sig, n) == 0;
}

VideoFormat video_format_sniff(const void *buf, size_t len)
{
    const unsigned char *p = (const unsigned char *)buf;

    if (p == NULL || len < 4) return VIDEO_FMT_UNKNOWN;

    /* ISO base media: a box whose type is "ftyp" at offset 4.  Covers mp4,
     * m4v and mov, all of which sceAvPlayer will at least open. */
    if (matches(p, len, 4, "ftyp", 4)) return VIDEO_FMT_MP4;

    /* RIFF....AVI  */
    if (matches(p, len, 0, "RIFF", 4) && matches(p, len, 8, "AVI ", 4))
        return VIDEO_FMT_AVI;

    /* Matroska and WebM share the EBML header. */
    if (len >= 4 && p[0] == 0x1A && p[1] == 0x45 && p[2] == 0xDF && p[3] == 0xA3)
        return VIDEO_FMT_MATROSKA;

    /* ASF/WMV: a 16-byte GUID; its first four bytes are enough here. */
    if (len >= 4 && p[0] == 0x30 && p[1] == 0x26 && p[2] == 0xB2 && p[3] == 0x75)
        return VIDEO_FMT_ASF;

    if (matches(p, len, 0, "OggS", 4)) return VIDEO_FMT_OGG;
    if (matches(p, len, 0, "FLV", 3))  return VIDEO_FMT_FLV;
    if (matches(p, len, 0, ".RMF", 4)) return VIDEO_FMT_REALMEDIA;

    /* MPEG start codes: 00 00 01 followed by BA (program stream pack),
     * B3 (video sequence header) or E0..EF (a PES video packet).  This is
     * the format an old PC visual novel most often ships. */
    if (len >= 4 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01) {
        if (p[3] == 0xBA || p[3] == 0xB3 || (p[3] >= 0xE0 && p[3] <= 0xEF))
            return VIDEO_FMT_MPEG;
    }

    /* MPEG-2 transport stream: 188-byte packets, each starting with 0x47. */
    if (len >= 189 && p[0] == 0x47 && p[188] == 0x47) return VIDEO_FMT_MPEG;

    return VIDEO_FMT_UNKNOWN;
}

VideoFormat video_format_from_name(const char *name)
{
    const char *dot;
    char ext[8];
    size_t i, n;

    if (name == NULL) return VIDEO_FMT_UNKNOWN;

    dot = strrchr(name, '.');
    if (dot == NULL) return VIDEO_FMT_UNKNOWN;
    dot++;

    n = strlen(dot);
    if (n == 0 || n >= sizeof(ext)) return VIDEO_FMT_UNKNOWN;
    for (i = 0; i < n; i++) {
        char c = dot[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        ext[i] = c;
    }
    ext[n] = '\0';

    if (!strcmp(ext, "mp4") || !strcmp(ext, "m4v") || !strcmp(ext, "mov"))
        return VIDEO_FMT_MP4;
    if (!strcmp(ext, "mpg") || !strcmp(ext, "mpeg") || !strcmp(ext, "m1v") ||
        !strcmp(ext, "m2v") || !strcmp(ext, "mpv") || !strcmp(ext, "ts"))
        return VIDEO_FMT_MPEG;
    if (!strcmp(ext, "avi"))  return VIDEO_FMT_AVI;
    if (!strcmp(ext, "wmv") || !strcmp(ext, "asf")) return VIDEO_FMT_ASF;
    if (!strcmp(ext, "mkv") || !strcmp(ext, "webm")) return VIDEO_FMT_MATROSKA;
    if (!strcmp(ext, "ogv") || !strcmp(ext, "ogm")) return VIDEO_FMT_OGG;
    if (!strcmp(ext, "flv"))  return VIDEO_FMT_FLV;
    if (!strcmp(ext, "rm") || !strcmp(ext, "rmvb")) return VIDEO_FMT_REALMEDIA;

    return VIDEO_FMT_UNKNOWN;
}

int video_format_is_playable(VideoFormat fmt)
{
    /* Only the MP4 family reaches the hardware decoder.  Even then the
     * streams inside must be H.264 and AAC, which cannot be told from the
     * container signature alone -- so this is a necessary condition, not a
     * promise. */
    return fmt == VIDEO_FMT_MP4;
}

const char *video_format_name(VideoFormat fmt)
{
    switch (fmt) {
    case VIDEO_FMT_MP4:       return "MP4";
    case VIDEO_FMT_MPEG:      return "MPEG-1/2";
    case VIDEO_FMT_AVI:       return "AVI";
    case VIDEO_FMT_ASF:       return "WMV/ASF";
    case VIDEO_FMT_MATROSKA:  return "Matroska/WebM";
    case VIDEO_FMT_OGG:       return "Ogg";
    case VIDEO_FMT_FLV:       return "Flash video";
    case VIDEO_FMT_REALMEDIA: return "RealMedia";
    default:                  return "unrecognised";
    }
}
