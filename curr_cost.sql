column a head 'Program' format a40
column b head 'Machine' format a20
column c head 'Process ID' format a12
column d head 'Server PID' format a6
column e head 'Logical|Reads' format 99999990
column f head 'Physical|Reads' format 99999990
column g head 'User' format a8
column h head 'Current Statement' format a64

break on g nodup on a nodup on b nodup on c nodup on d nodup on e nodup on f nodup
variable cons_gets number
variable db_block_gets number
variable phys_reads number

begin
   select statistic#
      into :cons_gets
   from v$statname
   where name = 'consistent gets';
   select statistic#
      into :db_block_gets
   from v$statname
   where name = 'db block gets';
   select statistic#
      into :phys_reads
   from v$statname
   where name = 'physical reads';
end;
/
set pages 5000
set lines 200
select
    a.osuser g,
    a.program a,
    a.machine b,
    a.process c,
    b.spid d,
    c.value + d.value e,
    e.value f,
    f.sql_text
from
    v$process b,
    v$sesstat c,
    v$sesstat d,
    v$sesstat e,
    v$sqltext f,
    v$session a
where
    a.paddr = b.addr
and a.sid = c.sid
and a.sid = d.sid
and a.sid = e.sid
and  c.statistic# = :cons_gets
and  d.statistic# = :db_block_gets
and  e.statistic# = :phys_reads
and a.sql_address = f.address(+)
order by 1,2,3,4,5,f.piece;
