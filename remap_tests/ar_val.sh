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

mpicc ar_val.c -o ar_val.out
if [ $? -ne 0 ]; then
    exit
fi


for i in 1 0; do
    for NUM_PROC in 4 8 16; do
    export OMPI_MCA_coll_remap_use_scotch=$i

    export OMPI_MCA_coll_remap_select_allreduce_alg=3;
    mpirun -n $NUM_PROC ar_val.out
    echo "rdouble exited $? with $NUM_PROC proc and scotch $i" | tee > /dev/stderr 

    export OMPI_MCA_coll_remap_select_allreduce_alg=4;
    mpirun -n $NUM_PROC ar_val.out
    echo "ring exited $? with $NUM_PROC proc and scotch $i" | tee > /dev/stderr 

    export OMPI_MCA_coll_remap_select_allreduce_alg=6;
    mpirun -n $NUM_PROC ar_val.out
    echo "rsa exited $? with $NUM_PROC proc and scotch $i" | tee > /dev/stderr
    done
done

