/*
 *  videodec.c -- see videodec.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include "videodec.h"

#define AUDIO_RATE      44100
#define AUDIO_CHANNELS  2

/* Decoded audio arrives in whatever chunks the file uses while we are
 * looking for a video frame, so it has to be buffered until the mixer asks
 * for it.  A second and a half absorbs the gap between a large video frame
 * and the audio around it without growing without bound. */
#define AUDIO_BUFFER_FRAMES (AUDIO_RATE * 3 / 2)

struct videodec {
    AVFormatContext *fmt;
    AVCodecContext  *vdec;
    AVCodecContext  *adec;
    int              vstream;
    int              astream;

    AVPacket        *pkt;
    AVFrame         *frame;

    struct SwsContext *sws;
    uint8_t          *rgba;
    int               rgba_pitch;

    SwrContext       *swr;

    int16_t          *audio;      /* ring of interleaved stereo frames */
    size_t            audio_head;
    size_t            audio_count;

    videodec_info     info;
    int               eof;
    double            last_pts;     /* to carry on from when one is missing */
    int               have_last_pts;
};

/* ---------------------------------------------------------------- audio */

static void audio_push(videodec *v, const int16_t *src, size_t frames)
{
    size_t i;

    if (v->audio == NULL) return;

    for (i = 0; i < frames; i++) {
        size_t slot;

        if (v->audio_count == AUDIO_BUFFER_FRAMES) {
            /* Full: drop the oldest frame.  Losing a few milliseconds of
             * sound beats stalling the video, and this only happens when
             * the caller is not draining. */
            v->audio_head = (v->audio_head + 1) % AUDIO_BUFFER_FRAMES;
            v->audio_count--;
        }
        slot = (v->audio_head + v->audio_count) % AUDIO_BUFFER_FRAMES;
        v->audio[slot * AUDIO_CHANNELS]     = src[i * AUDIO_CHANNELS];
        v->audio[slot * AUDIO_CHANNELS + 1] = src[i * AUDIO_CHANNELS + 1];
        v->audio_count++;
    }
}

size_t videodec_read_audio(videodec *v, int16_t *dst, size_t max_frames)
{
    size_t n = 0;

    if (v == NULL || dst == NULL || v->audio == NULL) return 0;

    while (n < max_frames && v->audio_count > 0) {
        dst[n * AUDIO_CHANNELS]     = v->audio[v->audio_head * AUDIO_CHANNELS];
        dst[n * AUDIO_CHANNELS + 1] = v->audio[v->audio_head * AUDIO_CHANNELS + 1];
        v->audio_head = (v->audio_head + 1) % AUDIO_BUFFER_FRAMES;
        v->audio_count--;
        n++;
    }
    return n;
}

size_t videodec_audio_available(const videodec *v)
{
    return v ? v->audio_count : 0;
}

static void decode_audio_frame(videodec *v, AVFrame *f)
{
    uint8_t *out = NULL;
    int out_frames, got;

    if (v->swr == NULL) return;

    /* Resampling can hold samples back, so ask for the worst case. */
    out_frames = (int)av_rescale_rnd(swr_get_delay(v->swr, v->adec->sample_rate)
                                     + f->nb_samples,
                                     AUDIO_RATE, v->adec->sample_rate,
                                     AV_ROUND_UP);
    if (out_frames <= 0) return;

    if (av_samples_alloc(&out, NULL, AUDIO_CHANNELS, out_frames,
                         AV_SAMPLE_FMT_S16, 0) < 0)
        return;

    got = swr_convert(v->swr, &out, out_frames,
                      (const uint8_t **)f->extended_data, f->nb_samples);
    if (got > 0) audio_push(v, (const int16_t *)out, (size_t)got);

    av_freep(&out);
}

/* ---------------------------------------------------------------- open */

