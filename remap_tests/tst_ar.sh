#!/cvmfs/soft.computecanada.ca/gentoo/2020/bin/bash

cd $HOME/ompi_4.0.5_dev/
source init_env.conf

OMB_MPI=$HOME/osu-micro-benchmarks-5.6.3/build/libexec/osu-micro-benchmarks/mpi/
OMB_COLL=$OMB_MPI/collective
OMB_AR=$OMB_COLL/osu_allreduce

export OMPI_MCA_coll=^hcoll
export OMPI_MCA_btl=^openib

export OMPI_MCA_coll_base_verbose=9

mpirun -n 4 $OMB_AR
