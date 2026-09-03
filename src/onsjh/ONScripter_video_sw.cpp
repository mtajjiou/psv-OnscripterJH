/* -*- C++ -*-
 *
 *  ONScripter_video_sw.cpp -- plays the videos the hardware decoder refuses
 *
 *  sceAvPlayer takes H.264 with AAC in an MP4 and nothing else, which leaves
 *  out the .mpg, .avi and .wmv files PC visual novels ship.  Those are
 *  decoded here in software instead, through libavcodec, and drawn with the
 *  same renderer the hardware path uses.
 *
 *  The decoding itself lives in src/common/videodec.c, which knows nothing
 *  about the Vita and is tested on the host against real encoded files.
 *  What is left here is presentation: when to show a frame, how to feed the
 *  mixer, and how to let the player skip.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "ONScripter.h"
#include "Utils.h"

extern "C" {
#include "videodec.h"
}

namespace {

/* The mixer pulls audio on its own thread while the main thread decodes, so
 * the decoder is shared state and needs a lock.  One video plays at a time,
 * so one static set of these is enough. */
struct SoftwareVideoAudio {
    videodec  *dec;
    SDL_mutex *lock;
    int        volume;   /* 0-128, SDL_mixer's scale */
};

SoftwareVideoAudio g_sw_audio = { NULL, NULL, MIX_MAX_VOLUME };

void softwareVideoAudioCallback(void *udata, Uint8 *stream, int len)
{
    SoftwareVideoAudio *a = (SoftwareVideoAudio *)udata;
    int frames = len / 4;          /* 16-bit stereo */
    size_t got = 0;

    memset(stream, 0, (size_t)len);
    if (a == NULL || a->dec == NULL || frames <= 0) return;

    if (a->lock) SDL_LockMutex(a->lock);
    got = videodec_read_audio(a->dec, (int16_t *)stream, (size_t)frames);
    if (a->lock) SDL_UnlockMutex(a->lock);

    /* Short reads leave silence, which is what the memset above already
     * put there.  Falling behind should thin the sound out, not repeat the
     * last buffer. */
    if (got > 0 && a->volume != MIX_MAX_VOLUME) {
        int16_t *p = (int16_t *)stream;
        size_t i, n = got * 2;
        for (i = 0; i < n; i++)
            p[i] = (int16_t)((int)p[i] * a->volume / MIX_MAX_VOLUME);
    }
}

}  /* namespace */

/* Returns 1 if the player asked to quit, 0 otherwise -- matching playAVC,
 * whose return value means the same thing. */
