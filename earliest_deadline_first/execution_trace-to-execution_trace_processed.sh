#!/bin/bash

if [ $# -ne 4 ]; then
    cat <<EOF
Usage: `basename $0` EXECUTION_TRACE_FILE TASK_1_PID TASK_2_PID TASK_3_PID

The output is in the format of a .csv file with TAB as the column delimiter.
EOF
    exit 1
fi

EXECUTION_TRACE_FILE="$1"
shift

for ((i = 1; i <= 3; i++)); do
    declare TASK_${i}_PID="$1"
    shift
done

sed -e 's%^\[[^0-9]*\([^]]\+\)\] \['"$TASK_1_PID"'\] \(.*\)%\1'\\t'\2%' \
    -e 's%^\[[^0-9]*\([^]]\+\)\] \['"$TASK_2_PID"'\] \(.*\)%\1'\\t\\t'\2%' \
    -e 's%^\[[^0-9]*\([^]]\+\)\] \['"$TASK_3_PID"'\] \(.*\)%\1'\\t\\t\\t'\2%' \
    -e 's% main ('"$TASK_1_PID"')% Task 1%' \
    -e 's% main ('"$TASK_2_PID"')% Task 2%' \
    -e 's% main ('"$TASK_3_PID"')% Task 3%' \
    "$EXECUTION_TRACE_FILE"