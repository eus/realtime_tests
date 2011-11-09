#!/bin/bash

# soft replenishment: lt 1 pt 6
# hard replenishment: lt 2 pt 4

every_BWI_no_3=30
every_BWI_no=120
every_BWI_yes_47=30
every_BWI_yes=80
every_BWI_no_hrt=20
every_BWI_yes_hrt=30

for ((i = 1; i <= 8; i++)); do

    if [ $i -eq 3 ]; then
	gnuplot -e '
set key bottom;
set term x11;
set xlabel "t (us)";
set ylabel "P[f - a <= t]";
plot "thesis_evaluation_soft_'$i'_BWI_no.txt" w steps lt 1 lw 2 title "", "" every '$every_BWI_no_3' w points pt 6 lw 2 ps 2 title "", "" every 1e9 w linespoints lt 1 pt 6 ps 2 lw 2 title "Soft replenishment";
set term postscript eps;
set output "thesis_evaluation_'$i'_BWI_no_soft.eps";
replot'
	gnuplot -e '
set key bottom;
set term x11;
set xlabel "t (us)";
set ylabel "P[f - a <= t]";
plot "thesis_evaluation_hard_'$i'_BWI_no.txt" w steps lt 2 lw 2 title "", "" every '$every_BWI_no_3' w points pt 4 lw 2 ps 2 title "", "" every 1e9 w linespoints lt 2 pt 4 ps 2 lw 2 title "Hard replenishment";
set term postscript eps;
set output "thesis_evaluation_'$i'_BWI_no_hard.eps";
replot'
    else
	gnuplot -e '
set key bottom;
set term x11;
set xlabel "t (us)";
set ylabel "P[f - a <= t]";
plot "thesis_evaluation_soft_'$i'_BWI_no.txt" w steps lt 1 lw 2 title "", "" every '$every_BWI_no' w points pt 6 lw 2 ps 2 title "", "" every 1e9 w linespoints lt 1 pt 6 ps 2 lw 2 title "Soft replenishment";
replot "thesis_evaluation_hard_'$i'_BWI_no.txt" w steps lt 2 lw 2 title "", "" every '$every_BWI_no' w points pt 4 lw 2 ps 2 title "", "" every 1e9 w linespoints lt 2 pt 4 ps 2 lw 2 title "Hard replenishment";
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
plot "thesis_evaluation_soft_'$i'_BWI_yes.txt" w steps lt 1 lw 2 title "", "" every '$every_BWI_yes_47' w points pt 6 lw 2 ps 2 title "", "" every 1e9 w linespoints lt 1 pt 6 ps 2 lw 2 title "Soft replenishment";
set term postscript eps;
set output "thesis_evaluation_'$i'_BWI_yes_soft.eps";
replot'
	gnuplot -e '
set key bottom;
set term x11;
set xlabel "t (us)";
set ylabel "P[f - a <= t]";
plot "thesis_evaluation_hard_'$i'_BWI_yes.txt" w steps lt 2 lw 2 title "", "" every '$every_BWI_yes_47' w points pt 4 lw 2 ps 2 title "", "" every 1e9 w linespoints lt 2 pt 4 ps 2 lw 2 title "Hard replenishment";
set term postscript eps;
set output "thesis_evaluation_'$i'_BWI_yes_hard.eps";
replot'
    else
	gnuplot -e '
set key bottom;
set term x11;
set xlabel "t (us)";
set ylabel "P[f - a <= t]";
plot "thesis_evaluation_soft_'$i'_BWI_yes.txt" w steps lt 1 lw 2 title "", "" every '$every_BWI_yes' w points pt 6 lw 2 ps 2 title "", "" every 1e9 w linespoints lt 1 pt 6 ps 2 lw 2 title "Soft replenishment";
replot "thesis_evaluation_hard_'$i'_BWI_yes.txt" w steps lt 2 lw 2 title "", "" every '$every_BWI_yes' w points pt 4 lw 2 ps 2 title "", "" every 1e9 w linespoints lt 2 pt 4 ps 2 lw 2 title "Hard replenishment";
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
plot "thesis_evaluation_soft_'$i'_BWI_no_hrt.txt" w steps lt 1 lw 2 title "", "" every '$every_BWI_no_hrt' w points pt 6 lw 2 ps 2 title "", "" every 1e9 w linespoints lt 1 pt 6 ps 2 lw 2 title "Soft replenishment in experiment without BWI";
replot "thesis_evaluation_hard_'$i'_BWI_no_hrt.txt" w steps lt 2 lw 2 title "", "" every '$every_BWI_no_hrt' w points pt 4 lw 2 ps 2 title "", "" every 1e9 w linespoints lt 2 pt 4 ps 2 lw 2 title "Hard replenishment in experiment without BWI";
set term postscript eps;
set output "thesis_evaluation_'$i'_BWI_no_hrt.eps";
replot'
	gnuplot -e '
set key bottom;
set term x11;
set xlabel "t (us)";
set ylabel "P[f - a <= t]";
plot "thesis_evaluation_soft_'$i'_BWI_yes_hrt.txt" w steps lt 1 lw 2 title "", "" every '$every_BWI_yes_hrt' w points pt 6 lw 2 ps 2 title "", "" every 1e9 w linespoints lt 1 pt 6 ps 2 lw 2 title "Soft replenishment in experiment with BWI";
replot "thesis_evaluation_hard_'$i'_BWI_yes_hrt.txt" w steps lt 2 lw 2 title "", "" every '$every_BWI_yes_hrt' w points pt 4 lw 2 ps 2 title "", "" every 1e9 w linespoints lt 2 pt 4 ps 2 lw 2 title "Hard replenishment in experiment with BWI";
set term postscript eps;
set output "thesis_evaluation_'$i'_BWI_yes_hrt.eps";
replot'
    fi

done
