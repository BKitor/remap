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
# OMB_AR=$OMB_COLL/osu_allreduce
OMB_BC=$OMB_COLL/osu_bcast

export OMPI_MCA_coll=^hcoll
export OMPI_MCA_btl=^openib

export OMPI_MCA_coll_base_verbose=9
# export OMPI_MCA_coll_remap_net_topo_input_mat="/home/bkitor/cedar_net_remap_mat.txt"
export OMPI_MCA_coll_remap_turn_off_remap=0
export OMPI_MCA_coll_remap_cc_cluster=0

for enable_scotch in 0 1; do
    export OMPI_MCA_coll_use_scotch=$enable_scotch
    for NUM_PROC in 4 8 16; do
        export OMPI_MCA_coll_remap_select_bcast_alg=5
        mpirun -n $NUM_PROC $OMB_BC

        export OMPI_MCA_coll_remap_select_bcast_alg=7
        mpirun -n $NUM_PROC $OMB_BC

        export OMPI_MCA_coll_remap_select_bcast_alg=8
        mpirun -n $NUM_PROC $OMB_BC
    done
done