#!/bin/sh
# e2monitor.sh - run the SQL monitoring every day.
#
# ************************************************
# Function to ready the results for our standard Microsoft Office macros.
#
. /export/home/dedwards/stellar_server_perf/monscripts/env.sh
office_ise() {
cat /dev/null >excel.txt
#
# Process the sar output
#
#allsar.sh sad> allsar.lis
#day=`nawk 'BEGIN { getline; getline; nf=split($NF, arr, "/");print arr[3] "_" arr[1] "_" arr[2];exit}' allsar.lis`
#export day
#sarprep allsar.lis
#rm sar_td.txt
#echo "SAR_DISK	sar_dsk.txt	dsk_$day.xls" >>excel.txt
#echo "SAR_CPU	sar_cpu.txt	cpu_$day.xls" >>excel.txt
#echo "SAR_MEM	sar_mem.txt	mem_$day.xls" >>excel.txt
#echo "SAR_SCR	sar_scr.txt	scr_$day.xls" >>excel.txt
#echo "SAR_TTY	sar_tty.txt	tty_$day.xls" >>excel.txt
#echo "SAR_RUNQ	sar_runq.txt	runq_$day.xls" >>excel.txt
#
# Process the utlbstat/utlestat output
#
if recordify.sh
then
    mv report.7 sav_report.7
#    rm -f report.*
    echo "ORAGS	orags.txt	orags_$day.xls" >>excel.txt
    echo "ORATS	orats.txt	orats_$day.xls" >>excel.txt
    cachehits.sh orags.txt cachehits.txt
    echo "CACHEHITS	cachehits.txt	cache_$day.xls" >>excel.txt
fi
start_time=`cat start_time`
##
## Process Accounting
##
#if [ -f ps_beg -a -f ps_end -a -f pacct.txt ]
#then
#    sarprep -p ps_beg ps_end pacct.txt
#    echo "RESOUT	resout.txt	resout_$day.xls" >>excel.txt
#fi
##
## Produce the vmstat output
##
#if [ -f vm.out ]
#then
#    sarprep -t 1200 -s $start_time -v vm.out
#    echo "IOCPU	vmout.txt	vmcpu_$day.xls" >>excel.txt
#    echo "SAR_SCR	vmscr.txt	vmrunq_$day.xls" >>excel.txt
#fi
return
}
# Set up the execution environment
#
ulimit unlimited
#
# Find out if we are already running. Exit if we are.
#
if ps -ef | grep '[b]adsort'
then
    ps -ef | nawk '/[b]adsort/ { print $2}' | xargs kill -USR1
fi
if ps -ef | grep '[m]ultista'
then
    ps -ef | nawk '/[m]ultista/ { print $2}' | xargs kill -15
fi
#if ps -ef | grep '[l]ockche'
#then
#    ps -ef | nawk '/[l]ockch/ { print $2}' | xargs kill -9
#fi
if [ "$1" = stop ]
then
    exit
fi
#
# Keep the last 30 days of stuff
#
cd $E2_HOME/monscripts/perfdb
nxt=`cat nxtid.dat`
if [ -z "$nxt" ]
then
    lst=0
    nxt=1
else
    lst=$nxt
    nxt=`expr $nxt + 1`
    if [ "$nxt" -gt 30 ]
    then
	nxt=1
    fi
fi
#
# Loop - Monitor each Database instance
#
echo $nxt > nxtid.dat
for ORACLE_SID in STPROD1
do
export ORACLE_SID
#
# Set up a new directory for today's files
#
if [ -d $ORACLE_SID.$nxt ]
then
    rm -rf $ORACLE_SID.$nxt
fi
#badsort -A $ORACLE_SID.$nxt
mkdir $ORACLE_SID.$nxt
#
# Store the incremented name
#
cd $ORACLE_SID.$nxt
tosecs >start_time
#
# Update the search terms
#
#badsort -B $OPWD
#
# Start the bstat/estat logging
#
multistat.sh >/dev/null 2>&1 &
#
# Start the SQL Monitors
#
badsort -c .00005 -r 8 -q $OPWD >/dev/null 2>&1&
#
# Start the UNIX Monitors
#
#if [ $ORACLE_SID = STPROD1 ]
#then
#/usr/lib/sa/sadc 1200 36 sad &
#vmstat 1200 144 >vm.out &
#fi
#
# Pre-process the data from the run before
#
if [ -d ../$ORACLE_SID.$lst ]
then
    cd ../$ORACLE_SID.$lst
    office_ise
fi
cd ..
done
tar cf oracle_summary_${lst}_$day.tar *.$lst/badsort.exp *.$lst/*.txt *.$lst/sav_report.7
bzip2 oracle_summary_${lst}_$day.tar
mv oracle_summary_${lst}_$day.tar.bz2 ..
exit
