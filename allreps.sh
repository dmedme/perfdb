#!/bin/sh
# allprep.sh - Process all the sar files that we have prior to loading
# them into the annexes
#
for dir in `ls -d annexes/so071/oracle/fis1`
do
    for i in `ls $dir`
    do
        if [ -f $dir/report.1 ]
        then
            (
               cd $dir
               recordify.sh > orarecs.lis
               grep 'GS}' orarecs.lis > orags.txt
               grep 'TS}' orarecs.lis > orats.txt
               rm orarecs.lis
            )
        fi
    done
done
