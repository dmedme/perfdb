#!/bin/sh
# Pull out all the program source
#
svrmgrl << EOF
connect internal
set linesize 132
set pagesize 0
spool progsource.sql
select a.source from source$ a, obj$ b
where a.obj# = b.obj#
and b.owner# >= 8
order by b.owner#, b.name, a.obj#, a.line;
spool off
spool allinds.tmp
select table_owner||'|'||table_name||'|'||index_name||'|'||
column_name from dba_ind_columns
where table_owner not like 'SYS%'
order by table_owner, table_name, index_name, column_position;
spool off
exit
EOF
#
# Strip out the white space
#
ex progsource.sql << EOF
g/[ 	][ 	]*\$/s///
g/^\$/d
w
q
EOF
sed 's/[ 	][ 	]*$//' allinds.tmp|nawk -F"|" 'BEGIN {li=""}
NF == 4 { if (li != $3)
{
    if (li != "")
        print l
    l = $0
    li = $3
}
    else
        l = l "|" $4
}
END {
    if (li != "")
        print l
}'  > allinds.lis
