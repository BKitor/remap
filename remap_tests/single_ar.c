#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"

int main(int argc, char* argv[]){

    int rank, size;
    int *snd_bff, *rcv_bff, memsize;

    MPI_Init(&argc, &argv);


    memsize = 1<<18;

    snd_bff = malloc(memsize*sizeof(int));
    rcv_bff = malloc(memsize*sizeof(int));


    MPI_Allreduce(snd_bff, rcv_bff, memsize, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    printf("allreduce called successfully\n");

    free(snd_bff);
    free(rcv_bff);

    MPI_Finalize();

    return 0;
}