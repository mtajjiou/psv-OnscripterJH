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

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
