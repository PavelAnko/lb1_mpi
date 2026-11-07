#include "array_sum.h"

#include <stdio.h>
#include <stdlib.h>

long long sum_sequential(const int *array, long long n) {
    long long sum = 0;
    for (long long i = 0; i < n; i++) {
        sum += array[i];
    }
    return sum;
}

long long sum_mpi_sendrecv(const int *full, int *local, long long n, MPI_Comm comm) {
    int rank = 0;
    int size = 0;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    if (size == 1) {
        return (rank == 0) ? sum_sequential(full, n) : 0;
    }

    long long start = 0;
    long long count = 0;
    array_block(n, rank, size, &start, &count);

    if (rank == 0) {
        for (int r = 1; r < size; r++) {
            long long remote_start = 0;
            long long remote_count = 0;
            array_block(n, r, size, &remote_start, &remote_count);
            if (remote_count > 0) {
                MPI_Send(full + remote_start, (int)remote_count, MPI_INT, r, 1, comm);
            }
        }
    } else if (count > 0) {
        MPI_Recv(local, (int)count, MPI_INT, 0, 1, comm, MPI_STATUS_IGNORE);
    }

    long long partial = (rank == 0) ? sum_sequential(full + start, count)
                                    : sum_sequential(local, count);

    if (rank == 0) {
        long long total = partial;
        for (int r = 1; r < size; r++) {
            long long remote = 0;
            MPI_Recv(&remote, 1, MPI_LONG_LONG, r, 2, comm, MPI_STATUS_IGNORE);
            total += remote;
        }
        return total;
    }

    MPI_Send(&partial, 1, MPI_LONG_LONG, 0, 2, comm);
    return 0;
}

long long sum_mpi_collective(const int *full, int *local, long long n, MPI_Comm comm) {
    int rank = 0;
    int size = 0;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    if (size == 1) {
        return (rank == 0) ? sum_sequential(full, n) : 0;
    }

    long long start = 0;
    long long count = 0;
    array_block(n, rank, size, &start, &count);
    (void)start;

    int *counts = malloc((size_t)size * sizeof(int));
    int *displs = malloc((size_t)size * sizeof(int));
    int counts_ok = counts != NULL && displs != NULL;
    int all_counts_ok = 0;
    MPI_Allreduce(&counts_ok, &all_counts_ok, 1, MPI_INT, MPI_LAND, comm);
    if (!all_counts_ok) {
        free(counts);
        free(displs);
        fprintf(stderr, "rank %d: out of memory\n", rank);
        MPI_Abort(comm, 1);
        return 0;
    }
    for (int r = 0; r < size; r++) {
        long long remote_start = 0;
        long long remote_count = 0;
        array_block(n, r, size, &remote_start, &remote_count);
        counts[r] = (int)remote_count;
        displs[r] = (int)remote_start;
    }

    int send_dummy = 0;
    int recv_dummy = 0;
    const void *sendbuf = (rank == 0) ? (const void *)full : (const void *)&send_dummy;
    int *recv = (count > 0) ? local : &recv_dummy;
    MPI_Scatterv(sendbuf, counts, displs, MPI_INT, recv, (int)count, MPI_INT, 0, comm);

    long long partial = sum_sequential(recv, count);
    free(counts);
    free(displs);

    long long total = 0;
    MPI_Reduce(&partial, &total, 1, MPI_LONG_LONG, MPI_SUM, 0, comm);
    return total;
}
