# Results
n=1000000000, processes=8

| Method | ms | Speedup |
|--------|----|---------|
| Sequential | 499.40 | 1.00x |
| MPI Send/Recv | 1319.07 | 0.38x |
| MPI Scatterv+Reduce | 1684.21 | 0.30x |

## Conclusions
- MPI Send/Recv: 0.38x (> MPI Scatterv+Reduce)
- MPI Scatterv+Reduce: 0.30x (< MPI Send/Recv)

Fastest: MPI Send/Recv (1319.07 ms).
