#!/bin/sh
nawk ' function do_print() {
print ""
print "Total Executions: 0 Disk Reads: 0 Buffer Gets: 0"
for (i=0; i < ln_cnt; i++)
    print ln[i]
ln_cnt = 0
print ""
if (md_cnt > 0)
{
print "Candidate Modules"
for (i=0; i < md_cnt; i++)
    print md[i]
md_cnt = 0
}
return
}
/^Falling back/ { next }
/^sql/ { if (ln_cnt > 0 )
    do_print()
    next
}
BEGIN { md_cnt = 0
ln_cnt = 0
}
END {
 if (ln_cnt > 0)
    do_print()
}
/^\// {md[md_cnt] = $0
md_cnt++
next
}
{
   ln[ln_cnt] = $0
   ln_cnt++
}' junk.log | sed 's/\.\.\. Word Sequence Match//' > junk.lis
