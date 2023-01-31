# vi: set ft=gnuplot :
set title font "Helvetica,20"
set key inside
set key left top Left
set terminal postscript color eps enhanced 22

set datafile separator ','

set size 1.3,0.55
set xtics nomirror

set style line 1 lc rgb 'black' lt 1 lw 1 pt 7

set logscale x 2
set logscale y 2
set xrange [4:256]
set ylabel "Analysis time (h)"
set xlabel "Code size (thousand lines)"
set output "figure-5.eps"
plot "scalability.dat" using ($2/1000):($3/3600) with points ls 1 notitle, \
    '' using ($2/1000):($3/3600+$4*0.4*$3/3600):1 with labels notitle
