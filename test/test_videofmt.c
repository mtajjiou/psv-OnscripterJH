/* Host-side tests for the video container sniffer.
 *
 * The signatures here are the real leading bytes of each container, so a
 * change that breaks recognition of an actual file breaks a test.
 */
#include <stdio.h>
#include <string.h>

#include "videofmt.h"

static int failures = 0;
static int checks = 0;

static const char *fmt_name(VideoFormat f) { return video_format_name(f); }

static void expect_sniff(const char *what, const unsigned char *buf, size_t len,
                         VideoFormat want)
{
    VideoFormat got = video_format_sniff(buf, len);
    checks++;
    if (got != want) {
        failures++;
        printf("FAIL: %s sniffed as %s, expected %s\n",
               what, fmt_name(got), fmt_name(want));
    }
}

static void expect_name(const char *name, VideoFormat want)
{
    VideoFormat got = video_format_from_name(name);
    checks++;
    if (got != want) {
        failures++;
        printf("FAIL: name %s read as %s, expected %s\n",
               name, fmt_name(got), fmt_name(want));
    }
}

static void check(int cond, const char *what)
{
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

int main(void)
{
    /* An MP4 begins with a box length then "ftyp"; the brand varies. */
    {
        unsigned char mp4[32];
        memset(mp4, 0, sizeof(mp4));
        mp4[3] = 0x20;
        memcpy(mp4 + 4, "ftypisom", 8);
        expect_sniff("mp4 (isom)", mp4, sizeof(mp4), VIDEO_FMT_MP4);
        memcpy(mp4 + 4, "ftypmp42", 8);
        expect_sniff("mp4 (mp42)", mp4, sizeof(mp4), VIDEO_FMT_MP4);
        memcpy(mp4 + 4, "ftypqt  ", 8);
        expect_sniff("mov", mp4, sizeof(mp4), VIDEO_FMT_MP4);
    }

    /* MPEG program stream: the pack start code.  This is what a PC visual
     * novel's .mpg actually contains. */
    {
        unsigned char ps[16] = { 0x00, 0x00, 0x01, 0xBA };
        expect_sniff("mpeg program stream", ps, sizeof(ps), VIDEO_FMT_MPEG);
        ps[3] = 0xB3;   /* video sequence header */
        expect_sniff("mpeg video sequence", ps, sizeof(ps), VIDEO_FMT_MPEG);
        ps[3] = 0xE0;   /* PES video packet */
        expect_sniff("mpeg PES packet", ps, sizeof(ps), VIDEO_FMT_MPEG);
    }

    /* MPEG-2 transport stream: sync bytes one packet apart. */
    {
        unsigned char ts[200];
        memset(ts, 0, sizeof(ts));
        ts[0] = 0x47;
        ts[188] = 0x47;
        expect_sniff("mpeg transport stream", ts, sizeof(ts), VIDEO_FMT_MPEG);
    }

    {
        unsigned char avi[16];
        memcpy(avi, "RIFF\0\0\0\0AVI LIST", 16);
        expect_sniff("avi", avi, sizeof(avi), VIDEO_FMT_AVI);
        /* A RIFF that is not AVI (a wav, say) must not be claimed. */
        memcpy(avi, "RIFF\0\0\0\0WAVEfmt ", 16);
        expect_sniff("riff wave", avi, sizeof(avi), VIDEO_FMT_UNKNOWN);
    }

    {
        unsigned char mkv[8] = { 0x1A, 0x45, 0xDF, 0xA3 };
        expect_sniff("matroska", mkv, sizeof(mkv), VIDEO_FMT_MATROSKA);
    }
    {
        unsigned char asf[8] = { 0x30, 0x26, 0xB2, 0x75 };
        expect_sniff("wmv/asf", asf, sizeof(asf), VIDEO_FMT_ASF);
    }
    {
        unsigned char ogg[8];
        memcpy(ogg, "OggS\0\0\0\0", 8);
        expect_sniff("ogg", ogg, sizeof(ogg), VIDEO_FMT_OGG);
    }
    {
        unsigned char flv[8];
        memcpy(flv, "FLV\1\5\0\0\0", 8);
        expect_sniff("flv", flv, sizeof(flv), VIDEO_FMT_FLV);
    }

    /* Degenerate input must not be claimed or read past. */
    expect_sniff("NULL", NULL, 0, VIDEO_FMT_UNKNOWN);
    {
        unsigned char tiny[3] = { 0x00, 0x00, 0x01 };
        expect_sniff("too short for a start code", tiny, sizeof(tiny),
                     VIDEO_FMT_UNKNOWN);
    }
    {
        /* "ftyp" claimed by the length field but the buffer ends first. */
        unsigned char cut[6] = { 0, 0, 0, 0x20, 'f', 't' };
        expect_sniff("truncated ftyp", cut, sizeof(cut), VIDEO_FMT_UNKNOWN);
    }
    {
        unsigned char text[16];
        memset(text, 'a', sizeof(text));
        expect_sniff("plain text", text, sizeof(text), VIDEO_FMT_UNKNOWN);
    }

    /* Extensions, including the case a real script uses. */
    expect_name("nar01.mpg", VIDEO_FMT_MPEG);
    expect_name("op.MPEG", VIDEO_FMT_MPEG);
    expect_name("movie/ed.mp4", VIDEO_FMT_MP4);
    expect_name("clip.AVI", VIDEO_FMT_AVI);
    expect_name("clip.wmv", VIDEO_FMT_ASF);
    expect_name("clip.webm", VIDEO_FMT_MATROSKA);
    expect_name("clip.rmvb", VIDEO_FMT_REALMEDIA);
    expect_name("noextension", VIDEO_FMT_UNKNOWN);
    expect_name("archive.tar.gz", VIDEO_FMT_UNKNOWN);
    expect_name(NULL, VIDEO_FMT_UNKNOWN);
    expect_name("trailing.", VIDEO_FMT_UNKNOWN);
    expect_name("way.toolongextension", VIDEO_FMT_UNKNOWN);

    /* Only MP4 reaches the decoder. */
    check(video_format_is_playable(VIDEO_FMT_MP4), "mp4 is playable");
    check(!video_format_is_playable(VIDEO_FMT_MPEG), "mpeg is not playable");
    check(!video_format_is_playable(VIDEO_FMT_AVI), "avi is not playable");
    check(!video_format_is_playable(VIDEO_FMT_UNKNOWN),
          "unknown is not playable");

    /* Every format must have a name, since messages print it. */
    {
        int f;
        for (f = VIDEO_FMT_UNKNOWN; f <= VIDEO_FMT_REALMEDIA; f++) {
            const char *n = video_format_name((VideoFormat)f);
            check(n != NULL && n[0] != '\0', "format has a printable name");
        }
    }

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
