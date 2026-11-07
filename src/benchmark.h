#ifndef BENCHMARK_H
#define BENCHMARK_H

typedef struct {
    char method[64];
    double time_ms;
    long long sum;
    long long expected_sum;
} BenchmarkResult;

void benchmark_result_set(BenchmarkResult *result, const char *method,
                          double time_ms, long long sum, long long expected_sum);

void benchmark_print(const BenchmarkResult *results, int count);

int benchmark_write_report(const char *path, long long n, int processes,
                           const BenchmarkResult *results, int count);

#endif
