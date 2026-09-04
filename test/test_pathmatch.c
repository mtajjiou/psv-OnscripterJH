/* Host-side tests for opening a file whose name is spelled differently.
 *
 * This is the lookup every file the engine opens goes through, so the
 * things to hold onto are: it still finds what it used to find, at any
 * depth; it does not invent a match; and the listing it now remembers
 * cannot survive a change to the folder -- a save written and then read
 * back under another case has to be found.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "pathmatch.h"

static int failures = 0;
static int checks = 0;

static void check(int cond, const char *what) {
    checks++;
    if (!cond) { failures++; printf("FAIL: %s\n", what); }
}

static void write_file(const char *path, const char *text) {
    FILE *fp = fopen(path, "wb");
    if (fp) { fputs(text, fp); fclose(fp); }
}

/* Opens through the lookup and reports whether it found anything. */
static int found(const char *base, const char *path) {
    FILE *fp = path_open_ci(base, path, "rb");
    if (fp == NULL) return 0;
    fclose(fp);
    return 1;
}

int main(int argc, char **argv) {
    const char *work = argc > 1 ? argv[1] : ".";
    char root[512];
    char base[600], sub[600], path[800];

    snprintf(root, sizeof(root), "%s/pathmatch", work);
    snprintf(base, sizeof(base), "%s/", root);
    snprintf(sub, sizeof(sub), "%s/BG", root);

    mkdir(root, 0777);
    mkdir(sub, 0777);
    snprintf(path, sizeof(path), "%s/Sub Folder", sub);
    mkdir(path, 0777);

    snprintf(path, sizeof(path), "%s/Title.PNG", sub);
    write_file(path, "picture");
    snprintf(path, sizeof(path), "%s/Sub Folder/Deep.TXT", sub);
    write_file(path, "deep");

    check(found(base, "BG/Title.PNG"), "a file found under its own name");
    check(found(base, "bg/title.png"), "and under another case");
    check(found(base, "bg/Sub folder/deep.txt"),
          "every component of a path is matched, not only the last");
    check(found(base, "/bg/title.png"), "a leading separator is stepped over");

    check(!found(base, "bg/missing.png"), "a file that is not there is not found");
    check(!found(base, "nothing/at/all.png"),
          "nor is one in a folder that is not there");
    check(!found(base, "bg/title.png.bak"),
          "and a name that merely starts alike is not a match");

    /* What the remembered listing must not outlive.
     *
     * The file is created under the folder's own spelling: a write only
     * ever lands where the path already resolves, since the last component
     * of a path that is not there yet cannot be matched against anything.
     * That is what this call has always done, and the engine writes saves
     * through its own opener rather than this one. */
    {
        FILE *fp = path_open_ci(base, "BG/Written.txt", "wb");
        check(fp != NULL, "a file can be created through the same call");
        if (fp) { fputs("x", fp); fclose(fp); }

        check(found(base, "bg/written.txt"),
              "and is found afterwards under another case, because writing "
              "forgets what the folder held");
    }

    /* A folder changed by something else needs saying so, and then holds. */
    {
        snprintf(path, sizeof(path), "%s/Outside.txt", sub);
        write_file(path, "made behind its back");

        path_match_forget();
        check(found(base, "bg/outside.txt"),
              "a folder changed elsewhere is found again once forgotten");
    }

    printf("pathmatch: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
