set pages 5000
column a1 format a15 head 'Tablespace Name'
column a2 format a30 head 'File Name'
column a3 format 99999999999 head 'Bytes|Configured'
column a4 format 99999999999 head 'Bytes|Free'
select a.tablespace_name a1,
a.file_name a2,
a.bytes a3,
nvl(sum(b.bytes),0) a4
 from
dba_data_files a, dba_free_space b
where a.file_id = b.file_id(+) group by a.tablespace_name, a.file_name, a.bytes

spool allsp.lis
/
spool off

