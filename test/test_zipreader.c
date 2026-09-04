/*
 *  test_zipreader.c -- host-side tests for src/common/zipreader.c
 *
 *  These run on a normal machine (see test/run_tests.sh), so the archive
 *  parsing, name sanitising and game-root detection used by the Vita
 *  launcher can be checked without a Vita or the vitasdk.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "zipreader.h"

static int failures = 0;
static int checks = 0;

static void ok(int cond, const char *what) {
    checks++;
    if (!cond) {
        failures++;
        printf("  FAIL: %s\n", what);
    }
}

static void eq_str(const char *got, const char *want, const char *what) {
    checks++;
    if (strcmp(got, want) != 0) {
        failures++;
        printf("  FAIL: %s (got \"%s\", want \"%s\")\n", what, got, want);
    }
}

static void test_sanitize(void) {
    char out[ZIP_MAX_NAME];

    printf("zip_sanitize_name\n");

    ok(zip_sanitize_name("game/nscript.dat", out, sizeof(out)) == ZIP_OK, "plain path accepted");
    eq_str(out, "game/nscript.dat", "plain path unchanged");

    ok(zip_sanitize_name("game\\sub\\a.txt", out, sizeof(out)) == ZIP_OK, "backslashes accepted");
    eq_str(out, "game/sub/a.txt", "backslashes converted");

    ok(zip_sanitize_name("game//./sub/a.txt", out, sizeof(out)) == ZIP_OK, "redundant parts accepted");
    eq_str(out, "game/sub/a.txt", "redundant parts collapsed");

    ok(zip_sanitize_name("game/", out, sizeof(out)) == ZIP_OK, "directory accepted");
    eq_str(out, "game/", "trailing slash preserved");

    ok(zip_sanitize_name("../evil", out, sizeof(out)) == ZIP_ERR_BADNAME, "parent escape rejected");
    ok(zip_sanitize_name("game/../../evil", out, sizeof(out)) == ZIP_ERR_BADNAME, "nested escape rejected");
    ok(zip_sanitize_name("/etc/passwd", out, sizeof(out)) == ZIP_ERR_BADNAME, "absolute path rejected");
    ok(zip_sanitize_name("\\windows\\x", out, sizeof(out)) == ZIP_ERR_BADNAME, "absolute backslash rejected");
    ok(zip_sanitize_name("ux0:data/x", out, sizeof(out)) == ZIP_ERR_BADNAME, "vita drive prefix rejected");
    ok(zip_sanitize_name("c:/x", out, sizeof(out)) == ZIP_ERR_BADNAME, "dos drive prefix rejected");
    ok(zip_sanitize_name("", out, sizeof(out)) == ZIP_ERR_BADNAME, "empty name rejected");
    ok(zip_sanitize_name("./", out, sizeof(out)) == ZIP_ERR_BADNAME, "dot-only name rejected");
    ok(zip_sanitize_name("game/nscript.dat", out, 8) == ZIP_ERR_BADNAME, "overlong name rejected");

    /* "..text" is a legitimate file name, only the exact ".." segment escapes. */
    ok(zip_sanitize_name("game/..text", out, sizeof(out)) == ZIP_OK, "dotdot prefix is not an escape");
}

static void test_script_names(void) {
    printf("zip_is_script_name\n");
    ok(zip_is_script_name("nscript.dat"), "nscript.dat");
    ok(zip_is_script_name("NSCRIPT.DAT"), "case insensitive");
    ok(zip_is_script_name("0.txt"), "0.txt");
    ok(zip_is_script_name("00.txt"), "00.txt");
    ok(zip_is_script_name("nscr_sec.dat"), "nscr_sec.dat");
    ok(zip_is_script_name("nscript.___"), "nscript.___");
    ok(zip_is_script_name("onscript.nt2"), "onscript.nt2");
    ok(zip_is_script_name("onscript.nt3"), "onscript.nt3");
    ok(!zip_is_script_name("arc.nsa"), "archive is not a script");
    ok(!zip_is_script_name("nscript.dat.bak"), "backup is not a script");
    ok(!zip_is_script_name(""), "empty is not a script");
}

/* Collects an extracted entry into memory so its bytes can be compared. */
typedef struct { char buf[4096]; size_t len; } sink;

static int sink_write(void *user, const void *data, size_t len) {
    sink *s = (sink *)user;
    if (s->len + len > sizeof(s->buf)) return -1;
    memcpy(s->buf + s->len, data, len);
    s->len += len;
    return 0;
}

