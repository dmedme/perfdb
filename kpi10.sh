#!/bin/ksh
# kpi10.sh - Collect data every 10 minutes or so.
# Data collected:
# - ORACLE Accumulators (from ORACLE virtual tables)
# Potentially, UNIX data
#
# These are all collected in a single command file:
# - For the convenience of source management
# - To ensure that only one step of the process is active at any time
#
OPWD=dedwards/d3dw4rd@SDWLIV
export OPWD
    if [ $# -gt 0 ]
    then
        last=$1
    else
        last=`ls -t row*.lis | sed -n '1 s/row\([1-9][0-9]*\).lis/\1/
    1 p'`
    fi
#
# Forever; collect data every 10 minutes.
#
while :
do
    this=`/export/home/dedwards/tosecs`
# ***********************************************************************
# Report Session History
#
    if [ "$last" = "" ]
    then
        early=`expr $this - 600`
    else
        rm -f row$early.lis acc$early.lis
        early=$last
    fi
# *********************************************************************
# ORACLE Accumulator Values
# - Capture these to files
# - Compare them to previous files
# - If no previous, or ORACLE has shut down in the interval, use the
#   current values
# - Otherwise, subtract as appropriate
sqlplus -s $OPWD << EOF >/dev/null 2>&1
set pages 0
set echo off
set verify off
set termout off
set trimspool on
set linesize 80
set feedback off
select b.name||'}'||a.value
from sys.v_\$sysstat a,sys.v_\$statname b
where a.statistic# = b.statistic#
and a.value > 1
order by 1

spool acc$this.lis
/
spool off
set linesize 160
select
c.name||':'|| 
decode(b.type#, 1, 'INDEX',
2, 'TABLE',
19, 'TABLE PARTITION',
20, 'INDEX PARTITION',
21, 'LOB',
40, 'LOB PARTITION',
'Type# '||to_char(b.type#)) ||':'||
b.name||':'||
to_char(nvl(a.dataobj#,-1))||':'||
a.statistic_name||'}'||
a.value
from sys.v_\$segstat a,
   sys.obj\$ b,
   sys.ts\$ c
where
   a.obj# = b.obj#
and nvl(a.dataobj#, -1) = nvl(b.dataobj#, -1)
and a.ts# = c.ts#
and a.value > 0
order by 1

spool row$this.lis
/
spool off
exit
EOF
#
# Now merge with the previous figures. The algorithm copes with missing
# elements, although there should not be any.
#
    nawk -F\} 'function readfil(fil) {
    if ((getline<fil)<1)
        return "z"
    else
    {
        rec = $0
        return $1
    }
}
# *****************************************************************************
# Perform a serial merge, and compute the differences in accumulator values
# - f1 is the earlier snapshot
# - f2 is the current snapshot
# - f3 is the output file
# - ts1 is the first timestamp
# - ts2 is the second timestamp
# - intvl is the interval we what to divide our total over
# *****************************************************************************
# This does not work because you cannot pass an array to a function.
# *****************************************************************************
# - acc_cnt is the count of accumulators
# - accs is an an array of field numbers to difference
# So these must be global.
# *****************************************************************************
# If the narrative starts "current", do not compute difference. We do not write
# out the "current" values if there is no earlier file.
# *****************************************************************************
function merge_diff(f1, f2, f3, ts1, ts2, intvl)
{
#
# We do nothing if either file is in fact empty. Debatable, this.
#
# Start on the earlier file
#
#    print "Merging " f1 " and " f2
    oldkey=readfil(f1)
    if (oldkey == "z")
    {
#        print "File " f1 " is empty!?"
        close(f1)
        return
    }
    oldrec = rec
#
# Now the current file
#
    newkey = readfil(f2)
    if (newkey == "z")
    {
#        print "File " f2 " is empty!?"
        close(f1)
        close(f2)
        return
    }
    newrec = rec
    restart_test = 1
#
# The files exist, and have records in them.
# - With a match, we write the difference
# - Old only, write zero
# - New only, write total
# - Match, write difference, unless re-start is apparent, in which case,
#   delete the output file, and return
#
    while (oldkey != "z" && newkey != "z")
    {
#        print oldkey " " newkey
        if (newkey <= oldkey)
        {
#            print "Match or new value"
            nnf = split(newrec,nar,"}")
            if (oldkey == newkey && nar[1] !~ /^current/)
            {
                onf = split(oldrec,oar,"}")
                if (restart_test == 1)
                {
                    if (oar[accs[1]] > nar[accs[1]])
                    {
#                       print "Restart: " oldkey ": " oar[acs[1]] " > " nar[accs[1]]
                       close(f1)
                       close(f2)
                       close(f3)
                       unlink(f3)
                       return
                    } 
                    restart_test = 0
                }
                for (i = 1; i <= acc_cnt; i++)
                    nar[accs[i]] = nar[accs[i]] - oar[accs[i]]
            }
            if (nar[1] ~ /^current/)
            {
                orec = ts2 "}0}"
                for (i = 1; i <= nnf; i++)
                    orec = orec "}" nar[i]
                print orec>f3
            }
            else
            {
                left = ts2 - ts1
                for (first = ts1; left > 0;)
                {
                    if (left < intvl + intvl)
                    {
                        avail = left
                        left = 0
                        orec = first "}" avail
                        for (i = 1; i <= nnf; i++)
                            orec = orec "}" nar[i]
                    }
                    else
                    {
                        avail = intvl
                        left -= intvl
                        orec = ts2 "}" avail
                        for (i = 1; i <= nnf; i++)
                            par[i] = nar[i]
                        for (i = 1; i < arr_cnt; i++)
                        {
                            par[arrs[i]] = avail/(ts2 - ts1) * par[arrs[i]]
                            nar[arrs[i]]= nar[arrs[i]] - par[arrs[i]]
                        }
                        for (i = 1; i <= nnf; i++)
                            orec = orec "}" par[i]
                    }
                    print orec>f3
                    first += avail
                }
            }
            if (newkey == oldkey)
            {
                oldkey =readfil(f1)
                oldrec = rec
            }
            newkey =readfil(f2)
            newrec = rec
        }
        else
        {
#            print "Missing value"
#
# Extend the old record with zero values.
#
            onf = split(oldrec,oar,"}")
            for (i = 1; i <= acc_cnt; i++)
                oar[accs[i]] = 0
            if (oar[1] ~ /^current/)
            {
                orec = ts2 "}0"
                for (i = 1; i <= onf; i++)
                    orec = orec "}0"
                print orec>f3
            }
            else
            {
                left = ts2 - ts1
                for (first = ts1; left > 0;)
                {
                    if (left < intvl + intvl)
                    {
                        avail = left
                        left = 0
                    }
                    else
                    {
                        avail = intvl
                        left -= intvl
                    }
                    orec = first "}" avail "}"
                    for (i = 1; i <= onf; i++)
                        orec = orec "}0"
                    print orec>f3
                    first += avail
                }
            }
            oldkey = readfil(f1)
            oldrec = rec
        }
    }
    close(f1)
    close(f2)
    close(f3)
    return
} 
BEGIN {
#
# First I/O by segment
# 
     acc_cnt = 1
     accs[1] = 2
     merge_diff("'row$early.lis'","'row$this.lis'", "'rowdiff$this.lis'", '$early','$this', '$this' - '$early')
#
# Then, the global accumumulators
# 
     merge_diff("'acc$early.lis'","'acc$this.lis'", "'accdiff$this.lis'", '$early','$this', '$this' - '$early')
}' /dev/null
    after=`/export/home/dedwards/tosecs`
    sleep_int=`expr 600 + $this - $after`
    sleep $sleep_int
    last=$this
done
exit 0
