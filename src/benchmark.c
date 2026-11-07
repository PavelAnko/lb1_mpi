#include "benchmark.h"

#include <stdio.h>

void benchmark_result_set(BenchmarkResult *result, const char *method,
                          double time_ms, long long sum, long long expected_sum) {
    snprintf(result->method, sizeof result->method, "%s", method);
    result->time_ms = time_ms;
    result->sum = sum;
    result->expected_sum = expected_sum;
}

static double speedup_of(double base_ms, double time_ms) {
    if (time_ms <= 0.0) {
        return 0.0;
    }
    return base_ms / time_ms;
}

void benchmark_print(const BenchmarkResult *results, int count) {
    if (count <= 0) {
        return;
    }
    double base = results[0].time_ms;
    printf("%-22s %10s %8s\n", "Method", "ms", "Speedup");
    for (int i = 0; i < count; i++) {
        double speedup = speedup_of(base, results[i].time_ms);
        if (results[i].time_ms <= 0.0) {
            printf("%-22s %10.2f %8s\n", results[i].method, results[i].time_ms, "inf");
        } else {
            printf("%-22s %10.2f %7.2fx\n", results[i].method, results[i].time_ms, speedup);
        }
    }
    fflush(stdout);
}

static void relation_notes(const BenchmarkResult *mt, int mt_count, int index,
                           char *out, size_t out_len) {
    out[0] = '\0';
    size_t used = 0;
    for (int j = 0; j < mt_count; j++) {
        if (j == index) {
            continue;
        }
        const char *rel = NULL;
        if (mt[j].time_ms > mt[index].time_ms * 1.05) {
            rel = ">";
        } else if (mt[index].time_ms > mt[j].time_ms * 1.05) {
            rel = "<";
        }
        if (!rel) {
            continue;
        }
        int wrote = snprintf(out + used, out_len - used, "%s%s %s",
                             used ? ", " : "", rel, mt[j].method);
        if (wrote < 0 || (size_t)wrote >= out_len - used) {
            break;
        }
        used += (size_t)wrote;
    }
}

int benchmark_write_report(const char *path, long long n, int processes,
                           const BenchmarkResult *results, int count) {
    FILE *file = fopen(path, "w");
    if (!file) {
        return -1;
    }

    double base = (count > 0) ? results[0].time_ms : 1.0;
    fprintf(file, "# Results\n");
    fprintf(file, "n=%lld, processes=%d\n\n", n, processes);
    fprintf(file, "| Method | ms | Speedup |\n");
    fprintf(file, "|--------|----|---------|\n");
    for (int i = 0; i < count; i++) {
        if (results[i].time_ms <= 0.0) {
            fprintf(file, "| %s | %.2f | inf |\n", results[i].method, results[i].time_ms);
        } else {
            fprintf(file, "| %s | %.2f | %.2fx |\n",
                    results[i].method, results[i].time_ms, speedup_of(base, results[i].time_ms));
        }
    }

    fprintf(file, "\n## Conclusions\n");
    int fastest = 1;
    for (int i = 1; i < count; i++) {
        char notes[512];
        relation_notes(results + 1, count - 1, i - 1, notes, sizeof notes);
        double speedup = speedup_of(base, results[i].time_ms);
        if (notes[0] == '\0') {
            fprintf(file, "- %s: %.2fx\n", results[i].method, speedup);
        } else {
            fprintf(file, "- %s: %.2fx (%s)\n", results[i].method, speedup, notes);
        }
        if (results[i].time_ms < results[fastest].time_ms) {
            fastest = i;
        }
    }

    if (count > 1) {
        fprintf(file, "\nFastest: %s (%.2f ms).\n",
                results[fastest].method, results[fastest].time_ms);
    }

    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}
