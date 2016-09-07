#!/bin/sh
# every20.sh - Collect data every 20 minutes or so on hiss01 and hiss02.
# Data collected:
# - User session history (from wtmp)
# - Summarised processing accounting (from pacct)
# - UNIX Space Utilisation (from df)
# (hiss01 only)
# - ORACLE Accumulators (from ORACLE virtual tables)
# - ORACLE Space Utilisation (from ORACLE Schema).
#
# These are all collected in a single command file:
# - For the convenience of source management
# - To ensure that only one step of the process is active at any time
#
# If it is started with a parameter, that is the
# last time to process, otherwise the last time is 20 minutes
# prior to now.
#
# The capture sub-system will accumulate items for the same slot,
# but network traffic is minimised if the capture programs accumulate
# themselves, particularly for the Process Accounting, where the numbers
# of records are very large.
#
. e2cserv.sh
e2com_ini_env

    if [ $# -gt 0 ]
    then
        last=$1
    else
        last=`ls -t row*.lis | sed -n '1 s/row\([1-9][0-9]*\).lis/\1/
    1 p'`
    fi
    host=`uname -n`
#
# Forever; collect data every 20 minutes.
#
while :
do
    this=`tosecs`
# ***********************************************************************
# Report Session History
#
    if [ "$last" = "" ]
    then
        early=`expr $this - 1200`
    else
        rm -f row$early.lis acc$early.lis
        early=$last
    fi
# ***********************************************************************
# getsess output:
# - user
# - session length
# - time (seconds since 1970)
# - time (normal date format)
# - terminal
# - id
# - type information (x 2)
# getsess is preferred over other wtmp access programs because:
# - it handles clock adjustments
# - it allows date range selection
# The -c option tells it to report all time spent by sessions that
# terminate during the time range, and not to:
# - truncate time spent before the start of the selected time
# - count time within the selected period of sessions that terminated
#   afterwards 
#
    if [ -f /etc/wtmp ]
    then
#
# Traditional UNIX
#
        wtmp=/etc/wtmp
    else
#
# Modern UNIX
#
        wtmp=/var/adm/wtmp
    fi
    getsess -s $early -e $this -c $wtmp | nawk '{
#
# Share the time out over 20 minute slots
#
    fin_time = $3 + $2
    avail = fin_time % 1200
    if (avail == 0)
        avail = 1200
    first = fin_time - avail 
    left = $2
    while (left > 0)
    {
       if (left > avail)
       {
           to_do = avail
           left -= to_do
       }
       else
       {
           to_do = left
           left = 0
       }
       print  $1 " " first " " to_do
       avail = 1200
       first -= avail
    }
}' | sort | nawk 'BEGIN {
user=""
st=0
}
{
if ( $1 != user || $2 !=  st)
{
    if (user != "")
       print "{TIME_TYPE=S HOST_NAME=\"'$host'\" REC_TYPE=SESSION USER_NAME=\"" user "\" OCC=" occ " EL=" el " START_TIME=" st " }"
    user = $1
    st = $2
    el = $3
    occ = 1
}
else
{
   occ++
   el += $3
}
}
END {
    if (occ > 0)
       print "{TIME_TYPE=S HOST_NAME=\"'$host'\" REC_TYPE=SESSION USER_NAME=\"" user "\" OCC=" occ " EL=" el " START_TIME=" st " }"
}' | e2sub -l 1 -t SP -f `pwd`/wtmp$this -w -n e2com_fifo
# ***********************************************************************
# hisps outputs a faithful rendition of the process acconting record.
# It is preferred over acctcom because:
# - it is available on ALL Unix implementations (although it has to be
#   said that those that do not support acctcom are no longer commercially
#   significant)
# - it is much more efficient (because it does not translate user numbers
#   to names, etc.)
# - it emits all the process accounting fields
# The output is:
# - Command
# - User time
# - System time
# - Elapsed time
# - Start time
# - UID
# - GID
# - Memory
# - IO
# - Major, Minor
# - Accounting Flags
#
    if [ -f /var/adm/pacct ]
    then
