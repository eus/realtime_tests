#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: `basename $0` SUB-EXPERIMENT_NUMBER"
    exit 1
fi

subexperiment_no=$1

sudo ./main-$subexperiment_no
output_name=`printf 'subexperiment_%02d' $subexperiment_no`
../read_task_stats_file ${output_name}.bin | tail -n 1 > ./${output_name}.m