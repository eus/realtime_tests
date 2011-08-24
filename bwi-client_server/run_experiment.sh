#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: `basename $0` SUB-EXPERIMENT_NUMBER"
    exit 1
fi

subexperiment_no=$1

sudo ./main-$subexperiment_no
output_name=`printf 'subexperiment_%02d' $subexperiment_no`
../read_task_stats_file -c matlab ${output_name}.bin > ./${output_name}.m