static int open_stream(videodec *v, enum AVMediaType type,
                       AVCodecContext **out, int *index)
{
    const AVCodec *codec = NULL;
    AVCodecContext *ctx;
    int idx;

    idx = av_find_best_stream(v->fmt, type, -1, -1, &codec, 0);
    if (idx < 0 || codec == NULL) return idx < 0 ? idx : AVERROR_DECODER_NOT_FOUND;

    ctx = avcodec_alloc_context3(codec);
    if (ctx == NULL) return AVERROR(ENOMEM);

    if (avcodec_parameters_to_context(ctx, v->fmt->streams[idx]->codecpar) < 0) {
        avcodec_free_context(&ctx);
        return AVERROR(EINVAL);
    }

    /* The Vita has four cores and these are old codecs; letting libavcodec
     * thread the decode is the difference between comfortable and not. */
    ctx->thread_count = 0;

    if (avcodec_open2(ctx, codec, NULL) < 0) {
        avcodec_free_context(&ctx);
        return AVERROR_DECODER_NOT_FOUND;
    }

    *out = ctx;
    *index = idx;
    return 0;
}

static void fill_info(videodec *v)
{
    AVStream *st;

    memset(&v->info, 0, sizeof(v->info));

    if (v->vdec) {
        v->info.has_video = 1;
        v->info.width  = v->vdec->width;
        v->info.height = v->vdec->height;
        snprintf(v->info.video_codec, sizeof(v->info.video_codec), "%s",
                 avcodec_get_name(v->vdec->codec_id));

        st = v->fmt->streams[v->vstream];
        if (st->avg_frame_rate.den > 0 && st->avg_frame_rate.num > 0)
            v->info.frame_rate = av_q2d(st->avg_frame_rate);
    }

    if (v->fmt->duration > 0)
        v->info.duration = (double)v->fmt->duration / AV_TIME_BASE;

    if (v->adec) {
        v->info.has_audio      = 1;
        v->info.audio_rate     = AUDIO_RATE;
        v->info.audio_channels = AUDIO_CHANNELS;
        snprintf(v->info.audio_codec, sizeof(v->info.audio_codec), "%s",
                 avcodec_get_name(v->adec->codec_id));
    }
}

videodec *videodec_open(const char *path, int *err)
{
    videodec *v;
    int rc;
    int video_error = VIDEODEC_OK;

    if (err) *err = VIDEODEC_OK;
    if (path == NULL) {
        if (err) *err = VIDEODEC_ERR_OPEN;
        return NULL;
    }

    v = (videodec *)calloc(1, sizeof(*v));
    if (v == NULL) {
        if (err) *err = VIDEODEC_ERR_MEMORY;
        return NULL;
    }
    v->vstream = v->astream = -1;

    if (avformat_open_input(&v->fmt, path, NULL, NULL) < 0) {
        if (err) *err = VIDEODEC_ERR_OPEN;
        videodec_close(v);
        return NULL;
    }
    if (avformat_find_stream_info(v->fmt, NULL) < 0) {
        if (err) *err = VIDEODEC_ERR_OPEN;
        videodec_close(v);
        return NULL;
    }

    /* Why the video did not open, kept in case the audio does not either
     * and there is a failure to explain. */
    rc = open_stream(v, AVMEDIA_TYPE_VIDEO, &v->vdec, &v->vstream);
    if (rc == 0 && (v->vdec->width <= 0 || v->vdec->height <= 0)) {
        avcodec_free_context(&v->vdec);
        v->vstream = -1;
        rc = AVERROR_DECODER_NOT_FOUND;
    }

    /* Tell "no video track" apart from "this build cannot decode it": the
     * first is an audio file or a broken one, the second is a build to
     * fix. */
    video_error = (rc == 0) ? VIDEODEC_OK
                : ((rc == AVERROR_STREAM_NOT_FOUND ||
                    rc == AVERROR_DECODER_NOT_FOUND)
                   ? (av_find_best_stream(v->fmt, AVMEDIA_TYPE_VIDEO,
                                          -1, -1, NULL, 0) < 0
                      ? VIDEODEC_ERR_NO_VIDEO : VIDEODEC_ERR_CODEC)
                   : VIDEODEC_ERR_NO_VIDEO);

    /* Audio is optional: a video with a codec we cannot decode still plays
     * silently, which is better than not playing. */
    if (open_stream(v, AVMEDIA_TYPE_AUDIO, &v->adec, &v->astream) == 0) {
        AVChannelLayout out_layout = AV_CHANNEL_LAYOUT_STEREO;

        if (swr_alloc_set_opts2(&v->swr, &out_layout, AV_SAMPLE_FMT_S16,
                                AUDIO_RATE, &v->adec->ch_layout,
                                v->adec->sample_fmt, v->adec->sample_rate,
                                0, NULL) < 0 || swr_init(v->swr) < 0) {
            swr_free(&v->swr);
            avcodec_free_context(&v->adec);
            v->astream = -1;
        }
        else {
            v->audio = (int16_t *)calloc(AUDIO_BUFFER_FRAMES * AUDIO_CHANNELS,
                                         sizeof(int16_t));
            if (v->audio == NULL) {
                swr_free(&v->swr);
                avcodec_free_context(&v->adec);
                v->astream = -1;
            }
        }
    }

    /* Neither picture nor sound is something to play.  With one of the
     * two, there is: a file whose video codec is missing from this build
     * still has its dialogue and its music, and a scene with sound and no
     * picture is closer to the game than a scene that was skipped. */
    if (v->vdec == NULL && v->adec == NULL) {
        if (err) *err = video_error;
        videodec_close(v);
        return NULL;
    }

    v->pkt   = av_packet_alloc();
    v->frame = av_frame_alloc();
    if (v->vdec) {
        v->rgba_pitch = v->vdec->width * 4;
        v->rgba = (uint8_t *)av_malloc((size_t)v->rgba_pitch * v->vdec->height);
    }
    if (v->pkt == NULL || v->frame == NULL ||
        (v->vdec != NULL && v->rgba == NULL)) {
        if (err) *err = VIDEODEC_ERR_MEMORY;
        videodec_close(v);
        return NULL;
    }

    fill_info(v);
    return v;
}

