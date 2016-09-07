rem
rem checkmax.sql - check that no objects have a next extent greater
rem than the largest free space extent in their tablespace, or are within
rem 5 extents of their maximum
rem
rem Copyright (c) E2 Systems Limited, 1997
rem
set lines 132
set pages 62
set newpage 0
column "Tablespace" head 'Tablespace|Name' format a24 ;
column "Name" head 'Object|Name' format a24 ;
column "Owner" head 'Owner' format a12 ;
column "Next" head 'Blocks|Needed' format 999999990 ;
column "Extents" head 'Current|Extents' format 999999990 ;
column "Limit" head 'Maximum|Extents' format 999999990 ;
ttitle 'Objects that will blow their Tablespace or Extent Limits'

select i.name "Tablespace",
     nvl(a.name,nvl(g.name, nvl(m.name, k.name))) "Name",
     nvl(b.name,h.name) "Owner",c.extsize "Next",c.extents "Extents",
c.maxexts "Limit"
from sys.obj$ a,
sys.user$ b,
sys.obj$ g,
sys.obj$ m,
sys.user$ h,
sys.tab$ d,
sys.ind$ e,
sys.ts$  i,
sys.clu$ j,
sys.undo$ k,
sys.file$ l,
sys.seg$ c
where
a.owner# = b.user#(+)
and a.obj#(+) = e.obj#
and m.obj#(+) = j.obj#
and e.file#(+) = c.file#
and e.block#(+) = c.block#
and g.owner# = h.user#(+)
and g.obj#(+) = d.obj#
and d.file#(+) = c.file#
and d.block#(+) = c.block#
and k.file#(+) = c.file#
and j.block#(+) = c.block#
and k.file#(+) = c.file#
and j.block#(+) = c.block#
and c.file# = l.file#
and i.ts# = l.ts#
and (c.extsize >
(select  
  max(f.length)
from sys.fet$ f
where c.ts# = f.ts#
)
or (c.maxexts - c.extents) < 5 and c.maxexts > 0)
order by 1;
