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

export OMPI_MCA_coll_base_verbose=9
# export OMPI_MCA_coll_remap_net_topo_input_mat="/home/bkitor/cedar_net_remap_mat.txt"
export OMPI_MCA_coll_remap_cc_cluster=0
export OMPI_MCA_coll_remap_use_scotch=1

mpicc single_bc.c -o single_bc.out
# LD_PRELOAD="/cvmfs/soft.computecanada.ca/gentoo/2020/usr/lib64/libibverbs.so.1" mpirun -n 4 single_ar.out
export OMPI_MCA_coll_remap_select_bcast_alg=7;
mpirun -n 8 single_bc.out

# export OMPI_MCA_coll_remap_select_bcast_alg=7;
# mpirun -n 4 single_bc.out
#  
# export OMPI_MCA_coll_remap_select_bcast_alg=8;
# mpirun -n 4 single_bc.out

# export OMPI_MCA_coll_remap_turn_off_remap=1
# mpirun -n 4 single_bc.out
