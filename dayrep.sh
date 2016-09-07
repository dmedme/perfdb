#!/bin/sh
# dayrep.sh - process the forms instrumentation data daily.
#
# Clear down the log file
#
. e2cserv.sh
e2com_ini_env
host=`uname -n`
# ***************************************************************************
# Process forever
#
while :
do
this=`tosecs`
mv actcost.dat actcost.sav
cat /dev/null > actcost.dat
chmod 0666 actcost.dat
#
# Load the log file into the database
#
sort -t\: -n +1 actcost.sav | nawk -F\: '{
            print "{TIME_TYPE=S HOST_NAME=\"'$host'\" USER_NAME=\"" nm "\" PID1=" $4 " PID2=" $5 " START_TIME=" $6 " EL=" $8 " OCC=1 U_CPU=" $9 " S_CPU=" $10 " I1=" $11 " I2=" $12 " I3=" $13 "  REC_TYPE=MENUALL REC_INSTANCE=\"" $3 "\" }"
            print "{TIME_TYPE=S HOST_NAME=\"'$host'\" USER_NAME=\"" nm "\" PID1=" $4 " PID2=" $5 " START_TIME=" $6 " EL=" $14 " OCC=1 U_CPU=" $15 " S_CPU=" $16 " I1=" $17 " I2=" $18 " I3=" $19 "  REC_TYPE=MENUONE REC_INSTANCE=\"" $3 "\" }"
}' | e2sub -l 1 -t SP -f `pwd`/menu$this -w -n e2com_fifo
    after=`tosecs`
    sleep_int=`expr 86400 + $this - $after`
    sleep $sleep_int
done
exit 0
