#!/bin/sh
# badfind.sh - Search for bad SQL
# Parameters:
# 1 - d (disk reads) or b (buffer gets) or e (elapsed) or c (cost) sequence
# 2 - threshold
#
# Cost parameters are set (roughly) for Edinburgh machine
#
# **********************************************************************
# This file is a Trade Secret of E2 Systems. It may not be executed,
# copied or distributed, except under the terms of an E2 Systems
# UNIX Instrumentation for CFACS licence. Presence of this file on your
# system cannot be construed as evidence that you have such a licence.
# **********************************************************************
# @(#) $Name$ $Id$
# Copyright (c) E2 Systems Limited 1994
#ORACLE_HOME=/u01/app/oratrg01/product/805
#ORACLE_SID=TRG01
ORACLE_HOME=/u01/app/oradev03/product/805
ORACLE_SID=DEV03
PATH=$ORACLE_HOME/bin:$PATH
export ORACLE_HOME ORACLE_SID PATH
if [ $# -lt 2 ]
then
    echo "Provide b (buffer gets), d (disk reads), e (elapsed) or c (cost), and a threshold"
    exit 0
fi
case $1 in
b*)
   seq=a.buffer_gets
   ;;
d*)
   seq=a.disk_reads
   ;;
c*)
   seq="0.000023*(a.buffer_gets + a.disk_reads *10)"
   ;;
e*)
   seq="(0.000023*(a.buffer_gets + a.disk_reads *10)*1.2 + a.disk_reads/500)"
   ;;
*)
    echo Invalid parameter $1
    echo "Provide b (buffer gets), d (disk reads), e (elapsed) or c (cost), and a threshold"
    exit 1
   ;;
esac
thresh=$2
set -x
svrmgrl << EOF
connect internal
spool temp$$.lis
select b.address,
       a.executions,
       a.disk_reads,
       a.buffer_gets,
       a.parsing_schema_id,
       b.sql_text
from v\$sqlarea a,
     v\$sqltext b
where $seq > $thresh
and a.address=b.address
and lower(substr(a.sql_text,1,5)) not in ( 'begin',
'decla', 'expla', ' begi', ' decl')
order by $seq desc, b.address, b.piece;
spool off
exit
EOF
nawk 'BEGIN {stub = ""
flag = 0
print "set echo on"
print "set feedback off"
print "set pause off"
print "spool badplan'$$'.lis"
}
/^ADDRESS/ { flag = 1
getline
last_a = ""
next
}
flag == 0 { next }
function rindex(s,t) {
    for (i = length(s); i > 0; i--)
        if (substr(s,i,1) == t)
            break
    return i
}
/^[ 	]*$/ { next }
{
    if ($1 != last_a)
    {
        if (last_a != "")
        {
            print stub ";"
            print "@xplread"
        }
        last_a = $1
        print "REM " substr($0,10,33)
        nf = split(substr($0,10,43),arr," ")
        user_id = arr[nf]
        print "alter session set current_schema=sys;"
        print "column user_name new_value user_id"
        print "select name user_name from user$ where user#=" user_id ";"
        print "alter session set current_schema=&user_id;"
        print "delete plan_table;"
        print "commit;"
        print "explain plan set statement_id = '\''A'\'' for"
        stub=substr($0,54)
        next
    }
    stub = stub substr($0,54)
    f = rindex(stub," ")
    if (f == 0)
        f = rindex(stub,",")
    if (f == 0)
        f = rindex(stub,")")
    if (f == 0)
        f = rindex(stub,"(")
    print substr(stub,1,f)
    stub = substr(stub,f+1)
}
END {
    print stub ";"
    print "@xplread"
    print "spool off"
    print "exit"
}' temp$$.lis |
sed '/^[ 	]*$/ d
#s/&/:/g
s/[ 	][ 	]*$//' > badplan$$.sql
rm temp$$.lis
sqlplus system/manager @badplan$$
rm badplan$$.sql
ex badplan$$.lis << EOF
g/[ 	][ 	]*\$/s///
w
q
EOF
exit
