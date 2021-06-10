#!/cvmfs/soft.computecanada.ca/gentoo/2020/bin/bash

which mpicc
if [ $? -ne 0 ]; then
    ehco "ERROR: no mpicc"
    exit
fi

if [ ! -d "$(pwd)/remap_tests" ]; then
    echo "ERROR: can't find remap_tests directory"
    exit
fi

cd remap_tests

# export OMPI_MCA_coll_base_verbose=9
# export OMPI_MCA_coll_remap_net_topo_input_mat="/home/bkitor/cedar_net_remap_mat.txt"
export OMPI_MCA_coll_remap_cc_cluster=0
export OMPI_MCA_coll_remap_turn_off_remap=0

mpicc bcast_val.c -o bcast_val.out
if [ $? -ne 0 ]; then
    exit
fi

# LD_PRELOAD="/cvmfs/soft.computecanada.ca/gentoo/2020/usr/lib64/libibverbs.so.1" mpirun -n 4 single_ar.out

# for i in 0 1; do
for i in 0 1; do
    for NUM_PROC in 4 8 16; do
    export OMPI_MCA_coll_remap_use_scotch=$i

    export OMPI_MCA_coll_remap_select_bcast_alg=5;
    mpirun -n $NUM_PROC bcast_val.out
    echo "bintree exited $? with $NUM_PROC proc and scotch $i"

    export OMPI_MCA_coll_remap_select_bcast_alg=7;
    mpirun -n $NUM_PROC bcast_val.out
    echo "knomial exited $? with $NUM_PROC proc and scotch $i"

    export OMPI_MCA_coll_remap_select_bcast_alg=8;
    mpirun -n $NUM_PROC bcast_val.out
    echo "scag exited $? with $NUM_PROC proc and scotch $i"
    done
done


# export OMPI_MCA_coll_remap_select_bcast_alg=7;
# mpirun -n 4 single_bc.out
#  
# export OMPI_MCA_coll_remap_select_bcast_alg=8;
# mpirun -n 4 single_bc.out

# mpirun -n 4 single_bc.out
