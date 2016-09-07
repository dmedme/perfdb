#!/bin/sh
# e2drep.sh - Sample Report on the Performance Database
#
# The purpose of this report is to illustrate how a simple general
# purpose reporting mechanism allowing the production of high
# quality graphical output from within the PC environment might be
# built.
# 
# Parameters:
# 1 - The name of the output file
# 2 - The Time Type reported on
#
# Further parameters would determine the selection of the columns, whether
# average, maximum, standard deviations etc. were desired, which hosts
# etc. were desired. For now we report on:
# - Average values
# - hiss01 and hiss02 separately
# - CPU
# - Disk I/Os
# - Run Q
# - Free Memory
# - Average response time (all transactions amalgamated)
# - Outage
#
# **********************************************************************
# Validate Parameters
#
# set -x
if [ $# -lt 2 ]
then
    echo Provide an Output File and the Date Type to Report On
    exit 1
fi
rep_name=$1
dw="b.start_time(+) >= a.start_time and b.start_time(+) < (a.start_time + a.duration) and"
dt=$2
case "$dt" in
D)
    dt_fmt='DD-Mon'
    ;;
S)
    dt_fmt='Dy HH24:MI:SS'
    ;;
DOW)
    dw="to_number(to_char(b.start_time(+),'SSSSS')) >= to_number(to_char(a.start_time(+),'SSSSS')) and to_number(to_char(b.start_time(+),'SSSSS')) < (to_number(to_char(a.start_time(+),'SSSSS')) + 1200) and"
    dt_fmt='Dy HH24:MI:SS'
    ;;
W)
    dt_fmt='YY WW'
    ;;
M)
    dt_fmt='YY Mon'
    ;;
*)
    echo "Date Format must be S (slot), D (day), DOW (day of week), W (week) or M (month)"
    exit 1
    ;;
esac

rep_time=`date`
sqlplus perfmon/perfmon << EOF
set lines 132
set pages 64
set newpage 0
set echo on
set echo off
set termout off
set feedback off
clear break
clear compute
ttitle left 'Report Date: $rep_time' center 'Performance Report' col 110 'Page :' sql.pno skip 1 -
left 'Date Type: $dt' skip 1
column a head 'When' format a12
column a1 head 'hiss01|CPU%' format 990.99
column a2 head 'hiss02|CPU%' format 990.99
column a3 head 'hiss01|DIO' format 99999999990
column a4 head 'hiss02|DIO' format 99999999990
column a5 head 'hiss01|RunQ' format 990
column a6 head 'hiss02|RunQ' format 990
column a7 head 'hiss01|Free Mem' format 999990
column a8 head 'hiss02|Free Mem' format 999990
column a9 head 'hiss01|Response' format 990.99
column a10 head 'hiss02|Response' format 990.99
column a11 head 'hiss01|Outages' format 990.99
column a12 head 'hiss02|Outages' format 990.99

select to_char (a.start_time,'$dt_fmt') a,
sum(decode(b.rec_type,'SARCPU',
    decode(upper(b.host_name),'HISS01',
              b.u_cpu_sum+b.s_cpu_sum,0),0))/(a.duration*864) a1,
sum(decode(b.rec_type,'SARCPU',
    decode(upper(b.host_name),'HISS02',
              b.u_cpu_sum+b.s_cpu_sum,0),0))/(a.duration*864) a2,
sum(decode(b.rec_type,'SARDIO',decode(upper(b.host_name),'HISS01',
            b.occ_sum,0),0)) a3,
sum(decode(b.rec_type,'SARDIO',decode(upper(b.host_name),'HISS02',
            b.occ_sum,0),0)) a4,
sum(decode(b.rec_type,'SARRUNQ',decode(upper(b.host_name),'HISS01',
            b.q_sum,0),0))/ greatest(sum(decode(b.rec_type,'SARRUNQ',
      decode(upper(b.host_name),'HISS01',b.samples,0),0)),1) a5,
sum(decode(b.rec_type,'SARRUNQ',decode(upper(b.host_name),'HISS02',
            b.q_sum,0),0))/ greatest(sum(decode(b.rec_type,'SARRUNQ',
      decode(upper(b.host_name),'HISS02',b.samples,0),0)),1) a6,
sum(decode(b.rec_type,'SARMEM',decode(upper(b.host_name),'HISS01',
          b.v_i_sum,0),0))/greatest(sum(decode(b.rec_type,'SARMEM',
      decode(upper(b.host_name),'HISS01',b.samples,0),0)),1) a7,
sum(decode(b.rec_type,'SARMEM',decode(upper(b.host_name),'HISS02',
          b.v_i_sum,0),0))/ greatest(sum(decode(b.rec_type,'SARMEM',
      decode(upper(b.host_name),'HISS02',b.samples,0),0)),1) a8,
sum(decode(b.rec_type,'RESPONSE',decode(upper(b.host_name),'HISS01',
          b.resp_sum,0),0))/greatest(sum(decode(b.rec_type,'RESPONSE',
      decode(upper(b.host_name),'HISS01',b.samples * b.occ_sum,0),0)),1) a9,
sum(decode(b.rec_type,'RESPONSE',decode(upper(b.host_name),'HISS02',
          b.resp_sum,0),0))/ greatest(sum(decode(b.rec_type,'RESPONSE',
      decode(upper(b.host_name),'HISS02',b.samples * b.occ_sum,0),0)),1) a10,
sum(decode(b.rec_type,'HISS_OUT',1,0)) a11,
sum(decode(b.rec_type,'HISS_OUT',1,0)) a12
from pdb_calendar a,
pdb_data b
where
    a.data_type = '$dt' and $dw
b.time_type(+) = a.data_type
group by a.duration,
a.start_time
order by a.start_time

spool $rep_name
/
prompt
prompt End of Report
spool off
exit
EOF
exit
