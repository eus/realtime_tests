#!/bin/bash

RAW_DATA=./execution_trace.txt

SCHED_DL_MSG='@[0-9]\+'

BUFSIZE=14080
KTRACE_DIR=/sys/kernel/debug/tracing
KTRACE_TRACING_ENABLED=$KTRACE_DIR/tracing_enabled
KTRACE_TRACING_CPUMASK=$KTRACE_DIR/tracing_cpumask
KTRACE_TRACING_BUFFER_SIZE_KB=$KTRACE_DIR/buffer_size_kb
KTRACE_CURRENT_TRACER=$KTRACE_DIR/current_tracer
KTRACE_TRACE=$KTRACE_DIR/trace
SCHED_FEATURES=/sys/kernel/debug/sched_features

# $1 what to write
# $2 the file to write to
function kwrite {
    echo $1 | sudo tee $2
}

# $1 the file used to disable a single feature
function kwrite_disable {
    kwrite 0 $1
}

# $1 the feature name
# $2 the file used to disable a collection of features
function kwrite_disable_feat {
    kwrite NO_$1 $2
}

# $1 the feature name
# $2 the file used to enable a collection of features
function kwrite_enable_feat {
    kwrite $1 $2
}

set -e

# Preparing the tracer
kwrite_disable $KTRACE_TRACING_ENABLED
kwrite 01 $KTRACE_TRACING_CPUMASK
kwrite $BUFSIZE $KTRACE_TRACING_BUFFER_SIZE_KB
kwrite function $KTRACE_CURRENT_TRACER

# Do the experiment
kwrite_enable_feat HRTICK $SCHED_FEATURES
sudo ./main
kwrite_disable_feat HRTICK $SCHED_FEATURES

# Processing the experiment result
## Take only the relevant part
starting_line=`grep -m 1 -n $SCHED_DL_MSG $KTRACE_TRACE | cut -d: -f 1`
tail -n +$starting_line $KTRACE_TRACE > $RAW_DATA

## Process it
./process_experiment_result.sh $RAW_DATA
