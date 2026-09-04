/* Host-side tests for reading either kind of archive.
 *
 * A mod arrives as a .7z as often as a .zip, and everything above the
 * reader -- the installer, the patch decisions, the game-root search --
 * has to reach the same conclusions about the same contents whichever
 * container they came in. So the checks here are mostly the same question
 * asked twice, once of each, and compared.
 *
 * The .7z fixtures need py7zr to build. When it is missing they are absent
 * and the 7z half is skipped rather than failed: the zip half still runs,
 * and a machine without py7zr is not a machine with a broken reader.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"

static int failures = 0;
static int checks = 0;
static int skipped = 0;

static void check(int cond, const char *what) {
    checks++;
    if (!cond) { failures++; printf("FAIL: %s\n", what); }
}

static void check_str(const char *got, const char *want, const char *what) {
    checks++;
    if (strcmp(got, want) != 0) {
        failures++;
        printf("FAIL: %s (got \"%s\", want \"%s\")\n", what, got, want);
    }
}

static int have(const char *dir, const char *name) {
    char path[512];
    FILE *fp;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    fp = fopen(path, "rb");
    if (fp == NULL) return 0;
    fclose(fp);
    return 1;
}

static archive *open_fixture(const char *dir, const char *name) {
    char path[512];
    int err = 0;
    archive *a;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    a = archive_open(path, &err);
    if (a == NULL) {
        printf("FAIL: cannot open %s (%s)\n", name, zip_error_string(err));
        failures++;
    }
    return a;
}

/* What the entry named `want` holds, appended by the callback. */
struct sink {
    char  *data;
    size_t len;
};

static int collect(void *user, const void *data, size_t len) {
    struct sink *s = (struct sink *)user;
    s->data = (char *)realloc(s->data, s->len + len + 1);
    memcpy(s->data + s->len, data, len);
    s->len += len;
    s->data[s->len] = '\0';
    return 0;
}

static int find_entry(archive *a, const char *name) {
    int i;
    for (i = 0; i < archive_count(a); i++)
        if (strcmp(archive_entry_name(a, i), name) == 0) return i;
    return -1;
}

/* The questions the installer asks, of whichever archive it is given. */
static void check_game_archive(archive *a, const char *what) {
    char root[ZIP_MAX_NAME];
    char label[128];
    int index;

    snprintf(label, sizeof(label), "%s: the game root is found", what);
    check(archive_find_game_root(a, root, sizeof(root)) == 1, label);
    snprintf(label, sizeof(label), "%s: and it is the wrapping folder", what);
    check_str(root, "MyGame", label);

    snprintf(label, sizeof(label), "%s: the wrapper is the common root", what);
    check(archive_common_root(a, root, sizeof(root)) == 1 &&
          strcmp(root, "MyGame") == 0, label);

    snprintf(label, sizeof(label), "%s: four files are counted", what);
    {
        int i, files = 0;
        for (i = 0; i < archive_count(a); i++)
            if (!archive_entry_is_dir(a, i)) files++;
        check(files == 4, label);
    }

    /* Byte for byte, which is the only thing that matters in the end. */
    index = find_entry(a, "MyGame/caption.txt");
    snprintf(label, sizeof(label), "%s: a file is found by name", what);
    check(index >= 0, label);
    if (index >= 0) {
        struct sink s = { NULL, 0 };
        snprintf(label, sizeof(label), "%s: and extracts", what);
        check(archive_extract_entry(a, index, collect, &s) == ZIP_OK, label);
        snprintf(label, sizeof(label), "%s: byte for byte", what);
        check(s.data && strcmp(s.data, "My Game Title\n") == 0, label);
        free(s.data);
    }

    /* Several chunks' worth, so the piece-by-piece hand-over is exercised
     * rather than only a file that fits in one. */
    index = find_entry(a, "MyGame/arc.nsa");
    if (index >= 0) {
        struct sink s = { NULL, 0 };
        size_t i;
        int intact = 1;

        archive_extract_entry(a, index, collect, &s);
        snprintf(label, sizeof(label), "%s: a large file comes out whole", what);
        check(s.len == 320000, label);
        for (i = 0; i < s.len && intact; i++)
            if (s.data[i] != "abcdefgh"[i % 8]) intact = 0;
        snprintf(label, sizeof(label), "%s: and in order", what);
        check(intact, label);
        free(s.data);
    }

    snprintf(label, sizeof(label), "%s: the total is what it will need", what);
    check(archive_total_size(a) == 320000 + 19 + 14 + 70, label);
}

