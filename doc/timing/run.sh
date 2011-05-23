#!/bin/bash

set -e

# $1: The name of the file containing timing data
function stdev {
	avg=`awk '{total += $0; count++}; END {printf "%.9f\n", total/count}' "$1"`
	n=`cat "$1" | wc -l`
	awk '{total += ($0 - '$avg')^2}; END {printf "%15s: %.9f +/- %.9f s\n", "'"$1"'", '"$avg"', sqrt(total/('$n' - 1))}' "$1"
}


# $1: GCC optimization level
function run_with_optimization {
	rm -f inc rdtsc inc.txt rdtsc.txt
	make CFLAGS='-Wall -O'$1 LDFLAGS='-lrt' inc
	make CFLAGS='-Wall -O'$1 LDFLAGS='-lrt' rdtsc
	for ((i = 1; i <= 100; i++)); do
		sudo ./inc >> inc.txt
		sudo ./rdtsc >> rdtsc.txt
		echo -n .
	done
	echo ''
	
	stdev inc.txt
	stdev rdtsc.txt
	
	rm inc rdtsc inc.txt rdtsc.txt
}

run_with_optimization 0
run_with_optimization 3
