#!/cvmfs/soft.computecanada.ca/gentoo/2020/bin/bash

which mpirun
if [ $? -ne 0 ]; then
    ehco "ERROR: no mpirun"
    exit
fi

if [ -z "$OMB_DIR" ]; then
    echo "ERROR: OMB_DIR not set"
fi
echo $OMB_DIR

OMB_COLL=$OMB_DIR/collective
OMB_AR=$OMB_COLL/osu_allreduce

export OMPI_MCA_coll=^hcoll
export OMPI_MCA_btl=^openib

# export OMPI_MCA_coll_base_verbose=9
# export OMPI_MCA_coll_remap_net_topo_input_mat="/home/bkitor/cedar_net_remap_mat.txt"

export OMPI_MCA_coll_remap_turn_off_remap=0
export OMPI_MCA_coll_remap_cc_cluster=0

export OMPI_MCA_coll_remap_select_allreduce_alg=3
# LD_PRELOAD="/cvmfs/soft.computecanada.ca/gentoo/2020/usr/lib64/libibverbs.so.1" 
mpirun -n 4 $OMB_AR

export OMPI_MCA_coll_remap_select_allreduce_alg=4
# LD_PRELOAD="/cvmfs/soft.computecanada.ca/gentoo/2020/usr/lib64/libibverbs.so.1" 
mpirun -n 4 $OMB_AR

export OMPI_MCA_coll_remap_select_allreduce_alg=6
# LD_PRELOAD="/cvmfs/soft.computecanada.ca/gentoo/2020/usr/lib64/libibverbs.so.1" 
mpirun -n 4 $OMB_AR