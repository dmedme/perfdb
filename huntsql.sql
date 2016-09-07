accept marker char prompt 'Enter statement marker : '
set pages 0
set lines 80
column sql_text format a80
select b.sql_text 
from sys.v_$sqltext b
where b.address in 
(select a.address from sys.v_$sqltext a
where lower(a.sql_text) like lower('%&marker%')
)
order by  b.address, b.piece

spool huntsql.lis
/
spool off
