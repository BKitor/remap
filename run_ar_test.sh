#!/cvmfs/soft.computecanada.ca/gentoo/2020/bin/bash

run_omb(){
    ./remap_tests/tst_ar.sh
}

run_single(){
    ./remap_tests/single_ar.sh
}

run_val(){
    ./remap_tests/ar_val.sh
}

if [[ "$1" == *"s"* ]];then run_single ; fi
if [[ "$1" == *"o"* ]];then run_omb ; fi
if [[ "$1" == *"v"* ]];then run_val ; fi
