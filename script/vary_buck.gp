#!/bin/gnuplot
reset

#set terminal postscript eps color enhanced size 5,3.5 font "Times-Roman" 22
#set output 'vary_buck.eps'

set term png

set output "/mnt/public/fwu/vary_buck.png"

set boxwidth 0.5
set style fill solid

set multiplot layout 3,1

set yrange[0:*]
set ylabel 'Space Overhead %'
plot "vary_buck.csv" using (($8/283644 - 1) * 100):xtic(3) notitle with boxes

set yrange[0:*]
set ylabel 'CPU %'
plot "vary_buck.csv" using 7:xtic(3) notitle with boxes

set yrange[0:*]
set ylabel 'Thoughput (MB/s)'
plot "vary_buck.csv" using 7:xtic(3) notitle with boxes