void videodec_close(videodec *v)
{
    if (v == NULL) return;

    if (v->sws) sws_freeContext(v->sws);
    if (v->swr) swr_free(&v->swr);
    if (v->rgba) av_free(v->rgba);
    if (v->frame) av_frame_free(&v->frame);
    if (v->pkt) av_packet_free(&v->pkt);
    if (v->vdec) avcodec_free_context(&v->vdec);
    if (v->adec) avcodec_free_context(&v->adec);
    if (v->fmt) avformat_close_input(&v->fmt);
    free(v->audio);
    free(v);
}

void videodec_get_info(const videodec *v, videodec_info *out)
{
    if (v == NULL || out == NULL) return;
    *out = v->info;
}

/* --------------------------------------------------------------- frames */

static int convert_frame(videodec *v, AVFrame *f)
{
    uint8_t *dst[4] = { v->rgba, NULL, NULL, NULL };
    int pitches[4]  = { v->rgba_pitch, 0, 0, 0 };

    /* Built lazily: the pixel format is not reliably known until a frame
     * has actually come out of the decoder. */
    v->sws = sws_getCachedContext(v->sws,
                                  f->width, f->height, (enum AVPixelFormat)f->format,
                                  v->vdec->width, v->vdec->height, AV_PIX_FMT_RGBA,
                                  SWS_BILINEAR, NULL, NULL, NULL);
    if (v->sws == NULL) return VIDEODEC_ERR_MEMORY;

    sws_scale(v->sws, (const uint8_t *const *)f->data, f->linesize,
              0, f->height, dst, pitches);
    return VIDEODEC_OK;
}

