#include "block.h"

void array_block(long long n, int rank, int size, long long *start, long long *count) {
    long long base = n / size;
    long long extra = n % size;
    if ((long long)rank < extra) {
        *count = base + 1;
        *start = (long long)rank * (base + 1);
    } else {
        *count = base;
        *start = extra * (base + 1) + (long long)(rank - (int)extra) * base;
    }
}
