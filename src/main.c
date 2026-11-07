#include "array_sum.h"
#include "benchmark.h"

#include <errno.h>
#include <limits.h>
#include <mpi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ELEMENT 1
#define DEFAULT_N 1000000000LL
#define DEFAULT_ITERATIONS 3

typedef long long (*ParallelSum)(const int *full, int *local, long long n, MPI_Comm comm);

static int parse_positive_ll(const char *text, long long *out) {
    char *end = NULL;
    errno = 0;
    long long value = strtoll(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value <= 0) {
        return -1;
    }
    *out = value;
    return 0;
}

static int parse_args(int argc, char **argv, long long *n, int *iterations) {
    if (argc >= 2) {
        long long value = 0;
        if (parse_positive_ll(argv[1], &value) != 0) {
            fprintf(stderr, "usage: %s [n] [iterations]\n", argv[0]);
            return 0;
        }
        *n = value;
    }
    if (argc >= 3) {
        long long value = 0;
        if (parse_positive_ll(argv[2], &value) != 0 || value > 100000) {
            fprintf(stderr, "usage: %s [n] [iterations]\n", argv[0]);
            return 0;
        }
        *iterations = (int)value;
    }
    if (argc > 3) {
        fprintf(stderr, "usage: %s [n] [iterations]\n", argv[0]);
        return 0;
    }
    return 1;
}

static int fail_after_bcast(int rank, int ok, const char *message) {
    MPI_Bcast(&ok, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (ok) {
        return 0;
    }
    if (rank == 0 && message != NULL) {
        fprintf(stderr, "%s\n", message);
    }
    MPI_Finalize();
    return 1;
}

static void measure_best(const char *method, int parallel, ParallelSum fn,
                         const int *array, int *local, long long n, long long expected,
                         int iterations, BenchmarkResult *out) {
    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) {
        printf("running %s...\n", method);
        fflush(stdout);
    }

    double best_ms = 0.0;
    long long best_sum = 0;
    int have_best = 0;

    for (int i = 0; i < iterations; i++) {
        MPI_Barrier(MPI_COMM_WORLD);
        double start = MPI_Wtime();
        long long sum = 0;
        if (parallel) {
            sum = fn(array, local, n, MPI_COMM_WORLD);
        } else if (rank == 0) {
            sum = sum_sequential(array, n);
        }
        double elapsed_ms = (MPI_Wtime() - start) * 1000.0;

        int ok = 1;
        if (rank == 0) {
            if (sum != expected) {
                fprintf(stderr, "%s: got %lld, expected %lld\n", method, sum, expected);
                ok = 0;
            } else if (!have_best || elapsed_ms < best_ms) {
                best_ms = elapsed_ms;
                best_sum = sum;
                have_best = 1;
            }
        }
        MPI_Bcast(&ok, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (!ok) {
            MPI_Abort(MPI_COMM_WORLD, 2);
        }
    }

    if (rank == 0) {
        benchmark_result_set(out, method, best_ms, best_sum, expected);
    }
}

static void warmup(const int *array, int *local, long long n, long long expected) {
    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0) {
        printf("warmup...\n");
        fflush(stdout);
        long long sum = sum_sequential(array, n);
        if (sum != expected) {
            fprintf(stderr, "Sequential warmup: got %lld, expected %lld\n", sum, expected);
            MPI_Abort(MPI_COMM_WORLD, 2);
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);

    long long sendrecv = sum_mpi_sendrecv(array, local, n, MPI_COMM_WORLD);
    long long collective = sum_mpi_collective(array, local, n, MPI_COMM_WORLD);
    if (rank == 0 && (sendrecv != expected || collective != expected)) {
        fprintf(stderr, "MPI warmup: send/recv=%lld collective=%lld expected=%lld\n",
                sendrecv, collective, expected);
        MPI_Abort(MPI_COMM_WORLD, 2);
    }
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank = 0;
    int size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    setvbuf(stdout, NULL, _IOLBF, 0);

    long long n = DEFAULT_N;
    int iterations = DEFAULT_ITERATIONS;
    int args_ok = 1;
    if (rank == 0) {
        args_ok = parse_args(argc, argv, &n, &iterations);
    }
    if (fail_after_bcast(rank, args_ok, NULL)) {
        return 1;
    }
    MPI_Bcast(&n, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    MPI_Bcast(&iterations, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int params_ok = 1;
    const char *params_error = NULL;
    if (rank == 0) {
        if (n > INT_MAX) {
            params_ok = 0;
            params_error = "n must fit into a 32-bit int (MPI Scatterv counts)";
        } else if ((unsigned long long)n > SIZE_MAX / sizeof(int)) {
            params_ok = 0;
            params_error = "n is too large to allocate";
        }
    }
    if (fail_after_bcast(rank, params_ok, params_error)) {
        return 1;
    }

    int *array = NULL;
    int alloc_ok = 1;
    if (rank == 0) {
        printf("n=%lld, processes=%d, element=%d\n", n, size, ELEMENT);
        printf("allocating %.2f GiB on rank 0...\n",
               (double)n * (double)sizeof(int) / (1024.0 * 1024.0 * 1024.0));
        fflush(stdout);
        array = malloc((size_t)n * sizeof(int));
        if (!array) {
            alloc_ok = 0;
        } else {
            for (long long i = 0; i < n; i++) {
                array[i] = ELEMENT;
            }
        }
    }
    if (fail_after_bcast(rank, alloc_ok,
                         "out of memory on rank 0 (the full array must fit there)")) {
        return 1;
    }

    long long start = 0;
    long long count = 0;
    array_block(n, rank, size, &start, &count);
    (void)start;
    int *local = NULL;
    int local_ok = 1;
    if (size > 1 && count > 0) {
        local = malloc((size_t)count * sizeof(int));
        if (!local) {
            local_ok = 0;
        }
    }
    int all_local_ok = 0;
    MPI_Allreduce(&local_ok, &all_local_ok, 1, MPI_INT, MPI_LAND, MPI_COMM_WORLD);
    if (!all_local_ok) {
        if (rank == 0) {
            fprintf(stderr, "out of memory while allocating a local block\n");
        }
        free(array);
        free(local);
        MPI_Finalize();
        return 1;
    }

    const long long expected = n * (long long)ELEMENT;
    warmup(array, local, n, expected);

    BenchmarkResult results[3];
    memset(results, 0, sizeof results);
    measure_best("Sequential", 0, NULL, array, local, n, expected, iterations, &results[0]);
    measure_best("MPI Send/Recv", 1, sum_mpi_sendrecv, array, local, n, expected, iterations, &results[1]);
    measure_best("MPI Scatterv+Reduce", 1, sum_mpi_collective, array, local, n, expected, iterations, &results[2]);

    if (rank == 0) {
        benchmark_print(results, 3);
        if (benchmark_write_report("RESULTS.md", n, size, results, 3) != 0) {
            fprintf(stderr, "failed to write RESULTS.md\n");
            free(array);
            free(local);
            MPI_Finalize();
            return 1;
        }
        printf("wrote RESULTS.md\n");
    }

    free(array);
    free(local);
    MPI_Finalize();
    return 0;
}