int videodec_next_frame(videodec *v, const uint8_t **rgba, int *pitch,
                        double *pts)
{
    if (v == NULL) return VIDEODEC_ERR_DECODE;
    /* Nothing to show.  Reported as the end rather than as an error, so a
     * caller that does not check has_video stops instead of looping. */
    if (v->vdec == NULL) return 0;

    for (;;) {
        int rc = avcodec_receive_frame(v->vdec, v->frame);

        if (rc == 0) {
            int cv = convert_frame(v, v->frame);
            if (cv != VIDEODEC_OK) return cv;

            {
                /* Not every frame carries a timestamp -- the last frame of
                 * an MPEG-1 program stream routinely does not.  Treating
                 * that as time zero sends a player back to the start of the
                 * clip for one frame, so continue from the previous frame
                 * at the stream's frame rate instead. */
                int64_t t = v->frame->best_effort_timestamp;
                double  at;

                if (t != AV_NOPTS_VALUE) {
                    at = t * av_q2d(v->fmt->streams[v->vstream]->time_base);
                }
                else if (v->have_last_pts) {
                    at = v->last_pts + (v->info.frame_rate > 0.0
                                        ? 1.0 / v->info.frame_rate : 0.0);
                }
                else {
                    at = 0.0;
                }

                v->last_pts = at;
                v->have_last_pts = 1;
                if (pts) *pts = at;
            }
            if (rgba)  *rgba  = v->rgba;
            if (pitch) *pitch = v->rgba_pitch;
            av_frame_unref(v->frame);
            return 1;
        }
        if (rc != AVERROR(EAGAIN) && rc != AVERROR_EOF)
            return VIDEODEC_ERR_DECODE;

        if (rc == AVERROR_EOF) return 0;

        /* The decoder wants more input. */
        if (v->eof) {
            avcodec_send_packet(v->vdec, NULL);   /* drain */
            v->eof = 2;
            continue;
        }

        if (av_read_frame(v->fmt, v->pkt) < 0) {
            v->eof = 1;
            continue;
        }

        if (v->pkt->stream_index == v->vstream) {
            if (avcodec_send_packet(v->vdec, v->pkt) < 0) {
                av_packet_unref(v->pkt);
                return VIDEODEC_ERR_DECODE;
            }
        }
        else if (v->pkt->stream_index == v->astream && v->adec) {
            if (avcodec_send_packet(v->adec, v->pkt) == 0) {
                AVFrame *af = av_frame_alloc();
                if (af) {
                    while (avcodec_receive_frame(v->adec, af) == 0) {
                        decode_audio_frame(v, af);
                        av_frame_unref(af);
                    }
                    av_frame_free(&af);
                }
            }
        }
        av_packet_unref(v->pkt);
    }
}

int videodec_pump(videodec *v)
{
    if (v == NULL) return VIDEODEC_ERR_DECODE;
    if (v->adec == NULL) return 0;

    if (v->eof) {
        /* Drained once, then finished: the buffered audio is still there
         * to be read out by the caller. */
        if (v->eof == 1) {
            AVFrame *af = av_frame_alloc();
            avcodec_send_packet(v->adec, NULL);
            if (af) {
                while (avcodec_receive_frame(v->adec, af) == 0) {
                    decode_audio_frame(v, af);
                    av_frame_unref(af);
                }
                av_frame_free(&af);
            }
            v->eof = 2;
        }
        return 0;
    }

    if (av_read_frame(v->fmt, v->pkt) < 0) {
        v->eof = 1;
        return 1;                     /* one more call drains the decoder */
    }

    if (v->pkt->stream_index == v->astream) {
        if (avcodec_send_packet(v->adec, v->pkt) == 0) {
            AVFrame *af = av_frame_alloc();
            if (af) {
                while (avcodec_receive_frame(v->adec, af) == 0) {
                    decode_audio_frame(v, af);
                    av_frame_unref(af);
                }
                av_frame_free(&af);
            }
        }
    }
    av_packet_unref(v->pkt);
    return 1;
}

const char *videodec_error_string(int err)
{
    switch (err) {
    case VIDEODEC_OK:            return "no error";
    case VIDEODEC_ERR_OPEN:      return "the file could not be opened";
    case VIDEODEC_ERR_NO_VIDEO:  return "the file has no video track";
    case VIDEODEC_ERR_CODEC:     return "this build cannot decode that codec";
    case VIDEODEC_ERR_MEMORY:    return "out of memory";
    case VIDEODEC_ERR_DECODE:    return "the video could not be decoded";
    default:                     return "unknown error";
    }
}
