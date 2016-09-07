#!/bin/sh
# e2monitor.sh - run the SQL monitoring every day.
#
# ************************************************
# Function to ready the results for our standard Microsoft Office macros.
#
office_ise() {
cat /dev/null >excel.txt
#
# Process the sar output
#
allsar.sh sad> allsar.lis
day=`awk 'BEGIN { getline; getline; nf=split($NF, arr, "/");print arr[3] "_" arr[1] "_" arr[2];exit}' allsar.lis`
sarprep allsar.lis
rm sar_td.txt
echo "SAR_DISK	sar_dsk.txt	dsk_$day.xls" >>excel.txt
echo "SAR_CPU	sar_cpu.txt	cpu_$day.xls" >>excel.txt
echo "SAR_MEM	sar_mem.txt	mem_$day.xls" >>excel.txt
echo "SAR_SCR	sar_scr.txt	scr_$day.xls" >>excel.txt
echo "SAR_TTY	sar_tty.txt	tty_$day.xls" >>excel.txt
echo "SAR_TTY	sar_runq.txt	runq_$day.xls" >>excel.txt
#
# Process the utlbstat/utlestat output
#
if recordify.sh
then
    mv report.7 sav_report.7
    rm -f report.*
    echo "ORAGS	orags.txt	orags_$day.xls" >>excel.txt
    echo "ORATS	orats.txt	orags_$day.xls" >>excel.txt
    cachehits.sh orags.txt cachehits.txt
    echo "CACHEHITS	cachehits.txt	cache_$day.xls" >>excel.txt
fi
start_time=`cat start_time`
#
# Process Accounting
#
if [ -f ps_beg -a -f ps_end -a -f pacct.txt ]
then
    sarprep -p ps_beg ps_end pacct.txt
    echo "RESOUT	resout.txt	resout_$day.xls" >>excel.txt
fi
#
# Produce the vmstat output
#
if [ -f vm.out ]
then
    sarprep -t 1200 -s $start_time -v vm.out
    echo "IOCPU	vmout.txt	vmcpu_$day.xls" >>excel.txt
    echo "SAR_SCR	vmscr.txt	vmrunq_$day.xls" >>excel.txt
fi
tar cf summary_$day.tar badsort.exp *.txt
bzip2 summary_$day.tar
mv summary_$day.tar.bz2 ../..
return
}
# Set up the execution environment
#
ulimit unlimited
ORACLE_SID=cfacs
ORACLE_HOME=/u05/oracle73/app/oracle/product/7322
export ORACLE_HOME ORACLE_SID
PERFDB_HOME=/u03/e2/perfdb
export PERFDB_HOME
PATH=$PERFDB_HOME:$PERFDB_HOME/../e2common:$PATH:$ORACLE_HOME/bin
export PATH
#
# Find out if we are already running. Exit if we are.
#
if ps -ef | grep '[b]adsort'
then
    ps -ef | awk '/[b]adsort/ { print $2}' | xargs kill -USR1
fi
if ps -ef | grep '[m]ultista'
then
    ps -ef | awk '/[m]ultista/ { print $2}' | xargs kill -15
fi
if ps -ef | grep '[l]ockche'
then
    ps -ef | awk '/[l]ockch/ { print $2}' | xargs kill -9
fi
if [ "$1" = stop ]
then
    exit
fi
cd $PERFDB_HOME
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
for ORACLE_SID in cfacs
do
export ORACLE_SID
#
# Set up a new directory for today's files
#
if [ -d $ORACLE_SID.$nxt ]
then
    rm -rf $ORACLE_SID.$nxt
fi
badsort -A $ORACLE_SID.$nxt
#
# Store the incremented name
#
cd $ORACLE_SID.$nxt
tosecs >start_time
#
# Update the search terms
#
badsort -B internal
#
# Start the bstat/estat logging
#
multistat.sh >/dev/null 2>&1 &
#
# Start the SQL Monitors
#
badsort -c .00005 -r 8 -q internal >/dev/null 2>&1&
lockcheck.sh >/dev/null 2>&1 &
#
# Start the UNIX Monitors
#
/usr/lib/sa/sadc 1200 72 sad &
vmstat 1200 72 >vm.out &
#
# Pre-process the data from the run before
#
cd ../$ORACLE_SID.$lst
office_ise
cd ..
done
exit
