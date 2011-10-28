#!/bin/bash

for ((i = 1; i <= 8; i++)); do

    if [ $i -eq 3 ]; then
	gnuplot -e '
set key bottom;
set term x11;
set xlabel "t (us)";
set ylabel "P[f - a <= t]";
plot "thesis_evaluation_soft_'$i'_BWI_no.txt" w steps lw 3 title "Soft replenishment";
set term postscript eps;
set output "thesis_evaluation_'$i'_BWI_no_soft.eps";
replot'
	gnuplot -e '
set key bottom;
set term x11;
set xlabel "t (us)";
set ylabel "P[f - a <= t]";
plot "thesis_evaluation_hard_'$i'_BWI_no.txt" w steps lw 3 title "Hard replenishment";
set term postscript eps;
set output "thesis_evaluation_'$i'_BWI_no_hard.eps";
replot'
    else
	gnuplot -e '
set key bottom;
set term x11;
set xlabel "t (us)";
set ylabel "P[f - a <= t]";
plot "thesis_evaluation_soft_'$i'_BWI_no.txt" w steps lw 3 title "Soft replenishment";
replot "thesis_evaluation_hard_'$i'_BWI_no.txt" w steps lw 3 title "Hard replenishment";
set term postscript eps;
set output "thesis_evaluation_'$i'_BWI_no.eps";
replot'
    fi

    if [ $i -eq 4 -o $i -eq 7 ]; then
	gnuplot -e '
set key bottom;
set term x11;
set xlabel "t (us)";
set ylabel "P[f - a <= t]";
plot "thesis_evaluation_soft_'$i'_BWI_yes.txt" w steps lw 3 title "Soft replenishment";
set term postscript eps;
set output "thesis_evaluation_'$i'_BWI_yes_soft.eps";
replot'
	gnuplot -e '
set key bottom;
set term x11;
set xlabel "t (us)";
set ylabel "P[f - a <= t]";
plot "thesis_evaluation_hard_'$i'_BWI_yes.txt" w steps lw 3 title "Hard replenishment";
set term postscript eps;
set output "thesis_evaluation_'$i'_BWI_yes_hard.eps";
replot'
    else
	gnuplot -e '
set key bottom;
set term x11;
set xlabel "t (us)";
set ylabel "P[f - a <= t]";
plot "thesis_evaluation_soft_'$i'_BWI_yes.txt" w steps lw 3 title "Soft replenishment";
replot "thesis_evaluation_hard_'$i'_BWI_yes.txt" w steps lw 3 title "Hard replenishment";
set term postscript eps;
set output "thesis_evaluation_'$i'_BWI_yes.eps";
replot'
    fi

    if [ $i -ne 1 ]; then
	gnuplot -e '
set key bottom;
set term x11;
set xlabel "t (us)";
set ylabel "P[f - a <= t]";
plot "thesis_evaluation_soft_'$i'_BWI_no_hrt.txt" w steps lw 3 title "Soft replenishment in experiment without BWI";
replot "thesis_evaluation_hard_'$i'_BWI_no_hrt.txt" w steps lw 3 title "Hard replenishment in experiment without BWI";
replot "thesis_evaluation_soft_'$i'_BWI_yes_hrt.txt" w linespoints lt 1 pt 2 lw 1 title "Soft replenishment in experiment with BWI";
replot "thesis_evaluation_hard_'$i'_BWI_yes_hrt.txt" w linespoints lt 1 pt 6 lw 1 title "Hard replenishment in experiment with BWI";
set term postscript eps;
set output "thesis_evaluation_'$i'_hrt.eps";
replot'
    fi

done
