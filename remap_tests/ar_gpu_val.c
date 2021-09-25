#include <stdio.h>
#include <stdlib.h>
#include <cuda_runtime.h>
#include <cuda.h>
#include "mpi.h"

// g_sum is Σ 0..WORLD_SIZE
// snd_buf, each element is == rank*iter
// Allreduce(snd_bff) -> each element in snd_bff = g_sum*iter

int main(int argc, char *argv[])
{
    int rank, size;
    int memsize;
    int g_err = 0;
    float *loc_snd_bff = NULL, *cu_snd_bff = NULL;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    memsize = 1 << 5;

    if (cudaSuccess != cudaSetDevice(rank)){
            printf("ERROR: Rank %d failed to set device: ABORTING\n", rank);
            MPI_Abort(MPI_COMM_WORLD, 420);
    }

    loc_snd_bff = malloc(memsize * sizeof(float));
    cudaMalloc((void*) &cu_snd_bff, memsize*sizeof(float));

    float g_sum = 0;
    for(int i = 0; i<size; i++)
        g_sum += i;

    for (int i = 0; i < size; i++)
    {
        int err = 0;
        for (int j = 0; j < memsize; j++)
        {
            loc_snd_bff[j] = rank*i;
        }
		cudaMemcpy(cu_snd_bff, loc_snd_bff, memsize*sizeof(float), cudaMemcpyHostToDevice);

        MPI_Allreduce(MPI_IN_PLACE, cu_snd_bff, memsize, MPI_FLOAT, MPI_SUM, MPI_COMM_WORLD);

		cudaMemcpy(loc_snd_bff, cu_snd_bff, memsize*sizeof(float), cudaMemcpyDeviceToHost);
        for (int j = 0; j < memsize; j++)
        {
            if (loc_snd_bff[j] != g_sum*i)
                err += 1;
        }

        if (err)
        {
            printf("ERROR: rank:%d snd_buff %d not equal round %d\n", rank, loc_snd_bff[0], i);
            g_err = 69;
        }

        fflush(stdout);
        fflush(stderr);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    free(loc_snd_bff);
    cudaFree(cu_snd_bff);
    MPI_Finalize();
    return g_err;
}
