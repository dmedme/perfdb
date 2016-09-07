#!/bin/sh
awk 'BEGIN { tot = 0 }
/^Total Executions:/ { cpu = $6 *.0008 + $9*.00008
tot = tot + cpu
print cpu " " tot
}
END { print "Total " tot }' badsort.exp > allcost.lis
