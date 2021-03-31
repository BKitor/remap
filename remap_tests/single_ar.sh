#!/cvmfs/soft.computecanada.ca/gentoo/2020/bin/bash

cd $HOME/ompi_4.0.5_dev/
source init_env.conf

cd build/libexec/bkitor

export OMPI_MCA_coll_base_verbose=9

mpicc single_ar.c -o single_ar.out
mpirun -n 2 single_ar.out