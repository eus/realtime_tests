#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: $0 KERNEL_TRACE_FILE"
    exit 1
fi

SCHED_DL_DATA=/tmp/sched_dl_data.txt
SCHED_DL_MSG='@[0-9]\+'
SCHED_DL_TIMELINE=/tmp/sched_dl_timeline.csv

## Extract the SCHED_DEADLINE trace_printk messages
grep $SCHED_DL_MSG $1 \
    | sed -e 's%.* \([.0-9]\+\):[^@]\+@\([^ ]\+\) \(.*\)%\1\t[\2] \3%' \
    > $SCHED_DL_DATA

## Extract the PIDs of the tasks
task_pids=`sed -e 's%.*\[[^]]\+\] \[\([^]]\+\)\].*%\1%' $SCHED_DL_DATA \
    | sort -u`
task_1_pid=`echo "$task_pids" | sed -n -e '1 p'`
task_2_pid=`echo "$task_pids" | sed -n -e '2 p'`
task_3_pid=`echo "$task_pids" | sed -n -e '3 p'`
if [ x"$task_1_pid"y == xy \
    -o x"$task_2_pid"y == xy \
    -o x"$task_3_pid"y == xy \
    -o x`echo "$task_pids" | sed -n -e '4 p'`y != xy ]; then
    echo "Cannot find exactly three PIDs; try to enlarge tracing buffer" >&2
    exit 1
fi
if [ $((task_1_pid + 1)) -ne $task_2_pid \
    -o $((task_2_pid + 1)) -ne $task_3_pid ]; then
    echo "The three PIDs are not consecutive; try to enlarge tracing buffer" >&2
    exit 1
fi

## Generate the timeline table
./execution_trace-to-execution_trace_processed.sh $SCHED_DL_DATA \
    $task_1_pid $task_2_pid $task_3_pid > $SCHED_DL_TIMELINE
