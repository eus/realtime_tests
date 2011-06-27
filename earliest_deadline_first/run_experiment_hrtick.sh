#!/bin/bash

. run_experiment_common.sh

set -e

# Do the experiment
kwrite_enable_feat HRTICK $SCHED_FEATURES
sudo ./main
kwrite_disable_feat HRTICK $SCHED_FEATURES
