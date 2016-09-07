#!/bin/sh
# compind.sh - compare indexes on different databases
#
#
. /export/home/dedwards/stellar_server_perf/monscripts/env.sh
get_inds() {
ORACLE_SID=$1
export ORACLE_SID
lbl=$2
uid=$3
sqlplus "$3" << EOF
set pages 0
set lines 150
set echo off
set feedback off
set verify off
column table_owner format a32
column table_name format a32
column index_name format a32
column column_name format a32
select table_owner,table_name,index_name, column_name from dba_ind_columns
where table_owner not like 'SYS%'
order by 1,2,3,column_position

spool ind$lbl.tmp
/
spool off
exit
EOF
#
# Get rid of the index name?
#
nawk 'BEGIN {
li = ""}
NF == 4 {
    if (li != $3)
    {
        if (li != "")
            print ind_line
        li = $3
        ind_line = $1 "|" $2 "|" $3 "|" $4
    }
    else
        ind_line = ind_line "|" $4
}
END {
    if (li != "")
        print ind_line
}' ind$lbl.tmp  | sort > ind$lbl.lis
    return
}
#
# Extract the indexes
#
get_inds STPROD1 STPROD1 '/ as sysdba'
#
exit
