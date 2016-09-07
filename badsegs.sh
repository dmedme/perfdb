#!/bin/sh
# badsegs.sh -  all objects with more than 30 segments
sqlplus sys/dougal << EOF >badsegs.lis
set pages 5000
set lines 132
column owner format a10
column segment_type format a10
column segment_name format a24
select owner,segment_name,segment_type,extents from dba_segments
where extents > 30 order by 1,2,3;
exit
EOF