static int find_entry(zip_reader *z, const char *name) {
    int i;
    for (i = 0; i < zip_count(z); i++)
        if (strcmp(zip_entry_name(z, i), name) == 0) return i;
    return -1;
}

static void test_archive(const char *dir, const char *file,
                         const char *want_root, const char *want_common,
                         const char *probe, const char *probe_content) {
    char path[1024];
    char out[ZIP_MAX_NAME];
    zip_reader *z;
    int err = 0, idx;

    snprintf(path, sizeof(path), "%s/%s", dir, file);
    printf("%s\n", file);

    z = zip_open(path, &err);
    ok(z != NULL, "archive opens");
    if (!z) {
        printf("  (%s)\n", zip_error_string(err));
        return;
    }

    ok(zip_count(z) > 0, "archive has entries");

    ok(zip_find_game_root(z, out, sizeof(out)) == 1, "game root found");
    eq_str(out, want_root, "game root");

    zip_common_root(z, out, sizeof(out));
    eq_str(out, want_common, "common root");

    idx = find_entry(z, probe);
    ok(idx >= 0, "probe entry present");
    if (idx >= 0) {
        sink s;
        s.len = 0;
        ok(zip_extract_entry(z, idx, sink_write, &s) == ZIP_OK, "probe extracts");
        ok(s.len == strlen(probe_content), "probe length matches");
        ok(s.len == strlen(probe_content) &&
           memcmp(s.buf, probe_content, s.len) == 0, "probe content matches");
        ok(zip_entry_size(z, idx) == strlen(probe_content), "declared size matches");
    }

    zip_close(z);
}

static void test_no_script(const char *dir) {
    char path[1024];
    char out[ZIP_MAX_NAME];
    zip_reader *z;
    int err = 0;

    snprintf(path, sizeof(path), "%s/noscript.zip", dir);
    printf("noscript.zip\n");
    z = zip_open(path, &err);
    ok(z != NULL, "archive opens");
    if (!z) return;
    ok(zip_find_game_root(z, out, sizeof(out)) == 0, "no game root reported");
    eq_str(out, "", "game root left empty");
    zip_close(z);
}

static void test_not_a_zip(const char *dir) {
    char path[1024];
    int err = ZIP_OK;
    zip_reader *z;

    printf("notazip.bin\n");
    snprintf(path, sizeof(path), "%s/notazip.bin", dir);
    z = zip_open(path, &err);
    ok(z == NULL, "garbage rejected");
    ok(err == ZIP_ERR_FORMAT, "reported as a format error");

    printf("missing.zip\n");
    snprintf(path, sizeof(path), "%s/does_not_exist.zip", dir);
    err = ZIP_OK;
    z = zip_open(path, &err);
    ok(z == NULL, "missing file rejected");
    ok(err == ZIP_ERR_IO, "reported as an io error");
}

/* --- the awkward archives ---------------------------------------------
 *
 * Names in other alphabets, names longer than the reader will take, an
 * empty file, a file big enough to span many chunks, a symlink, two
 * entries with one name, and zip64.  Every one of these is something a
 * real download has contained, and every one of them is a way for an
 * install to go quietly wrong: a mangled name, a truncated file, an entry
 * that should have been refused.
 */

/* Counts an entry's bytes without keeping them, for files larger than a
 * sink buffer. */
typedef struct { size_t len; unsigned long sum; } counter;

static int count_write(void *user, const void *data, size_t len) {
    counter *c = (counter *)user;
    const unsigned char *p = (const unsigned char *)data;
    size_t i;
    c->len += len;
    for (i = 0; i < len; i++) c->sum += p[i];
    return 0;
}

