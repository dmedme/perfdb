#!/bin/sh
awk 'BEGIN { tot = 0 }
/^Total Executions:/ { cpu = $6 + $9
tot = tot + cpu
print cpu " " tot
}
END { print "Total " tot }' fred.exp > allcost.lis
