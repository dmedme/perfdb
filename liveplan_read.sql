set pages 5000
set lines 132
set echo off
select a.sid||' ' ||h.pln "Execution Plan"
from sys.v_$session a, sys.v_$sqltext b,
(select address, lpad(' ',2*level)||operation||' '||
    options||' '||
    object_name||' '||
    object_node pln
from sys.v_$sql_plan
connect by prior id = parent_id
and prior address = address
start with parent_id is null
) h
where b.address = a.sql_address
and b.address = h.address
and b.sql_text like '%gl_bc_packets%'
/
set echo on
