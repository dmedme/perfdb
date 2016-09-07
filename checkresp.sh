#!/bin/sh
# checkresp.sh - Run a PATH script to determine if response is acceptable.
#
# Loop forever, testing the response times on the HISS system
#
. e2cserv.sh
e2com_ini_env
set -x
while :
do
this=`tosecs`
seq=`expr 1 + \\( $this % 86400 \\) / 1200`
#
# Kick off a thread for each host
#
for host in hiss01 hiss02
do
(
ptydrive <`which $host.$seq.ech` log.$host.$this 1 1 1 >/dev/null 2>&1
now=`tosecs`
#
# Load the log file into the database
nawk -F: 'BEGIN { cat_cnt = 0 }
$6 == "A" {
# Setup event ... remember the description.
for (i = 0; i< cat_cnt; i++)
{
if (lab[i] == $2 $7 )
    next
}
cat_cnt++
lab[i] = $2 $7
desc[lab[i]] =  $NF
next
}
$6 == "Z" {
atim = substr($5,1,9) + 0
key= $2 $7
narr=desc[key]
if ( $7 == "X0" || $7 == "X1" || $7 == "X2")
    out="SERVER"
else
if ( $7 == "X3")
    out="ORACLE"
else
    out="SLOWNESS"

#
# Abort Event Detected; there must have been an outage.
#
            print "{TIME_TYPE=S HOST_NAME=\"'$host'\"  START_TIME=" atim " EL=" ('$now' - atim) " REC_TYPE=HISS_OUT REC_INSTANCE=\"" out "\" NARRATIVE=\"" narr "\" }"
next }
NF > 6 {
#
# A normal response time
#
key= $2 $6
narr=desc[key]
etim = substr($5,1,9) + 0
            print "{TIME_TYPE=S HOST_NAME=\"'$host'\" START_TIME=" etim " RESP=" ($7/100) " OCC=1 REC_TYPE=RESPONSE REC_INSTANCE=\"" narr "\" }"
}' log.$host.$this | e2sub -l 1 -t SP -f `pwd`/resp.$host.$this -w -n e2com_fifo
) &
done
#
# Wait for both threads to exit
#
wait
    after=`tosecs`
    sleep_int=`expr 1200 + $this - $after`
    sleep $sleep_int
done
exit 0
