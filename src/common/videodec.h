/*
 *  videodec.h -- software video decoding for the formats the Vita's
 *                hardware decoder will not take
 *
 *  sceAvPlayer handles H.264 with AAC in an MP4 and refuses everything else,
 *  which leaves the .mpg, .avi and .wmv files that PC visual novels actually
 *  ship.  This decodes those with libavcodec instead: frames come out as
 *  RGBA ready to hand to a texture, audio as interleaved 16-bit stereo ready
 *  to feed a mixer.
 *
 *  The interface is pull-based and owns no timing policy -- the caller
 *  decides when to show a frame from the presentation timestamp it is given,
 *  because the Vita path and the host tests want very different things
 *  there.
 *
 *  Nothing here is Vita-specific, so it builds and is tested on the host.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef VIDEODEC_H
#define VIDEODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct videodec videodec;

enum {
    VIDEODEC_OK = 0,
    VIDEODEC_ERR_OPEN = -1,      /* the file will not open or has no streams */
    VIDEODEC_ERR_NO_VIDEO = -2,  /* no video stream in it */
    VIDEODEC_ERR_CODEC = -3,     /* the codec is not in this build */
    VIDEODEC_ERR_MEMORY = -4,
    VIDEODEC_ERR_DECODE = -5
};

typedef struct {
    /* A file opens when either its picture or its sound can be decoded.
     * has_video says which of the two you have; width and height are 0
     * when there is no picture. */
    int      has_video;
    int      width;
    int      height;
    double   duration;        /* seconds; 0 when the container does not say */
    double   frame_rate;      /* frames per second; 0 when unknown */
    int      has_audio;
    int      audio_rate;      /* output sample rate, Hz */
    int      audio_channels;  /* output channel count (always 2 today) */
    char     video_codec[32];
    char     audio_codec[32];
} videodec_info;

/* Opens path for decoding.  Returns NULL and sets *err only when neither
 * the video nor the audio can be decoded: a file whose video codec is
 * missing from this build still opens for its sound. */
videodec *videodec_open(const char *path, int *err);

void videodec_close(videodec *v);

/* Describes the streams.  Valid as soon as the handle exists. */
void videodec_get_info(const videodec *v, videodec_info *out);

/* Decodes the next video frame into an RGBA buffer owned by the decoder,
 * valid until the next call.  *pts is the presentation time in seconds.
 * Returns 1 on a frame, 0 at end of stream, and a negative error code on
 * failure.  Audio decoded along the way is buffered for videodec_read_audio. */
int videodec_next_frame(videodec *v, const uint8_t **rgba, int *pitch,
                        double *pts);

/* Copies up to max_samples interleaved stereo sample *frames* out of the
 * buffer filled by decoding, returning how many were written.  Returns 0
 * when nothing is buffered yet -- the caller should output silence rather
 * than block. */
size_t videodec_read_audio(videodec *v, int16_t *dst, size_t max_samples);

/* How many sample frames are waiting.  Lets a caller decode ahead until the
 * audio buffer is deep enough to start. */
size_t videodec_audio_available(const videodec *v);

/* Advances a file that has no picture, filling the audio buffer.  Returns 1
 * while there is more to come, 0 at the end, negative on error.  Callers
 * with video use videodec_next_frame, which pumps the audio as it goes. */
int videodec_pump(videodec *v);

const char *videodec_error_string(int err);

#ifdef __cplusplus
}
#endif

#endif
