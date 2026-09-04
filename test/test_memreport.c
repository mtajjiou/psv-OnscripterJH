/* Host-side tests for the heap report.
 *
 * The figures come from the allocator and cannot be arranged; what can be
 * checked is that the line says what it means, in units a person reads,
 * and that the high-water mark only ever goes up -- a peak that follows
 * the current figure back down would be worth nothing, since the whole
 * point of it is to survive the free that happened after the crash was
 * avoided.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memreport.h"

static int failures = 0;
static int checks = 0;

static void check(int cond, const char *what) {
    checks++;
    if (!cond) { failures++; printf("FAIL: %s\n", what); }
}

int main(void) {
    char line[256];

    mem_format(line, sizeof(line), "list built", 5 * 1024 * 1024,
               2 * 1024 * 1024, 9 * 1024 * 1024);
    check(strstr(line, "list built") != NULL, "the line says what had happened");
    check(strstr(line, "5120 KB in use") != NULL, "and what is in use, in KB");
    check(strstr(line, "2048 KB free") != NULL, "and what is free in the heap");
    check(strstr(line, "9216 KB at the highest") != NULL,
          "and the high-water mark");
    check(line[strlen(line) - 1] == '\n', "and ends a line");

    /* A buffer too small to hold it truncates rather than runs over. */
    check(mem_format(line, 8, "x", 1, 2, 3) > 0,
          "a short buffer still reports the length it wanted");
    check(strlen(line) < 8, "and writes no more than it was given");

    /* The peak has to survive the memory being given back. */
    {
        size_t before, peak_after_alloc, peak_after_free;
        void *block;

        before = mem_used();
        block = malloc(4 * 1024 * 1024);
        memset(block, 1, 4 * 1024 * 1024);
        check(mem_used() >= before, "allocating shows up as memory in use");
        peak_after_alloc = mem_peak();

        free(block);
        peak_after_free = mem_peak();
        check(peak_after_free >= peak_after_alloc,
              "and freeing it does not take the high-water mark back down");
    }

    printf("memreport: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