#
# Modern UNIX
#
        acbase=/var/adm/pacct
    elif [ -f /usr/adm/pacct ]
    then
#
# Old System V
#
        acbase=/usr/adm/pacct
    else
#
# BSD
#
        acbase=/usr/adm/acct
    fi
#
# The data will be voluminous. Thus, for reporting purposes, we merge
# things.
# For reporting purposes, we accumulate separately:
# - anything starting with run (eg. runform3, and runmenu5)
# - anything starting with sql (eg. sqlplus, sqldba, sqlldr)
# - oracle
#
# Everything else gets lumped together as other.  
#
    cat `ls -tr ${acbase}*` | hisps -s $early -e $this -n | sort -t"|" -n +5 -6 |
    nawk -F\| 'function readproc() {
    if ((getline)<1)
        exit
    else
    {
        aid = $6 + 0
        x = substr($1,1,3)
        if ($1 == "run" || $1 == "sql" || $1 == "rpt" || $1 == "rpf" )
            comm = $1
        else
        if ($1 =="oracle  ")
            comm = "oracle"
        else
            comm = "other"
        u_cpu=$2
        s_cpu=$3
        el=$4
        st=$5
        mem=$8
        v_i=$9
    }
    return
}
BEGIN {
pwcom="nawk -F: '\''{print $2 \"|\" $1 }'\'' /etc/passwd | sort -t\\| -n"
#
# Step one. Substitute the user name for the ID, and change the process
# name as described. Only carry forward fields of interest.
#
    if ((pwcom|getline)< 1)
        pid=999999
    else
    {
        pid=$1
        nm=$2
    }
    readproc()
#
# Loop - merge in the names
#
    for(;;)
    {
        if (aid <= pid)
        {
            if (aid < pid)
                nm = "unk" aid


#
# Share the time out over 20 minute slots
#
            fin_time = st + el
            avail = fin_time % 1200
            if (avail == 0)
                avail = 1200
            first = fin_time - avail 
            left = el
            while (left > 0)
            {
                if (left > avail)
                {
                    to_do = avail
                    left -= to_do
                }
                else
                {
                    to_do = left
                    left = 0
                }
#
# Apportion other fields according to the share of el that to_do represents
#
                printf "%s %s %d %.2f %.2f %.2f %.2f %.2f\n", nm, comm, first, avail, avail*u_cpu/el, avail*s_cpu/el, avail*v_i/el, avail*mem/el
                avail = 1200
                first -= avail
            }
            readproc()
        }
        else
        if ((pwcom|getline)< 1)
            pid=999999
        else
        {
            pid=$1
            nm=$2
        }
    }
}' | sort | nawk 'BEGIN {
#
# Now accumulate commands together for each user and slot
# and write out in the format that we use for performance data
#
user=""
comm = ""
st=0
}
{
if ( $1 != user || $2 != comm || st != $3)
{
    if (comm != "")
       print "{TIME_TYPE=S HOST_NAME=\"'$host'\" REC_TYPE=PROCESS USER_NAME=\"" user "\" REC_INSTANCE=\"" comm "\" OCC=" occ " EL=" el " START_TIME=" st " U_CPU=" u_cpu " S_CPU=" s_cpu " V_I=" v_i " I1=" mem " }"
    user = $1
    comm = $2
    st = $3
    el = $4
    u_cpu = $5
    s_cpu = $6
    v_i = $7
    mem = $8
    occ = 1
}
else
{
   occ++
   el += $4
   u_cpu  += $5
   s_cpu += $6
   v_i += $7
   mem += $8
}
}
END {
    if (occ > 0)
       print "{TIME_TYPE=S HOST_NAME=\"'$host'\" REC_TYPE=PROCESS USER_NAME=\"" user "\" REC_INSTANCE=\"" comm "\" OCC=" occ " EL=" el " START_TIME=" st " U_CPU=" u_cpu " S_CPU=" s_cpu " V_I=" v_i " I1=" mem " }"
}' | tee tacct$this | e2sub -l 1 -t SP -f `pwd`/pacct$this -w -n e2com_fifo
#
# ************************************************************************
# File System Utilisation
#
df -v | nawk '/\// {
       print "{TIME_TYPE=S HOST_NAME=\"'$host'\" REC_TYPE=DFSPACE REC_INSTANCE=\"" $1 "\" V_I=" $3*512 " V_O=" $5*512 " START_TIME='$this' }"
}' | e2sub -l 1 -t SP -f `pwd`/df$this -w -n e2com_fifo
# ************************************************************************
# Only collect ORACLE data on the server
#
if [ "$host" = hiss01 ]
then
db_block_size=`sqlplus -s system/albatross482 << EOF
set echo off
set termout off
set feedback of
set verify off
select value from sys.v_\$parameter where name='db_block_size';
exit
EOF
`
if [ "$db_block_size" = "" ]
then
    db_block_size=2048
