#!/bin/sh
# allprep.sh - Process all the sar files that we have prior to loading
# them into the annexes
#
for dir in `ls -d annexes/*/sar`
do
    fst=
    for i in `ls $dir/sar??`
    do
        dt=`echo $i | sed 's/.*sar//'`
        rm -f sar_*.txt
        nawk -f sarprep.awk $i
        rm -f sar_tty.txt
        rm -f sar_td.txt
        for j in sar_*.txt
        do
            if [ -z "$fst" ]
            then
                sed "2,\$ s/^/$dt-Jul-1998 /" $j >$dir/$j
            else
                sed "1 d
2,\$ s/^/$dt-Jul-1998 /" $j >>$dir/$j
            fi
        done
        fst=1
    done
done
