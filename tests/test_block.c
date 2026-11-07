#include "block.h"

#include <stdio.h>
#include <stdlib.h>

static int fail(const char *message) {
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

static int expect_block(long long n, int rank, int size, long long start, long long count) {
    long long got_start = -1;
    long long got_count = -1;
    array_block(n, rank, size, &got_start, &got_count);
    if (got_start != start || got_count != count) {
        fprintf(stderr, "FAIL: n=%lld size=%d rank=%d got [%lld, +%lld), expected [%lld, +%lld)\n",
                n, size, rank, got_start, got_count, start, count);
        return 1;
    }
    return 0;
}

static int cover(long long n, int size) {
    long long *seen = calloc((size_t)n, sizeof(long long));
    if (!seen) {
        return fail("out of memory");
    }
    long long total = 0;
    long long min_count = n;
    long long max_count = 0;
    for (int rank = 0; rank < size; rank++) {
        long long start = 0;
        long long count = 0;
        array_block(n, rank, size, &start, &count);
        if (count < 0 || start < 0 || start + count > n) {
            free(seen);
            fprintf(stderr, "FAIL: rank %d out of range start=%lld count=%lld n=%lld\n",
                    rank, start, count, n);
            return 1;
        }
        for (long long i = 0; i < count; i++) {
            seen[start + i]++;
        }
        total += count;
        if (count < min_count) {
            min_count = count;
        }
        if (count > max_count) {
            max_count = count;
        }
    }
    int bad = 0;
    if (total != n) {
        fprintf(stderr, "FAIL: n=%lld size=%d covered %lld\n", n, size, total);
        bad = 1;
    }
    if (max_count - min_count > 1) {
        fprintf(stderr, "FAIL: n=%lld size=%d counts differ by %lld\n", n, size, max_count - min_count);
        bad = 1;
    }
    for (long long i = 0; i < n; i++) {
        if (seen[i] != 1) {
            fprintf(stderr, "FAIL: n=%lld size=%d index %lld seen %lld times\n", n, size, i, seen[i]);
            bad = 1;
            break;
        }
    }
    free(seen);
    return bad;
}

int main(void) {
    int failed = 0;
    failed += expect_block(10, 0, 3, 0, 4);
    failed += expect_block(10, 1, 3, 4, 3);
    failed += expect_block(10, 2, 3, 7, 3);
    failed += expect_block(5, 0, 8, 0, 1);
    failed += expect_block(5, 4, 8, 4, 1);
    failed += expect_block(5, 5, 8, 5, 0);
    failed += expect_block(1, 0, 1, 0, 1);
    failed += expect_block(1000000000LL, 0, 16, 0, 62500000LL);

    int sizes[] = {1, 2, 3, 7, 8, 16, 64};
    long long lengths[] = {1, 2, 7, 16, 1000, 1000003LL};
    for (int s = 0; s < 7; s++) {
        for (int n = 0; n < 6; n++) {
            failed += cover(lengths[n], sizes[s]);
        }
    }
    if (failed) {
        fprintf(stderr, "%d check(s) failed\n", failed);
        return 1;
    }
    printf("block tests passed\n");
    return 0;
}
