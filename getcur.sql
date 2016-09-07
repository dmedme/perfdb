set pages 5000
set lines 132
column sid format a10
column process format a10
column program format a10
column spid format a10
column sql_text format a80
break on sid nodup on process nodup on program nodup on spid nodup

select a.sid, a.process, a.program, c.spid, b.sql_text 
from sys.v_$session a, sys.v_$sqltext b, sys.v_$process c
where b.address = a.sql_address
and c.addr = a.paddr
 and c.spid in (&&pids)
order by 1,2,3,4, b.address, b.piece

spool getcur.lis
/
spool off
