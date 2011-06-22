#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: $0 SCHED_DEADLINE_TIMELINE_CSV_FILE"
    exit 1
fi

SCHED_DL_TIMELINE=$1

# More keyword should be appended using '\\|' like 'x\\|y'
UNDESIRED_KEYWORD='select_task_rq_dl'

# The algorithm to detect potential outdated rq->clock for each
# timeline differs only in the number of \t character to be detected
# because the \t character signifies whose timeline is which. The
# first sed looks only for "resumes its BW pair" in the timeline that
# belongs to a particular task. If the token falls in another task's
# timeline, the token and its group is removed in one go: sed -n -e
# '{N;s%^[0-9.]\+\t[0-9]\+\t\t\(\t\)\?[^\t]\+%%;t;p}'

# Since an outdated rq->clock is only apparent when the task resuming
# the BW is not run after the decision to resume the BW, the first sed
# s command removes any candidate that runs itself after deciding to
# resume the BW: s%^[0-9.]\+\t[0-9]\+\t[^\t]\+%%

# Since an outdated rq->clock is most apparent when the task resuming
# the BW is delayed by another task running quite long, the second sed
# s command removes any candidate whose intervening task does not run
# long enough:
# s%^[0-9.]\+\t[0-9]\+\t\t\(\t\)\?\('$UNDESIRED_KEYWORD'\)%%

# The remaining candidates should then be inspected manually to decide
# whether the budget, which the task resuming the BW had when the task
# decided to resume its BW, can still be used fully after the
# intervening task picks the task that resumes the BW.

# Detect the timeline of Task 1
grep -A 1 resumes $SCHED_DL_TIMELINE \
    | grep -v ^-- \
    | sed -n -e '{N;s%^[0-9.]\+\t[0-9]\+\t\t\(\t\)\?[^\t]\+%%;t;p}' \
    | sed -n \
    -e '1~2 h' \
    -e '0~2 {
s%^[0-9.]\+\t[0-9]\+\t[^\t]\+%%;
t;
s%^[0-9.]\+\t[0-9]\+\t\t\(\t\)\?\('$UNDESIRED_KEYWORD'\)%%;
t;
H;g;p}'

# Detect the timeline of Task 2
grep -A 1 resumes $SCHED_DL_TIMELINE \
    | grep -v ^-- \
    | sed -n -e '{N;s%^[0-9.]\+\t[0-9]\+\t\(\t\t\)\?[^\t]\+%%;t;p}' \
    | sed -n \
    -e '1~2 h' \
    -e '0~2 {
s%^[0-9.]\+\t[0-9]\+\t\t[^\t]\+%%;
t;
s%^[0-9.]\+\t[0-9]\+\t\(\t\t\)\?\('$UNDESIRED_KEYWORD'\)%%;
t;
H;g;p}'

# Detect the timeline of Task 3
grep -A 1 resumes $SCHED_DL_TIMELINE\
    | grep -v ^-- \
    | sed -n -e '{N;s%^[0-9.]\+\t[0-9]\+\t\(\t\)\?[^\t]\+%%;t;p}' \
    | sed -n \
    -e '1~2 h' \
    -e '0~2 {
s%^[0-9.]\+\t[0-9]\+\t\t\t[^\t]\+%%;
t;
s%^[0-9.]\+\t[0-9]\+\t\(\t\)\?\('$UNDESIRED_KEYWORD'\)%%;
t;
H;g;p}'
