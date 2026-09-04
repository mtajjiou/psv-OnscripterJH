/* Host-side tests for reading a game out of its archive.
 *
 * The engine asks for files by the names a script uses -- the wrong case,
 * the wrong separator, sometimes with a "./" in front -- and a mount that
 * misses one of those spellings looks to the player like a game with
 * missing pictures rather than like a bug. That, and the rule deciding
 * which files cannot be served from an archive at all, are what is checked
 * here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zipfs.h"

static int failures = 0;
static int checks = 0;

static void check(int cond, const char *what) {
    checks++;
    if (!cond) { failures++; printf("FAIL: %s\n", what); }
}

static zipfs *mount(const char *dir, const char *name) {
    char path[512];
    zipfs *fs;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    fs = zipfs_open(path);
    if (fs == NULL) { failures++; printf("FAIL: cannot mount %s\n", name); }
    return fs;
}

static void test_lookup(const char *dir) {
    /* nested.zip wraps the game in "MyGame/": the mount addresses files as
     * the game does, from inside that folder. */
    zipfs *fs = mount(dir, "nested.zip");
    if (fs == NULL) return;

    check(zipfs_count(fs) == 4, "every file in the game is mounted");
    check(zipfs_size(fs, "nscript.dat") == 19,
          "the script is found under the game root, not under the wrapper");
    check(zipfs_size(fs, "MyGame/nscript.dat") == -1,
          "and not under the wrapper's name as well");

    check(zipfs_size(fs, "NSCRIPT.DAT") == zipfs_size(fs, "nscript.dat"),
          "case does not matter");
    check(zipfs_size(fs, "./caption.txt") == zipfs_size(fs, "caption.txt"),
          "a leading ./ does not matter");
    check(zipfs_size(fs, "missing.dat") == -1, "a file not in it is not found");

    {
        long size = zipfs_size(fs, "caption.txt");
        unsigned char *buffer = (unsigned char *)malloc((size_t)size + 1);
        size_t got = zipfs_read(fs, "caption.txt", buffer);

        check(got == (size_t)size, "a file reads back at its stated size");
        buffer[got] = '\0';
        check(strcmp((char *)buffer, "My Game Title\n") == 0,
              "and byte for byte");
        free(buffer);
    }

    {
        /* Big and deflated: several inflate chunks, all landing in the
         * caller's buffer in order. */
        long size = zipfs_size(fs, "arc.nsa");
        unsigned char *buffer = (unsigned char *)malloc((size_t)size);
        size_t got = zipfs_read(fs, "arc.nsa", buffer);
        int intact = (got == (size_t)size);
        size_t i;

        for (i = 0; intact && i < got; i++)
            if (buffer[i] != "abcdefgh"[i % 8]) intact = 0;
        check(intact, "a file spanning many chunks comes out whole");
        free(buffer);
    }

    {
        /* Stored rather than deflated: the other path through the reader. */
        long size = zipfs_size(fs, "icon.png");
        unsigned char *buffer = (unsigned char *)malloc((size_t)size);
        check(zipfs_read(fs, "icon.png", buffer) == (size_t)size,
              "a stored file reads too");
        free(buffer);
    }

    zipfs_close(fs);
}

static void test_subfolders(const char *dir) {
    /* A script writes "extra\font.ttf"; the archive holds "extra/font.ttf". */
    zipfs *fs = mount(dir, "patch.zip");
    if (fs == NULL) return;

    check(zipfs_size(fs, "MyGame English Patch v2/extra/font.ttf") == 60,
          "a file in a subfolder is mounted, at its full depth when the "
          "archive has no game root to measure from");
    check(zipfs_size(fs, "mygame english patch v2\\extra\\FONT.TTF") == 60,
          "and answers to any spelling of that path");
    zipfs_close(fs);
}

static void test_separators(const char *dir) {
    zipfs *fs = mount(dir, "deep.zip");
    if (fs == NULL) return;

    check(zipfs_size(fs, "0.txt") > 0, "the game root is where the script is");
    check(zipfs_size(fs, "backup\\old\\nscript.dat") ==
          zipfs_size(fs, "backup/old/nscript.dat"),
          "a Windows separator finds the same file as a unix one");
    zipfs_close(fs);
}

static void test_failure(void) {
    check(zipfs_open("/nonexistent/nothing.zip") == NULL,
          "an archive that is not there does not mount");
    check(zipfs_size(NULL, "x") == -1, "a failed mount answers nothing");
    check(zipfs_count(NULL) == 0, "and holds nothing");
    zipfs_close(NULL);
}

static void test_needs_disk(void) {
    check(zipfs_needs_disk("arc.nsa"), "a game archive goes to the card");
    check(zipfs_needs_disk("ARC.NSA"), "whatever its case");
    check(zipfs_needs_disk("data/voice.sar"), "so does a .sar");
    check(zipfs_needs_disk("video/op.mp4"), "and a video");
    check(zipfs_needs_disk("default.ttf"), "and a font");

    check(!zipfs_needs_disk("nscript.dat"), "the script can be read from the zip");
    check(!zipfs_needs_disk("bg/title.png"), "so can a picture");
    check(!zipfs_needs_disk("se/click.wav"), "and a sound");
    check(!zipfs_needs_disk("nsa"), "an extension is not a whole name");
    check(!zipfs_needs_disk(""), "an empty name needs nothing");
    check(!zipfs_needs_disk(NULL), "nor does no name at all");
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";

    test_lookup(dir);
    test_subfolders(dir);
    test_separators(dir);
    test_failure();
    test_needs_disk();

    printf("zipfs: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
