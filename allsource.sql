set lines 132
set pages 0
set feedback off
set verify off
select a.source from sys.source$ a,
sys.obj$ b 
where b.owner# > 1
and a.obj# = b.obj#
order by a.obj#, a.line

spool appsource.sql
/
spool off