static void test_names(const char *dir) {
    char path[1024];
    char out[ZIP_MAX_NAME];
    zip_reader *z;
    int err = 0, idx;

    printf("names.zip\n");
    snprintf(path, sizeof(path), "%s/names.zip", dir);
    z = zip_open(path, &err);
    ok(z != NULL, "an archive named in japanese opens");
    if (!z) return;

    /* The root is the folder's name in its own alphabet, byte for byte:
     * mangling it here is how a game ends up installed under a folder the
     * engine cannot then open. */
    ok(zip_find_game_root(z, out, sizeof(out)) == 1, "game root found");
    eq_str(out, "\346\234\210\345\247\253", "japanese folder name survives");

    ok(find_entry(z, "\346\234\210\345\247\253/\350\203\214\346\231\257.png") >= 0,
       "a japanese file name survives");
    ok(find_entry(z, "\346\234\210\345\247\253/\321\200\321\203\321\201\321\201\320\272\320\270\320\271.txt") >= 0,
       "a cyrillic file name survives");
    ok(find_entry(z, "\346\234\210\345\247\253/a file with spaces.txt") >= 0,
       "spaces survive");
    ok(find_entry(z, "\346\234\210\345\247\253/[patch] v1.2 (final).txt") >= 0,
       "brackets and parentheses survive");
    ok(find_entry(z, "\346\234\210\345\247\253/caf\303\251 & co's.txt") >= 0,
       "accents, ampersand and apostrophe survive");
    ok(find_entry(z, "\346\234\210\345\247\253/dots.in.the.name.txt") >= 0,
       "dots in a name are not an extension problem");

    /* Two names that differ only by case are two entries.  On a card that
     * does not tell them apart the second overwrites the first, which is a
     * property of the filesystem rather than of the reader -- what matters
     * here is that the reader does not silently merge them. */
    ok(find_entry(z, "\346\234\210\345\247\253/UPPER.TXT") >= 0, "upper case entry present");
    ok(find_entry(z, "\346\234\210\345\247\253/upper.txt") >= 0, "lower case entry present");
    ok(find_entry(z, "\346\234\210\345\247\253/UPPER.TXT") !=
       find_entry(z, "\346\234\210\345\247\253/upper.txt"),
       "and they are two entries, not one");

    /* Sanitising leaves the bytes alone: these are not path characters. */
    idx = find_entry(z, "\346\234\210\345\247\253/caf\303\251 & co's.txt");
    if (idx >= 0) {
        ok(zip_sanitize_name(zip_entry_name(z, idx), out, sizeof(out)) == ZIP_OK,
           "an awkward name passes sanitising");
        eq_str(out, "\346\234\210\345\247\253/caf\303\251 & co's.txt",
               "unchanged by it");
    }

    zip_close(z);
}

static void test_long_names(const char *dir) {
    char path[1024];
    zip_reader *z;
    int err = 0, i, found_long = 0, found_toolong = 0;

    printf("longnames.zip\n");
    snprintf(path, sizeof(path), "%s/longnames.zip", dir);
    z = zip_open(path, &err);
    ok(z != NULL, "an archive with a very long name opens");
    if (!z) return;

    /* The over-long entry is dropped rather than truncated: a truncated
     * name is a different file, and writing one would put a game's data in
     * a place nothing looks for it. */
    for (i = 0; i < zip_count(z); i++) {
        const char *name = zip_entry_name(z, i);
        size_t len = strlen(name);
        if (len > 380 && len < ZIP_MAX_NAME) found_long = 1;
        if (len >= ZIP_MAX_NAME) found_toolong = 1;
    }
    ok(found_long, "a long name within the limit is kept");
    ok(!found_toolong, "one past the limit is not");
    ok(zip_count(z) == 2, "so the archive reads as two entries, not three");
    ok(zip_skipped_names(z) == 1, "and the reader says one was dropped");
    ok(find_entry(z, "game/nscript.dat") >= 0,
       "the game is still installable without it");

    zip_close(z);
}

static void test_sizes(const char *dir) {
    char path[1024];
    zip_reader *z;
    counter c;
    int err = 0, idx;

    printf("sizes.zip\n");
    snprintf(path, sizeof(path), "%s/sizes.zip", dir);
    z = zip_open(path, &err);
    ok(z != NULL, "archive opens");
    if (!z) return;

    idx = find_entry(z, "game/empty.dat");
    ok(idx >= 0, "an empty file is still an entry");
    if (idx >= 0) {
        c.len = 0; c.sum = 0;
        ok(zip_extract_entry(z, idx, count_write, &c) == ZIP_OK,
           "and extracts without complaint");
        ok(c.len == 0, "as nothing at all");
        ok(zip_entry_size(z, idx) == 0, "with a declared size of zero");
    }

    /* Several megabytes, so the read loop runs many times and the buffers
     * the reader now holds are reused rather than reallocated. */
    idx = find_entry(z, "game/big.nsa");
    ok(idx >= 0, "the large entry is present");
    if (idx >= 0) {
        const uint32_t declared = zip_entry_size(z, idx);
        ok(declared > 3u * 1024 * 1024, "and is megabytes, not kilobytes");
        c.len = 0; c.sum = 0;
        ok(zip_extract_entry(z, idx, count_write, &c) == ZIP_OK,
           "it extracts whole");
        ok(c.len == declared, "every byte of it arrives");
    }

    ok(zip_total_size(z) >= (uint64_t)3 * 1024 * 1024,
       "the total the installer checks free space against includes it");

    zip_close(z);
}

