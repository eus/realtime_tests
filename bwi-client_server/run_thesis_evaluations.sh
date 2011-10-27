#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: `basename $0` {soft|hard}" >&2
    exit 1
fi

name=thesis_evaluation
if [ $1 = "soft" ]; then
    out_name=${name}_soft
elif [ $1 = "hard" ]; then
    out_name=${name}_hard
else
    echo "Usage: `basename $0` {soft|hard}" >&2
    exit 1
fi

set -e

for ((i = 1; i <= 8; i++)); do
    name_bwi_no=${name}_${i}_BWI_no
    name_bwi_yes=${name}_${i}_BWI_yes
    out_name_bwi_no=${out_name}_${i}_BWI_no
    out_name_bwi_yes=${out_name}_${i}_BWI_yes

    echo "Evaluation $i: No BWI"
    sudo ./main -t 60s -p 20 -f ${name_bwi_no}.cfg
    ../read_task_stats_file -c gnuplot ${name_bwi_no}.bin \
	> ./${out_name_bwi_no}.txt
    if [ $i -ne 1 ]; then
	../read_task_stats_file -c gnuplot ${name_bwi_no}_hrt.bin \
	    > ./${out_name_bwi_no}_hrt.txt
    fi

    sleep 5

    echo "Evaluation $i: BWI"
    sudo ./main -t 60s -p 20 -f ${name_bwi_yes}.cfg
    ../read_task_stats_file -c gnuplot ${name_bwi_yes}.bin \
	> ./${out_name_bwi_yes}.txt
    if [ $i -ne 1 ]; then
	../read_task_stats_file -c gnuplot ${name_bwi_yes}_hrt.bin \
	    > ./${out_name_bwi_yes}_hrt.txt
    fi

    sleep 5
done
