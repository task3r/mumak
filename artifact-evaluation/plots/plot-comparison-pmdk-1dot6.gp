# vi: set ft=gnuplot :
load 'histogram.cfg'

set output "figure-4a.eps"

plot "pmdk-1dot6.dat" \
    using 2:xtic(1) title col ls 1, '' using 3 title col ls 2, \
    '' using 4 title col ls 3, '' using 5 title col ls 4
