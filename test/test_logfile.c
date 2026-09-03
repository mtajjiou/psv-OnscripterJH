/* Host-side tests for the log file.
 *
 * A log exists to be read after something went wrong, so what is checked
 * here is that it survives the ways that go wrong: a log that cannot be
 * opened must not take the program down with it, the run before the bad one
 * must still be there, and a game left running must not fill the card.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logfile.h"

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

/* Whole contents of a file, or NULL. Caller frees. */
static char *slurp(const char *path, long *len_out)
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
    if (len_out) *len_out = len;
    return buf;
}

int main(int argc, char **argv)
{
    const char *dir = (argc > 1) ? argv[1] : ".";
    char path[512], previous[520];
    char *text;
    long len = 0;

    snprintf(path, sizeof(path), "%s/onsjh.log", dir);
    snprintf(previous, sizeof(previous), "%s.1", path);
    remove(path);
    remove(previous);

    /* Nothing written before a log is open, and no crash for trying. */
    check(log_is_open() == 0, "no log is open to start with");
    log_write("dropped\n");
    log_printf("dropped %d\n", 1);
    log_close();                       /* closing a closed log is fine */
    check(log_is_open() == 0, "closing what was never opened is harmless");

    check(log_open(path) == 1, "a log opens");
    check(log_is_open() == 1, "and says so");
    log_write("first line\n");
    log_printf("value %d and %s\n", 42, "text");
    log_close();

    text = slurp(path, &len);
    check(text != NULL, "the log is on disk");
    if (text) {
        check(strstr(text, "first line\n") != NULL, "what was written is in it");
        check(strstr(text, "value 42 and text\n") != NULL, "printf-style too");
        check(strstr(text, "dropped") == NULL,
              "what was written before it opened is not");
        free(text);
    }

    /* The run before the bad one is what someone actually needs. */
    check(log_open(path) == 1, "opening again works");
    log_write("second run\n");
    log_close();

    text = slurp(previous, &len);
    check(text != NULL, "the previous log was kept");
    if (text) {
        check(strstr(text, "first line\n") != NULL,
              "and it is the previous run, not the new one");
        free(text);
    }
    text = slurp(path, &len);
    check(text != NULL, "the new log is there too");
    if (text) {
        check(strstr(text, "second run\n") != NULL, "with the new run in it");
        check(strstr(text, "first line") == NULL,
              "and not the old one as well");
        free(text);
    }

    /* A log that cannot be opened leaves everything else working. */
    check(log_open("/no/such/directory/onsjh.log") == 0,
          "an unwritable path is refused");
    check(log_is_open() == 0, "and leaves no log open");
    log_write("nowhere\n");
    log_printf("nowhere %d\n", 2);
    check(log_open(NULL) == 0, "no path at all is refused");
    check(log_open("") == 0, "an empty path is refused");

    /* A long run rotates rather than growing forever. */
    check(log_open(path) == 1, "a log for the long run");
    {
        /* Just past the cap, in lines a real log could contain. */
        char line[256];
        long written = 0;
        int i;
        memset(line, 'x', sizeof(line) - 2);
        line[sizeof(line) - 2] = '\n';
        line[sizeof(line) - 1] = '\0';
        for (i = 0; written < LOG_MAX_BYTES + 4096; i++) {
            log_write(line);
            written += (long)strlen(line);
        }
    }
    text = slurp(path, &len);
    check(text != NULL, "there is still a log at the same path");
    check(len < LOG_MAX_BYTES, "and it was started again rather than grown");
    free(text);
    log_close();

    remove(path);
    remove(previous);

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
