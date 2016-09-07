#!/bin/sh
# e2monitor.sh - run the SQL monitoring every day.
#
# Set up the execution environment
#
ulimit unlimited
PERFDB_HOME=/u3/e2/2000/perfdb
export PERFDB_HOME
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
fi
ORACLE_HOME=/u1/oracle/v7016
export ORACLE_HOME
PATH=$PERFDB_HOME:$PATH:$ORACLE_HOME/bin
export PATH
#
# Loop - Monitor each Database instance
#
echo $nxt > nxtid.dat
for ORACLE_SID in CFX FIN
do
export ORACLE_SID
#
# Set up a new directory for today's files
#
badsort -A $ORACLE_SID.$nxt
#
# Store the incremented name
#
cd $ORACLE_SID.$nxt
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
badsort -c .00008 -r 9 -q internal >/dev/null 2>&1 &
lockcheck.sh >/dev/null 2>&1 &
#
# Pre-process the data from the run before
#
cd ../$ORACLE_SID.$lst
if recordify.sh
then
    mv report.5 sav_report.5
    rm -f report.*
fi
sarday=`nawk -F- '{print $1; exit}' orags.txt`
if [ ! -z "$sarday" ]
then
    allsar.sh $sarday > allsar.lis
fi
cd ..
done
exit
