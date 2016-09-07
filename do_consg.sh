#!/bin/sh
# *********************************************************************
# ORACLE Accumulator Values
# - Capture these to files
# - Compare them to previous files
# - If no previous, or ORACLE has shut down in the interval, use the
#   current values
# - Otherwise, subtract as appropriate
early=0
this=1
while [ $this -lt 10 ]
do
sqlplus -s e2monitor/e2monitor@HWORFIN.WORLD << EOF >/dev/null 2>&1
set pages 0
set echo off
set verify off
set termout off
set linesize 20
set feedback off
select to_char(sid)||':'||to_char(statistic#)||'}'||to_char(value)
from sys.v_\$sesstat
where sid=70
order by sid||':'||to_char(statistic#)

spool consg$this.lis
/
spool off
exit
EOF
#
# Now merge with the previous figures. The algorithm copes with missing
# elements, although there should not be any.
#
    gawk -F\} 'function readfil(fil) {
    if ((getline<fil)<1)
        return "z"
    else
    {
        rec = $0
        return $1
    }
}
BEGIN {
#
# The consistent gets
# 
    oldkey=readfil("'consg$early.lis'")
    if (oldkey != "z")
        oldrec = rec
    newkey = readfil("'consg$this.lis'")
#
# If could not connect to ORACLE, exit immediately
#
    if (newkey == "z")
        exit
    else
    {
        newrec = rec
        if (oldkey != "z")
        {
            onf = split(oldrec,oar,"}")
            nnf = split(newrec,nar,"}")
            restart_flag = 0
        }
        else
            restart_flag = 1
    }
#
# Loop - merge the two files
#
    while (newkey != "z")
    {
        if (newkey <= oldkey)
        {
            nnf = split(newrec,nar,"}")
            if (oldkey == newkey)
            {
                onf = split(oldrec,oar,"}")
                nar[nnf] = nar[nnf] - oar[nnf]
            }
            if (nar[nnf] > 0)
            print newkey "}" nar[nnf]
            newkey =readfil("'consg$this.lis'")
            newrec = rec
        }
        else
        {
            oldkey =readfil("'consg$early.lis'")
            oldrec = rec
            onf = split(oldrec,oar,"}")
        }
    }
}' | sort -n -t"}" +1
    echo $this
    early=$this
    this=`expr $this + 1`
    sleep 10
done | tee active.lis