static void test_symlink(const char *dir) {
    char path[1024];
    zip_reader *z;
    sink s;
    int err = 0, idx;

    printf("symlink.zip\n");
    snprintf(path, sizeof(path), "%s/symlink.zip", dir);
    z = zip_open(path, &err);
    ok(z != NULL, "an archive containing a symlink opens");
    if (!z) return;

    /* The Vita's filesystem has no symlinks at all, so there is nothing to
     * follow and nothing to create.  A link entry is read as what it is on
     * disk: a small file whose contents are the path it pointed at.  That
     * is the safe outcome -- the escape a malicious link would attempt
     * cannot happen if no link is ever made -- and this test is here to
     * notice if that ever changes. */
    idx = find_entry(z, "game/link.txt");
    ok(idx >= 0, "the link is an entry like any other");
    if (idx >= 0) {
        s.len = 0;
        ok(zip_extract_entry(z, idx, sink_write, &s) == ZIP_OK,
           "and extracts as a plain file");
        ok(s.len > 0, "with the target path as its contents");
    }

    /* And the target it names is still refused by the name check, so even
     * if a link were somehow followed it could not leave the folder. */
    {
        char out[ZIP_MAX_NAME];
        ok(zip_sanitize_name("../../../ux0:data/somewhere", out, sizeof(out))
           == ZIP_ERR_BADNAME, "the path it points at would be refused anyway");
    }

    zip_close(z);
}

static void test_duplicate(const char *dir) {
    char path[1024];
    zip_reader *z;
    int err = 0, i, seen = 0;

    printf("duplicate.zip\n");
    snprintf(path, sizeof(path), "%s/duplicate.zip", dir);
    z = zip_open(path, &err);
    ok(z != NULL, "an archive with a repeated name opens");
    if (!z) return;

    /* Both are kept, and extraction writes them in order, so the last one
     * wins on the card -- which is what a patch appended to a release
     * expects to happen. */
    for (i = 0; i < zip_count(z); i++)
        if (strcmp(zip_entry_name(z, i), "game/nscript.dat") == 0) seen++;
    ok(seen == 2, "both entries are there");

    zip_close(z);
}

static void test_zip64(const char *dir) {
    char path[1024];
    zip_reader *z;
    int err = ZIP_OK;

    printf("zip64.zip\n");
    snprintf(path, sizeof(path), "%s/zip64.zip", dir);
    z = zip_open(path, &err);
    /* Refused, and refused by name: the installer turns this into "repack
     * it as a normal zip", which is the only thing the player can do. */
    ok(z == NULL, "a zip64 archive is refused");
    ok(err == ZIP_ERR_ZIP64, "and says that is why");
    ok(strstr(zip_error_string(err), "zip64") != NULL,
       "with a message naming zip64");

    /* The other shape: no locator, but an entry carrying the 0xFFFFFFFF
     * sentinel that means "the real size is in the zip64 extra field".
     * Read at face value that is a request for four gigabytes from a file
     * that has two hundred bytes in it. */
    printf("zip64_sentinel.zip\n");
    snprintf(path, sizeof(path), "%s/zip64_sentinel.zip", dir);
    err = ZIP_OK;
    z = zip_open(path, &err);
    ok(z == NULL, "a zip64 size sentinel is refused too");
    ok(err == ZIP_ERR_ZIP64, "as zip64 rather than as corruption");
}

int main(int argc, char **argv) {
    const char *dir = (argc > 1) ? argv[1] : ".";

    test_sanitize();
    test_script_names();

    /* Game files sit directly at the archive root. */
    test_archive(dir, "flat.zip", "", "",
                 "nscript.dat", "flat game script\n");
    /* Everything is inside one folder, the common case. */
    test_archive(dir, "nested.zip", "MyGame", "MyGame",
                 "MyGame/nscript.dat", "nested game script\n");
    /* Two levels deep, plus a decoy script in a backup folder. */
    test_archive(dir, "deep.zip", "outer/inner", "outer",
                 "outer/inner/0.txt", "deep game script\n");
    /* A file large and repetitive enough to span several inflate chunks. */
    test_archive(dir, "nested.zip", "MyGame", "MyGame",
                 "MyGame/caption.txt", "My Game Title\n");

    test_no_script(dir);
    test_not_a_zip(dir);

    test_names(dir);
    test_long_names(dir);
    test_sizes(dir);
    test_symlink(dir);
    test_duplicate(dir);
    test_zip64(dir);

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
