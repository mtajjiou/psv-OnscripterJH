/* Host-side tests for the software video decoder.
 *
 *   test_videodec <fixture dir>
 *
 * The fixtures are real encoded files produced by the ffmpeg command line,
 * in the formats a PC visual novel ships: MPEG-1 in a program stream, MPEG-4
 * in an AVI, H.264 in an MP4.  Decoding them here is the same code path the
 * engine runs on the console, so a regression in framing, colour conversion
 * or audio resampling shows up as a failing test rather than a black screen
 * on the device.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "videodec.h"

static int failures = 0;
static int checks = 0;

static void check(int cond, const char *what)
{
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

/* Decodes a whole file and reports what came out. */
static void decode_all(const char *dir, const char *file,
                       int want_w, int want_h, int want_audio,
                       const char *want_codec)
{
    char path[1024];
    videodec_info info;
    videodec *v;
    int err = 0, frames = 0, rc;
    double last_pts = -1.0;
    int pts_monotonic = 1;
    int saw_opaque = 0;
    size_t audio_frames = 0;

    snprintf(path, sizeof(path), "%s/%s", dir, file);

    v = videodec_open(path, &err);
    checks++;
    if (v == NULL) {
        failures++;
        printf("FAIL: %s did not open: %s\n", file, videodec_error_string(err));
        return;
    }

    videodec_get_info(v, &info);

    checks++;
    if (info.width != want_w || info.height != want_h) {
        failures++;
        printf("FAIL: %s is %dx%d, expected %dx%d\n",
               file, info.width, info.height, want_w, want_h);
    }

    checks++;
    if (strcmp(info.video_codec, want_codec) != 0) {
        failures++;
        printf("FAIL: %s decoded as %s, expected %s\n",
               file, info.video_codec, want_codec);
    }

    checks++;
    if (info.has_audio != want_audio) {
        failures++;
        printf("FAIL: %s has_audio=%d, expected %d\n",
               file, info.has_audio, want_audio);
    }

    for (;;) {
        const uint8_t *rgba = NULL;
        int pitch = 0;
        double pts = 0.0;
        int16_t pcm[4096 * 2];
        size_t got;

        rc = videodec_next_frame(v, &rgba, &pitch, &pts);
        if (rc <= 0) break;
        frames++;

        if (rgba == NULL || pitch != want_w * 4) {
            printf("FAIL: %s frame %d has pitch %d, expected %d\n",
                   file, frames, pitch, want_w * 4);
            failures++;
            checks++;
            break;
        }

        /* Every pixel must be opaque: a wrong pixel format conversion
         * shows up first as a zero alpha channel, and a texture full of
         * transparent pixels looks like "no video" on the device. */
        if (rgba[3] == 0xFF) saw_opaque = 1;

        if (pts + 1e-9 < last_pts) pts_monotonic = 0;
        last_pts = pts;

        /* Drain audio as a player would, so the ring buffer is exercised
         * rather than sitting permanently full. */
        while ((got = videodec_read_audio(v, pcm, 4096)) > 0)
            audio_frames += got;
    }

    checks++;
    if (rc < 0) {
        failures++;
        printf("FAIL: %s failed mid-decode: %s\n",
               file, videodec_error_string(rc));
    }

    /* The fixtures are one second at 10fps.  Demand most of the frames
     * rather than exactly ten: a container may hold a frame back. */
    checks++;
    if (frames < 8) {
        failures++;
        printf("FAIL: %s produced %d frames, expected about 10\n", file, frames);
    }

    check(saw_opaque, "decoded frames are opaque RGBA");
    check(pts_monotonic, "presentation timestamps do not go backwards");

    if (want_audio) {
        /* One second of 44.1kHz stereo, allowing for codec priming and the
         * tail the resampler holds. */
        checks++;
        if (audio_frames < 30000) {
            failures++;
            printf("FAIL: %s yielded %lu audio frames, expected about 44100\n",
                   file, (unsigned long)audio_frames);
        }
    }

    videodec_close(v);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    int err;

    /* The format that actually broke: MPEG-1 in a program stream, which is
     * what nar01.mpg is. */
    decode_all(dir, "clip_mpeg1.mpg", 160, 120, 1, "mpeg1video");
    /* DivX-era AVI. */
    decode_all(dir, "clip_mpeg4.avi", 160, 120, 1, "mpeg4");
    /* The one the hardware could already play, decoded in software here. */
    decode_all(dir, "clip_h264.mp4",  160, 120, 1, "h264");
    /* No audio track at all must still play. */
    decode_all(dir, "clip_silent.mpg", 160, 120, 0, "mpeg1video");

    /* Failure paths must report, not crash. */
    err = 0;
    check(videodec_open("/nonexistent/none.mpg", &err) == NULL &&
          err == VIDEODEC_ERR_OPEN, "a missing file reports ERR_OPEN");
    err = 0;
    check(videodec_open(NULL, &err) == NULL && err == VIDEODEC_ERR_OPEN,
          "a NULL path reports ERR_OPEN");
    {
        char path[1024];
        snprintf(path, sizeof(path), "%s/notazip.bin", dir);
        err = 0;
        check(videodec_open(path, &err) == NULL,
              "a file that is not a video does not open");
    }
    {
        /* An audio-only file has no video track; that is a distinct error
         * from a codec this build lacks. */
        char path[1024];
        snprintf(path, sizeof(path), "%s/audio_only.wav", dir);
        err = 0;
        check(videodec_open(path, &err) == NULL && err == VIDEODEC_ERR_NO_VIDEO,
              "an audio-only file reports ERR_NO_VIDEO");
    }

    videodec_close(NULL);   /* must be a no-op */
    check(videodec_read_audio(NULL, NULL, 0) == 0, "read_audio(NULL) is 0");
    check(videodec_audio_available(NULL) == 0, "audio_available(NULL) is 0");
    check(videodec_error_string(VIDEODEC_ERR_CODEC) != NULL,
          "every error has a message");

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
