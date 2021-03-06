#!/bin/bash

SCHED_FEATURES=/sys/kernel/debug/sched_features

# $1 what to write
# $2 the file to write to
function kwrite {
    echo $1 | sudo tee $2
}

# $1 what to write
# $2 the file to write to
function kwrite_append {
    echo $1 | sudo tee -a $2
}

# $1 the file used to disable a single feature
function kwrite_disable {
    kwrite 0 $1
}

# $1 the file used to enable a single feature
function kwrite_enable {
    kwrite 1 $1
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

function prepare_ftrace {
    BUFSIZE=28160
    FTRACE_DIR=/sys/kernel/debug/tracing
    FTRACE_TRACING_ENABLED=$FTRACE_DIR/tracing_enabled
    FTRACE_TRACING_ON=$FTRACE_DIR/tracing_on
    FTRACE_TRACING_CPUMASK=$FTRACE_DIR/tracing_cpumask
    FTRACE_TRACING_BUFFER_SIZE_KB=$FTRACE_DIR/buffer_size_kb
    FTRACE_CURRENT_TRACER=$FTRACE_DIR/current_tracer
    FTRACE_TRACE=$FTRACE_DIR/trace
    FTRACE_EVENT=$FTRACE_DIR/set_event

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
}