int ONScripter::playSoftwareVideo(const char *path, bool click_flag,
                                  bool loop_flag)
{
    int err = 0;
    videodec *dec = videodec_open(path, &err);

    if (dec == NULL) {
        utils::printError("video: [%s] could not be decoded: %s\n",
                          path, videodec_error_string(err));
        return 0;
    }

    videodec_info info;
    videodec_get_info(dec, &info);
    utils::printInfo("video: playing [%s] in software, %dx%d %s%s%s\n",
                     path, info.width, info.height, info.video_codec,
                     info.has_audio ? " + " : " (silent)",
                     info.has_audio ? info.audio_codec : "");

    /* Named apart from the engine's own `texture` member, which the rest of
     * the renderer draws from and which is restored at the end. */
    SDL_Texture *frame_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                               SDL_TEXTUREACCESS_STREAMING,
                                               info.width, info.height);
    if (frame_tex == NULL) {
        utils::printError("video: no texture for [%s]: %s\n", path, SDL_GetError());
        videodec_close(dec);
        return 0;
    }

    /* Letterbox into the same rectangle the hardware path draws into, so a
     * software-decoded video sits where a hardware one would. */
    SDL_Rect dst_rect = { (960 - screen_device_width) / 2,
                          (544 - screen_device_height) / 2,
                          screen_device_width, screen_device_height };

    bool hooked = false;
    if (info.has_audio && audio_open_flag) {
        /* The decoder resamples to 44100Hz stereo.  Mix_HookMusic hands us
         * the mixer's own format, so if a game opened the device at some
         * other rate the sound would play at the wrong speed -- say so
         * rather than let it sound broken for no visible reason. */
        int mix_rate = 0, mix_channels = 0;
        Uint16 mix_format = 0;
        if (Mix_QuerySpec(&mix_rate, &mix_format, &mix_channels) &&
            (mix_rate != info.audio_rate || mix_channels != info.audio_channels))
            utils::printError("video: mixer is %dHz/%dch but the decoder "
                              "produces %dHz/%dch; audio may be off pitch\n",
                              mix_rate, mix_channels,
                              info.audio_rate, info.audio_channels);

        g_sw_audio.dec    = dec;
        g_sw_audio.lock   = SDL_CreateMutex();
        g_sw_audio.volume = music_volume * MIX_MAX_VOLUME / 100;
        if (g_sw_audio.volume > MIX_MAX_VOLUME) g_sw_audio.volume = MIX_MAX_VOLUME;
        if (g_sw_audio.volume < 0) g_sw_audio.volume = 0;

        /* Decode a little ahead before the mixer starts pulling, so the
         * first moment of sound is not silence. */
        for (int i = 0; i < 4 && videodec_audio_available(dec) < 8192; i++) {
            const uint8_t *ignore_rgba = NULL;
            int ignore_pitch = 0;
            double ignore_pts = 0.0;
            if (videodec_next_frame(dec, &ignore_rgba, &ignore_pitch,
                                    &ignore_pts) <= 0)
                break;
        }

        Mix_HookMusic(softwareVideoAudioCallback, &g_sw_audio);
        hooked = true;
    }

    int ret = 0;
    bool quit = false;
    Uint32 started = SDL_GetTicks();

    while (!quit) {
        const uint8_t *rgba = NULL;
        int pitch = 0;
        double pts = 0.0;
        int rc;

        if (g_sw_audio.lock) SDL_LockMutex(g_sw_audio.lock);
        rc = videodec_next_frame(dec, &rgba, &pitch, &pts);
        if (g_sw_audio.lock) SDL_UnlockMutex(g_sw_audio.lock);

        if (rc < 0) {
            utils::printError("video: [%s] stopped early: %s\n",
                              path, videodec_error_string(rc));
            break;
        }
        if (rc == 0) {
            if (!loop_flag) break;
            /* Looping means starting the file again; reopening is cheap
             * beside decoding it and avoids a seek path that every
             * container implements differently. */
            if (g_sw_audio.lock) SDL_LockMutex(g_sw_audio.lock);
            videodec_close(dec);
            dec = videodec_open(path, &err);
            g_sw_audio.dec = dec;
            if (g_sw_audio.lock) SDL_UnlockMutex(g_sw_audio.lock);
            if (dec == NULL) break;
            started = SDL_GetTicks();
            continue;
        }

        /* Show the frame at the time it claims.  A frame that is already
         * late is drawn immediately rather than dropped: these are small
         * videos, and a stutter reads better than a gap. */
        Uint32 due = started + (Uint32)(pts * 1000.0);
        Uint32 now = SDL_GetTicks();
        while (now < due && !quit) {
            Uint32 wait = due - now;
            SDL_Delay(wait > 10 ? 10 : wait);
            now = SDL_GetTicks();

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (click_flag && event.type == SDL_JOYBUTTONDOWN) {
                    Uint8 button = event.jbutton.button;
                    if (button == 2 || button == 11) quit = true;   /* cross, start */
                }
                else if (event.type == SDL_QUIT) {
                    ret = 1;
                    quit = true;
                }
            }
        }
        if (quit) break;

        void *pixels = NULL;
        int tex_pitch = 0;
        if (SDL_LockTexture(frame_tex, NULL, &pixels, &tex_pitch) == 0) {
            if (tex_pitch == pitch) {
                memcpy(pixels, rgba, (size_t)pitch * info.height);
            }
            else {
                /* The texture may be padded; copy row by row. */
                for (int y = 0; y < info.height; y++)
                    memcpy((Uint8 *)pixels + (size_t)y * tex_pitch,
                           rgba + (size_t)y * pitch, (size_t)pitch);
            }
            SDL_UnlockTexture(frame_tex);
        }
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, frame_tex, NULL, &dst_rect);
        SDL_RenderPresent(renderer);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (click_flag && event.type == SDL_JOYBUTTONDOWN) {
                Uint8 button = event.jbutton.button;
                if (button == 2 || button == 11) quit = true;
            }
            else if (event.type == SDL_QUIT) {
                ret = 1;
                quit = true;
            }
        }
    }

    if (hooked) Mix_HookMusic(NULL, NULL);

    if (g_sw_audio.lock) SDL_LockMutex(g_sw_audio.lock);
    g_sw_audio.dec = NULL;
    if (g_sw_audio.lock) {
        SDL_mutex *lock = g_sw_audio.lock;
        g_sw_audio.lock = NULL;
        SDL_UnlockMutex(lock);
        SDL_DestroyMutex(lock);
    }

    videodec_close(dec);
    SDL_DestroyTexture(frame_tex);

    /* Put the engine's own texture back the way the hardware path leaves
     * it, so the scene after the video draws from the accumulated screen
     * rather than the last video frame. */
    if (texture) SDL_DestroyTexture(texture);
    texture = SDL_CreateTextureFromSurface(renderer, accumulation_surface);
    if (texture) {
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    return ret;
}
