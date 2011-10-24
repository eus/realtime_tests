#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: `basename $0` SUB-EXPERIMENT_NUMBER"
    exit 1
fi

subexperiment_no=$1
name=`printf 'subexperiment_%02d' $subexperiment_no`

. run_experiment_common.sh

set -e

prepare_ftrace

# Run the experiment
sudo ./main -t 60s -p 20 -f ${name}_ftraced.cfg

# Extract the result
../read_task_stats_file -c matlab ${name}.bin > ./${name}.m
