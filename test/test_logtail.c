/* Host-side tests for reading the end of a log.
 *
 * A log viewer is read when something has gone wrong, so the cases that
 * matter are the awkward ones: a file bigger than the window, a file with
 * no trailing newline, a line longer than the window, an empty file and one
 * that is not there at all.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logtail.h"

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

static void write_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) return;
    fwrite(text, 1, strlen(text), fp);
    fclose(fp);
}

int main(int argc, char **argv)
{
    const char *dir = (argc > 1) ? argv[1] : ".";
    char path[512];
    char buffer[512];
    char *lines[64];
    int n;

    snprintf(path, sizeof(path), "%s/tail.log", dir);

    /* A file that is not there is not an empty file. */
    check(log_tail("/no/such/file.log", buffer, sizeof(buffer), lines, 64) == -1,
          "a missing file is refused");
    check(log_size("/no/such/file.log") == 0, "and has no size");

    write_file(path, "");
    check(log_tail(path, buffer, sizeof(buffer), lines, 64) == 0,
          "an empty file has no lines");

    write_file(path, "one\ntwo\nthree\n");
    n = log_tail(path, buffer, sizeof(buffer), lines, 64);
    check(n == 3, "three lines are three lines");
    if (n == 3) {
        check(strcmp(lines[0], "one") == 0, "oldest first");
        check(strcmp(lines[2], "three") == 0, "newest last");
        check(strchr(lines[2], '\n') == NULL, "newlines are stripped");
    }
    check(log_size(path) == 14, "the size is the file's size");

    /* No trailing newline: the last line still counts. */
    write_file(path, "alpha\nbeta");
    n = log_tail(path, buffer, sizeof(buffer), lines, 64);
    check(n == 2, "a file that does not end in a newline keeps its last line");
    if (n == 2) check(strcmp(lines[1], "beta") == 0, "and it is intact");

    /* Windows line endings, since a log can be read and re-saved on a PC. */
    write_file(path, "a\r\nb\r\n");
    n = log_tail(path, buffer, sizeof(buffer), lines, 64);
    check(n == 2, "crlf lines are lines");
    if (n == 2) check(strcmp(lines[0], "a") == 0, "with the carriage return gone");

    /* More lines than asked for: the end is what is kept. */
    {
        char many[1024];
        int i;
        many[0] = '\0';
        for (i = 0; i < 40; i++) {
            char line[16];
            snprintf(line, sizeof(line), "line%d\n", i);
            strcat(many, line);
        }
        write_file(path, many);

        n = log_tail(path, buffer, sizeof(buffer), lines, 5);
        check(n == 5, "at most as many lines as asked for");
        if (n == 5) {
            check(strcmp(lines[4], "line39") == 0, "the last line is the last line");
            check(strcmp(lines[0], "line35") == 0, "and the five are the last five");
        }
    }

    /* A file far bigger than the buffer: what comes back is the end of it,
     * and never the fragment the window happened to start in. */
    {
        char *big = (char *)malloc(40000);
        int i;
        big[0] = '\0';
        for (i = 0; i < 2000; i++) {
            char line[24];
            snprintf(line, sizeof(line), "aaaaaaaaaa%d\n", i);
            strcat(big, line);
        }
        write_file(path, big);
        free(big);

        n = log_tail(path, buffer, sizeof(buffer), lines, 64);
        check(n > 0, "a long file still gives lines");
        if (n > 0) {
            check(strcmp(lines[n - 1], "aaaaaaaaaa1999") == 0,
                  "ending with the newest line");
            check(strncmp(lines[0], "aaaaaaaaaa", 10) == 0,
                  "and starting with a whole line, not half of one");
        }
    }

    /* A single line longer than the window is truncated, not lost. */
    {
        char *huge = (char *)malloc(4096);
        memset(huge, 'x', 4000);
        huge[4000] = '\n';
        huge[4001] = '\0';
        write_file(path, huge);
        free(huge);

        n = log_tail(path, buffer, sizeof(buffer), lines, 64);
        check(n >= 1, "a line longer than the buffer still appears");
        if (n >= 1)
            check(strlen(lines[n - 1]) > 0, "with as much of it as fits");
    }

    /* Refusals rather than crashes. */
    check(log_tail(NULL, buffer, sizeof(buffer), lines, 64) == -1,
          "no path is refused");
    check(log_tail(path, NULL, sizeof(buffer), lines, 64) == -1,
          "no buffer is refused");
    check(log_tail(path, buffer, 1, lines, 64) == -1,
          "a buffer with no room is refused");
    check(log_tail(path, buffer, sizeof(buffer), lines, 0) == -1,
          "room for no lines is refused");

    remove(path);
    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
