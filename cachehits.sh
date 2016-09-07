#!/bin/sh
# cachehits.sh - calculate the cache hit ratio from a given orags file
# Parameters:
# 1 - Input file
# 2 - Output file
ifn=$1
ofn=$2
nawk -F "}" '/}consistent gets}/ { print $1 "}" $NF }' $ifn | sort > cg$$.lis
nawk -F "}" '/}db block gets}/ { print $1 "}" $NF }' $ifn | sort > db$$.lis
nawk -F "}" '/}physical reads}/ { print $1 "}" $NF }' $ifn | sort > pr$$.lis
join -t "}" cg$$.lis db$$.lis > cgdb$$.lis 
join -t "}" cgdb$$.lis pr$$.lis | awk -F "}" '{ print $1 "}" 100*(1 - $4/($2+$3))}' > $ofn
rm -f cg$$.lis db$$.lis pr$$.lis cgdb$$.lis 
