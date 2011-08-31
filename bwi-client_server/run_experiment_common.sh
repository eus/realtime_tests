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
