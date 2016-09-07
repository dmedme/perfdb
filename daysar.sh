#!/bin/sh
#
# Capture sar output for the rest of a day, forever.
#
. e2cserv.sh
e2com_ini_env
while :
do
this=`tosecs`
goes=`expr \( 86400 - \( $this % 86400 \)  \) / 1200 `
/usr/lib/sa/sadc 1200 $goes sad$this
#
# Order of options must correspond to that expected by sarproc.awk
#
for i in u d q c y r
do
   sar -$i -f sad$this
done  | nawk -f ` which sarproc.awk ` | e2sub -f `pwd`/sar$this -t SP -l 1 -w -n e2com_fifo
rm sad$this
done
exit
