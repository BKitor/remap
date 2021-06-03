#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"

int main(int argc, char *argv[])
{
    int rank, size;
    int *snd_bff, memsize;
    int g_err = 0;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    memsize = 1 << 5;

    snd_bff = malloc(memsize * sizeof(int));

    for (int i = 0; i < size; i++)
    {
        int err = 0;
        for (int j = 0; j < memsize; j++)
        {
            snd_bff[j] = rank;
        }

        MPI_Bcast(snd_bff, memsize, MPI_INT, i, MPI_COMM_WORLD);

        for (int j = 0; j < memsize; j++)
        {
            if (snd_bff[j] != i)
                err += 1;
        }

        if (err)
        {
            printf("ERROR: rank:%d snd_buff %d not equal round %d\n", rank, snd_bff[0], i);
            g_err = 1;
        }
        // else
        //     printf("SUCCESS: rank: %d, round: %d\n", rank, i);
        fflush(stdout);
        fflush(stderr);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    free(snd_bff);

    MPI_Finalize();

    return g_err;
}