# vi: set ft=gnuplot :
set title font "Helvetica,20"
set key outside
set terminal postscript color eps enhanced 22

set datafile separator ','

set size 1.3,0.55
set xtics nomirror

set xlabel "Workload Size (ops)"

set style line 1 lc rgb 'black'   lw 4 pt 5
set style line 2 lc rgb 'gray40'  lw 4 pt 6
set style line 3 lc rgb 'gray80'  lw 4 pt 8
set style line 4 lc rgb 'black'   lw 4 pt 10
set style line 5 lc rgb 'gray40'  lw 4 pt 12
set style line 6 lc rgb 'gray80'  lw 4 pt 16
set style line 7 lc rgb 'black'   lw 4 pt 4

set ylabel "Unique execution paths"

set yrange[0:200]

set output "figure-3a.eps"

plot "coverage-all.dat" \
       using (log($1)):2:xtic(1) with linespoints ls 1 t "Btree", \
    '' using (log($1)):3:xtic(1) with linespoints ls 2 t "Rbtree", \
    '' using (log($1)):4:xtic(1) with linespoints ls 3 t "\nHashmap\nAtomic",  


set yrange[0:2500]
set output "figure-3b.eps"

plot "coverage-store.dat" \
       using (log($1)):2:xtic(1) with linespoints ls 1 t "Btree", \
    '' using (log($1)):3:xtic(1) with linespoints ls 2 t "Rbtree", \
    '' using (log($1)):4:xtic(1) with linespoints ls 3 t "\nHashmap\nAtomic",  

