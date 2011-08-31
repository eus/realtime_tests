#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: `basename $0` SUB-EXPERIMENT_NUMBER"
    exit 1
fi

subexperiment_no=$1
name=`printf 'subexperiment_%02d' $subexperiment_no`

BUFSIZE=28160
FTRACE_DIR=/sys/kernel/debug/tracing
FTRACE_TRACING_ENABLED=$FTRACE_DIR/tracing_enabled
FTRACE_TRACING_ON=$FTRACE_DIR/tracing_on
FTRACE_TRACING_CPUMASK=$FTRACE_DIR/tracing_cpumask
FTRACE_TRACING_BUFFER_SIZE_KB=$FTRACE_DIR/buffer_size_kb
FTRACE_CURRENT_TRACER=$FTRACE_DIR/current_tracer
FTRACE_TRACE=$FTRACE_DIR/trace
FTRACE_EVENT=$FTRACE_DIR/set_event

. run_experiment_common.sh

set -e

# Preparing the tracer
kwrite_disable $FTRACE_TRACING_ON
kwrite_enable $FTRACE_TRACING_ENABLED
kwrite 01 $FTRACE_TRACING_CPUMASK
kwrite $BUFSIZE $FTRACE_TRACING_BUFFER_SIZE_KB
kwrite function $FTRACE_CURRENT_TRACER
kwrite "" $FTRACE_TRACE

kwrite 'sched:*' $FTRACE_EVENT
kwrite_append 'timer:*' $FTRACE_EVENT
kwrite_append 'irq:*' $FTRACE_EVENT
kwrite_append 'syscalls:sys_enter_bwi_give_server' $FTRACE_EVENT
kwrite_append 'syscalls:sys_exit_bwi_give_server' $FTRACE_EVENT
kwrite_append 'syscalls:sys_enter_bwi_take_back_server' $FTRACE_EVENT
kwrite_append 'syscalls:sys_exit_bwi_take_back_server' $FTRACE_EVENT

# Run the experiment
sudo ./main -t 60s -p 20 -f ${name}_ftraced.cfg

# Extract the result
../read_task_stats_file -c matlab ${name}.bin > ./${name}.m