static void test_both(const char *dir) {
    archive *a;

    a = open_fixture(dir, "nested.zip");
    if (a) {
        check(archive_kind_of(a) == ARCHIVE_ZIP, "a zip opens as a zip");
        check_game_archive(a, "zip");
        archive_close(a);
    }

    if (!have(dir, "nested.7z")) {
        printf("SKIPPED the 7z half: fixtures need py7zr\n");
        skipped = 1;
        return;
    }

    a = open_fixture(dir, "nested.7z");
    if (a) {
        check(archive_kind_of(a) == ARCHIVE_7Z, "a 7z opens as a 7z");
        check_game_archive(a, "7z");
        archive_close(a);
    }
}

static void test_patch_7z(const char *dir) {
    char root[ZIP_MAX_NAME];
    archive *a;

    if (skipped) return;

    a = open_fixture(dir, "patch.7z");
    if (a == NULL) return;

    check(archive_find_game_root(a, root, sizeof(root)) == 0,
          "a patch in a 7z has no game in it, as in a zip");
    check(archive_common_root(a, root, sizeof(root)) == 1,
          "but it does have a wrapping folder");
    check_str(root, "MyGame English Patch v2", "which is what would be stripped");
    archive_close(a);
}

static void test_names_7z(const char *dir) {
    archive *a;
    int index;

    if (skipped) return;

    a = open_fixture(dir, "names.7z");
    if (a == NULL) return;

    /* 7z holds names as UTF-16; everything above this deals in bytes. */
    index = find_entry(a, "\xe6\x9c\x88\xe5\xa7\xab/nscript.dat");
    check(index >= 0, "a Japanese name survives the conversion to bytes");

    index = find_entry(a, "\xe6\x9c\x88\xe5\xa7\xab/a/b/c/deep.txt");
    check(index >= 0, "and so does a deep path, with unix separators");
    archive_close(a);
}

/* The difference between the two formats that everything above the reader
 * would otherwise have to know about: a zip writes a folder as a name
 * ending in '/', a 7z as a flag on a name that does not.  The installer
 * strips that slash, so a 7z folder reported without one loses a letter
 * and every file under it fails to write. */
static void test_directory_names(const char *dir) {
    const char *names[] = { "nested.zip", "nested.7z", "patch.7z" };
    size_t k;

    for (k = 0; k < sizeof(names) / sizeof(names[0]); k++) {
        archive *a;
        int i, dirs = 0, slashed = 0;

        if (!have(dir, names[k])) continue;
        a = open_fixture(dir, names[k]);
        if (a == NULL) continue;

        for (i = 0; i < archive_count(a); i++) {
            const char *name = archive_entry_name(a, i);
            const size_t len = strlen(name);

            if (!archive_entry_is_dir(a, i)) {
                checks++;
                if (len > 0 && name[len - 1] == '/') {
                    failures++;
                    printf("FAIL: %s: a file's name ends in a slash (%s)\n",
                           names[k], name);
                }
                continue;
            }

            dirs++;
            if (len > 0 && name[len - 1] == '/') slashed++;
        }

        checks++;
        if (dirs != slashed) {
            failures++;
            printf("FAIL: %s: %d of %d folders end in a slash\n",
                   names[k], slashed, dirs);
        }
        archive_close(a);
    }
}

static void test_suffixes(void) {
    check(archive_has_suffix("game.zip"), "a .zip is an archive");
    check(archive_has_suffix("game.7z"), "so is a .7z");
    check(archive_has_suffix("GAME.7Z"), "whatever its case");
    check(!archive_has_suffix("game.rar"), "a .rar is not one this can open");
    check(!archive_has_suffix("7z"), "but a bare \"7z\" is not a name");
    check(archive_has_suffix(".zip"),
          "a file that is nothing but a suffix is still an archive, which is "
          "what the installer already has an answer for");

    check(archive_suffix_length("game.zip") == 4, "a .zip suffix is four");
    check(archive_suffix_length("game.7z") == 3, "a .7z suffix is three");
    check(archive_suffix_length("game.rar") == 0, "and anything else is none");
}

static void test_failure(void) {
    int err = 0;
    check(archive_open("/nonexistent/nothing.zip", &err) == NULL,
          "an archive that is not there does not open");
    check(archive_count(NULL) == 0, "and holds nothing");
    archive_close(NULL);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";

    test_both(dir);
    test_patch_7z(dir);
    test_names_7z(dir);
    test_directory_names(dir);
    test_suffixes();
    test_failure();

    printf("archive: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
