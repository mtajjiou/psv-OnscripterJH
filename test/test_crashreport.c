/* Host-side tests for the crash report.
 *
 * The report exists for the run that went wrong, so what is checked here is
 * that it survives the shapes going wrong takes: a run that never gets to
 * write anything, a fault the engine catches, a clean exit that must leave
 * nothing behind, and a marker left by a run that died.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crashreport.h"

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

static char *slurp(const char *path)
{
    FILE *fp = fopen(path, "rb");
    char *buf;
    long len;

    if (fp == NULL) return NULL;
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) { fclose(fp); return NULL; }
    if (len > 0 && fread(buf, 1, (size_t)len, fp) != (size_t)len) {
        free(buf); fclose(fp); return NULL;
    }
    buf[len] = '\0';
    fclose(fp);
    return buf;
}

static int exists(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return 0;
    fclose(fp);
    return 1;
}

int main(int argc, char **argv)
{
    const char *dir = (argc > 1) ? argv[1] : ".";
    char marker[512], report[512];
    char *text;

    snprintf(marker, sizeof(marker), "%s/session.txt", dir);
    snprintf(report, sizeof(report), "%s/crash.txt", dir);
    remove(marker);
    remove(report);

    check(crash_previous_was_unclean(marker) == 0,
          "no marker means the last run finished");
    check(crash_previous_was_unclean(NULL) == 0, "no path is not a crash");

    /* A run that starts leaves a marker with what it knows. */
    crash_begin(marker, report, "01.72 abcdef0", "ux0:/onsemu/Tsukihime");
    check(exists(marker), "starting a run leaves a marker");
    text = slurp(marker);
    check(text != NULL, "the marker can be read");
    if (text) {
        check(strstr(text, "game: ux0:/onsemu/Tsukihime") != NULL,
              "and says which game");
        check(strstr(text, "build: 01.72 abcdef0") != NULL, "and which build");
        free(text);
    }

    /* Position: a new label is worth writing down, a new line is not. */
    crash_set_position("*prologue", 120);
    text = slurp(marker);
    check(text && strstr(text, "label: *prologue") != NULL,
          "a label change reaches the marker");
    free(text);

    crash_set_position("*prologue", 121);
    text = slurp(marker);
    check(text && strstr(text, "line: 120") != NULL,
          "a line change alone does not rewrite it");
    free(text);

    crash_set_position("*chapter2", 5);
    text = slurp(marker);
    check(text && strstr(text, "label: *chapter2") != NULL,
          "the next label does");
    check(text && strstr(text, "line: 5") != NULL, "with the line it was on");
    free(text);

    /* A fault the engine catches writes the report itself. */
    crash_set_file("cg/ev01.jpg");
    crash_write("parse error");
    check(exists(report), "a caught fault writes a report");
    check(!exists(marker), "and clears the marker, which is now redundant");
    text = slurp(report);
    if (text) {
        check(strstr(text, "reason: parse error") != NULL, "with the reason");
        check(strstr(text, "label: *chapter2") != NULL, "where it was");
        check(strstr(text, "last file: cg/ev01.jpg") != NULL,
              "and what it last opened");
        free(text);
    }
    remove(report);

    /* A clean exit leaves nothing behind. */
    crash_begin(marker, report, "build", "game");
    crash_end();
    check(!exists(marker), "a clean exit removes the marker");
    check(crash_previous_was_unclean(marker) == 0, "and reads as a clean run");

    /* A run that died leaves its marker, which becomes the report. */
    crash_begin(marker, report, "01.72 abcdef0", "ux0:/onsemu/Tsukihime");
    crash_set_position("*ending", 900);
    /* -- process dies here; nothing else runs -- */

    check(crash_previous_was_unclean(marker) == 1,
          "the marker says the run did not finish");
    check(crash_promote_marker(marker, report) == 1, "and becomes a report");
    check(!exists(marker), "the marker is consumed");
    text = slurp(report);
    if (text) {
        check(strstr(text, "did not exit cleanly") != NULL,
              "the report says what happened");
        check(strstr(text, "label: *ending") != NULL,
              "and where it had got to");
        check(strstr(text, "the game was still running") == NULL,
              "without the marker's own wording left in it");
        free(text);
    }
    check(crash_promote_marker(marker, report) == 0,
          "with nothing to promote a second time");

    /* Turned off entirely: no paths, no files, no crash. */
    crash_begin(NULL, NULL, "build", "game");
    crash_set_position("*label", 1);
    crash_set_file("file");
    crash_write("reason");
    crash_end();
    check(1, "a session with nowhere to write is harmless");

    remove(marker);
    remove(report);
    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