fi
#
# ORACLE Free Space
#
sqlplus -s system/albatross482 << EOF |
e2sub -l 1 -t SP -f `pwd`/orasp$this -w -n e2com_fifo
set pages 0
set echo off
set verify off
set termout off
set linesize 132
set feedback off
select '{HOST_NAME=''$host'' REC_TYPE=ORASPACE REC_INSTANCE='||
a.tablespace_name||' TIME_TYPE=S START_TIME=$this '||
' V_I='||to_char(a.bytes)||
' V_O='||to_char(sum(b.bytes))||'}'
from sys.dba_data_files a,
sys.dba_free_space b
where
    a.tablespace_name = b.tablespace_name
and a.file_id = b.file_id
group by a.tablespace_name, a.bytes;
rem
rem object counts
rem
select '{HOST_NAME=''$host'' REC_TYPE=ORAOBJ TIME_TYPE=S START_TIME=$this '||
' OCC='||to_char(count(*))||'}'
from sys.obj\$;
rem
rem objects whose next extent exceeds the largest free element in their
rem tablespace
rem
select '{HOST_NAME=''$host'' REC_TYPE=ORAEXCEP TIME_TYPE=S START_TIME=$this '||
' REC_INSTANCE='||nvl(f.name,a.name)||' V_I=' ||to_char( b.extsize*$db_block_size) || '}'
  from sys.obj\$ a, sys.tab\$ d, sys.ind\$ e, sys.undo\$ f,
       sys.seg\$ b, sys.fet\$ c
 where b.ts# = c.ts# and b.extsize > all(c.length)
 and b.ts# = d.ts#(+) and b.file# = d.file#(+) and b.block# = d.block#(+)
 and b.ts# = e.ts#(+) and b.file# = e.file#(+) and b.block# = e.block#(+)
 and b.file# = f.file#(+) and b.block# = f.block#(+)
 and ((nvl(d.obj#,e.obj#) = a.obj# and f.name is null)
  or f.name is not null);
exit
EOF
# *********************************************************************
# ORACLE Accumulator Values
# - Capture these to files
# - Compare them to previous files
# - If no previous, or ORACLE has shut down in the interval, use the
#   current values
# - Otherwise, subtract as appropriate
sqlplus -s system/albatross482 << EOF >/dev/null 2>&1
set pages 0
set echo off
set verify off
set termout off
set linesize 80
set feedback off
select parameter||':'||to_char(count)||':'||to_char(usage)||':'||
to_char(gets)||':'||to_char(getmisses)
from sys.v_\$rowcache
order by 1

spool row$this.lis
/
spool off
select b.name||':'||a.value
from sys.v_\$sysstat a,sys.v_\$statname b
where a.statistic# = b.statistic#
order by 1

spool acc$this.lis
/
spool off
exit
EOF
#
# Now merge with the previous figures. The algorithm copes with missing
# elements, although there should not be any.
#
    nawk -F\: 'function readfil(fil) {
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
# First, the row cache
# 
    oldkey=readfil("'row$early.lis'")
    if (oldkey != "z")
        oldrec = rec
    newkey = readfil("'row$this.lis'")
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
#
# Check to see if ORACLE has restarted, by seeing if the miss count has
# fallen
#
            onf = split(oldrec,oar,":")
            nnf = split(newrec,nar,":")
            if (oar[onf] > nar[nnf])
            {
                oldkey = "z"
                restart_flag = 1
            }
            else
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
            nnf = split(newrec,nar,":")
            if (oldkey == newkey)
            {
                onf = split(oldrec,oar,":")
                nar[nf] = nar[nf] - oar[nf]
                nar[nf - 1] = nar[nf - 1] - oar[nf - 1]
            }
            fin_time = '$this'
            avail = fin_time % 1200
            if (avail == 0)
                avail = 1200
            first = fin_time - avail 
            left = '$this' - '$early'
            while (left > 0)
            {
                if (left > avail)
                {
                    to_do = avail
                    left -= to_do
                }
                else
                {
                    to_do = left
                    left = 0
                }
       print "{TIME_TYPE=S HOST_NAME=\"'$host'\" REC_TYPE=ORAROWC REC_INSTANCE=\"" nar[1] "\" START_TIME=" first " I1=" nar[4]*to_do/1200 " I2=" nar[5]*to_do/1200 " V_I=" nar[2] " V_O=" nar[3] " }"
                avail = 1200
                first -= avail
            }
            newkey =readfil("'row$this.lis'")
            newrec = rec
        }
        else
        {
            oldkey =readfil("'row$early.lis'")
            oldrec = rec
        }
    }
