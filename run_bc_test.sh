#!/cvmfs/soft.computecanada.ca/gentoo/2020/bin/bash

run_omb(){
    ./remap_tests/tst_bc.sh
}

run_single(){
    ./remap_tests/single_bc.sh
}

if [[ "$1" == *"s"* ]];then run_single ; fi
if [[ "$1" == *"o"* ]];then run_omb ; fi
