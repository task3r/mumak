# vi: set ft=gnuplot :
load 'histogram.cfg'

set output "figure-4b.eps"

plot "pmdk-1dot8.dat" \
    using 2:xtic(1) title col ls 1, '' using 3 title col ls 2, \
    '' using 4 title col ls 3, '' using 5 title col ls 4
