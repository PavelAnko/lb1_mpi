#ifndef ARRAY_SUM_H
#define ARRAY_SUM_H

#include "block.h"
#include <mpi.h>

long long sum_sequential(const int *array, long long n);

long long sum_mpi_sendrecv(const int *full, int *local, long long n, MPI_Comm comm);
long long sum_mpi_collective(const int *full, int *local, long long n, MPI_Comm comm);

#endif
