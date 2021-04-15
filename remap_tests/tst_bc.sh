#!/cvmfs/soft.computecanada.ca/gentoo/2020/bin/bash

cd $HOME/ompi_4.0.5_dev/
source init_env.conf

OMB_MPI=$HOME/osu-micro-benchmarks-5.6.3/build/libexec/osu-micro-benchmarks/mpi/
OMB_COLL=$OMB_MPI/collective
# OMB_AR=$OMB_COLL/osu_allreduce
OMB_BC=$OMB_COLL/osu_bcast

export OMPI_MCA_coll=^hcoll
export OMPI_MCA_btl=^openib

# export OMPI_MCA_coll_base_verbose=9
export OMPI_MCA_coll_remap_net_topo_input_mat="/home/bkitor/cedar_net_remap_mat.txt"

export OMPI_MCA_coll_remap_select_allreduce_alg=$i

export OMPI_MCA_coll_remap_turn_off_remap=0
export OMPI_MCA_coll_remap_cc_cluster=3
# LD_PRELOAD="/cvmfs/soft.computecanada.ca/gentoo/2020/usr/lib64/libibverbs.so.1" \
mpirun -n 4 $OMB_BC

export OMPI_MCA_coll_remap_turn_off_remap=0
export OMPI_MCA_coll_remap_cc_cluster=0
# LD_PRELOAD="/cvmfs/soft.computecanada.ca/gentoo/2020/usr/lib64/libibverbs.so.1" \
mpirun -n 4 $OMB_BC

export OMPI_MCA_coll_remap_turn_off_remap=1
export OMPI_MCA_coll_remap_cc_cluster=0
# LD_PRELOAD="/cvmfs/soft.computecanada.ca/gentoo/2020/usr/lib64/libibverbs.so.1" \
mpirun -n 4 $OMB_BC