#
# Second, the ORACLE accumulators
# 
    oldkey=readfil("'acc$early.lis'")
    if (oldkey != "z")
        oldrec = rec
    newkey = readfil("'acc$this.lis'")
#
# If could not connect to ORACLE, exit immediately
#
    if (newkey == "z")
        exit
    else
    {
        newrec = rec
        if (oldkey != "z" && !restart_flag)
        {
#
# Check to see if ORACLE has restarted, by seeing if the miss count has
# fallen
#
            onf = split(oldrec,oar,":")
            nnf = split(newrec,nar,":")
            if (oar[onf] > nar[nnf])
                oldkey = "z"
        }
    }
#
# Loop - merge the two files
#
    while (newkey != "z")
    {
        if (newkey <= oldkey)
        {
            nnf = split(newrec,nar,":")
            if (oldkey == newkey && nar[1] !~ "current")
            {
                onf = split(oldrec,oar,":")
                nar[nf] = nar[nf] - oar[nf]
            }
            fin_time = '$this'
            avail = fin_time % 1200
            if (avail == 0)
                avail = 1200
            first = fin_time - avail 
            left = '$this' - '$early'
            while (left > 0)
            {
                if (left > avail)
                {
                    to_do = avail
                    left -= to_do
                }
                else
                {
                    to_do = left
                    left = 0
                }
print "{TIME_TYPE=S HOST_NAME=\"'$host'\" REC_TYPE=ORASYS REC_INSTANCE=\"" nar[1] "\" START_TIME=" first " V_I=" nar[nf]*to_do/1200 " }"
                avail = 1200
                first -= avail
            }
            newkey =readfil("'acc$this.lis'")
            newrec = rec
        }
        else
        {
            oldkey =readfil("'acc$early.lis'")
            oldrec = rec
        }
    }
}' | e2sub -l 1 -t SP -f `pwd`/ora$this -w -n e2com_fifo
fi 
    after=`tosecs`
    sleep_int=`expr 1200 + $this - $after`
    sleep $sleep_int
done
exit 0
