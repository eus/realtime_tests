#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: `basename $0` SUB-EXPERIMENT_NUMBER"
    exit 1
fi

subexperiment_no=$1
name=`printf 'subexperiment_%02d' $subexperiment_no`

set -e

sudo ./main -t 60s -p 20 -f ${name}.cfg
../read_task_stats_file -c matlab ${name}.bin > ./${name}.m
