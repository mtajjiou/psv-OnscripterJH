/*
 *  videofmt.h -- identifies a video container from its first bytes
 *
 *  The Vita decodes video through sceAvPlayer, which handles H.264 video and
 *  AAC audio in an MP4 container and nothing else.  Scripts written for the
 *  PC ask for whatever their author had -- .mpg, .avi, .wmv -- and handing
 *  one of those to sceAvPlayer just fails, silently, so the scene appears to
 *  do nothing at all.
 *
 *  Naming the format lets the engine say what is wrong and what would fix
 *  it, instead of skipping the video with no explanation.  The container is
 *  read from the file's own bytes rather than its extension, because a
 *  misnamed file is exactly the case where the extension misleads.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef VIDEOFMT_H
#define VIDEOFMT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VIDEO_FMT_UNKNOWN = 0,
    VIDEO_FMT_MP4,       /* ISO base media: mp4, m4v, mov -- the playable one */
    VIDEO_FMT_MPEG,      /* MPEG-1/2 program or elementary stream: .mpg/.mpeg */
    VIDEO_FMT_AVI,
    VIDEO_FMT_ASF,       /* .wmv/.asf */
    VIDEO_FMT_MATROSKA,  /* .mkv/.webm */
    VIDEO_FMT_OGG,       /* .ogv/.ogm */
    VIDEO_FMT_FLV,
    VIDEO_FMT_REALMEDIA
} VideoFormat;

/* Enough bytes for every signature below.  Read at least this many. */
#define VIDEO_FMT_SNIFF_LEN 32

/* Identifies the container in buf.  A short or unrecognised buffer gives
 * VIDEO_FMT_UNKNOWN. */
VideoFormat video_format_sniff(const void *buf, size_t len);

/* Guesses from a file name, for when the bytes are not at hand.  Weaker than
 * sniffing -- an extension is a claim, not evidence. */
VideoFormat video_format_from_name(const char *name);

/* Whether the Vita's decoder can play this container at all. */
int video_format_is_playable(VideoFormat fmt);

/* A short name for messages: "MPEG-1/2", "AVI", ... */
const char *video_format_name(VideoFormat fmt);

#ifdef __cplusplus
}
#endif

#endif
