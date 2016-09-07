#!/bin/sh
# allsar.sh - turn a sar file into something human readable
#
if [ $# -lt 1 ]
then
    echo Provide a day to process
    exit
fi
fname=/var/adm/sa/sa$1
if [ ! -f $fname ]
then
    echo there is no sar file for day $1
    exit
fi
for i in u b d y c w a q v t m  p g r k
do
    sar -$i -f $fname
done 
