#!/bin/ksh
# e2rootmon.sh - run monitors needing root privileges 
# *************************************************************************
# To be run from root crontab daily
#
# Executing the script with the single argument stop should kill it all off
# *************************************************************************
PATH_HOME=/export/home/e2systems
ORACLE_HOME=/cedar/oracle/product/oratools6i
PATH=/usr/sbin:$PATH_HOME/unix:$PATH_HOME/netmon:$PATH_HOME/perfdb:/usr/local/bin:/usr/ccs/bin:$ORACLE_HOME/bin:/usr/bin:/usr/ucb:/etc:.
LD_LIBRARY_PATH=$ORACLE_HOME/lib
ORACLE_SID=EFIN
TNS_ADMIN=/cedar/oracle/product/oratools6i/network/admin
RDBMS_HOME=/cedar/oracle/product/8.1.7s
DEV2K_HOME=/cedar/oracle/product/oratools6i
TWO_TASK=EFIN
export PATH PATH_HOME ORACLE_HOME ORACLE_SID LD_LIBRARY_PATH RDBMS_HOME DEV2KHOME TNS_ADMIN
# ************************************************
# Function to capture network traffic
# ************************************************
capture() {
# Look for rogue transactions amongst the manual collection
if [ ! -d "$1" ]
then
    mkdir $1
    chown e2sys $1
fi
cd $1
ulimit -n 1024
save_id=0
trap "" 1 2
rm -f snoop.fifo
while :
do
    mkfifo snoop.fifo
#
# Simulation for testing when not root
#    sleep 100 >snoop.fifo &
    snoop -o snoop.fifo port 80 or port 9000 or port 9500 2>/dev/null &
    p=$!
    trap "kill -15 $p; break" 15
#    t3mon -l 8 -a 60  snoop.fifo>t3mon.log 2>&1
    genconv -l 8 snoop.fifo>t3mon.log 2>&1
    kill -9 $p
    rm -f snoop.fifo
    mkdir capt3.$save_id
#    mv core t3_*.msg t3mon.log capt3.$save_id
#    chown e2sys capt3.$save_id capt3.$save_id/*
    mv core t3mon.log capt3.$save_id
    chown e2sys capt3.$save_id capt3.$save_id/*
    save_id=`expr $save_id + 1`
done
#
# Process the results
#
#grep '|Session ' t3_*.msg t3mon.log capt3.*/t3_*.msg capt3.*/t3mon.log > allsess.log
grep '|Session ' t3mon.log capt3.*/t3mon.log > allsess.log
#grep '|RESPONSE|' t3_*.msg t3mon.log capt3.*/t3_*.msg capt3.*/t3mon.log > allresp.log
allresp.sh allsess.log 24 /dev/null 10.1.1.100 10.1.1.102 >allsess.txt
nawk -F"|" '$5 == 9000 { print }' allsess.txt | sarprep -c
#sarprep -u allresp.log
chown e2sys *
}
# ************************************************
# Functions to manage the monitoring
# ***************************************************
# Start
begin_stats() {
pid=$1
export pid
save_dir=$PATH_HOME/unix/save.$pid
export save_dir
mkdir $save_dir
chmod 0777  $save_dir
chown oracle $save_dir
tosecs >$save_dir/start_time
# **************************************************
# Trigger the monitors not being run by e2monitor.sh
iostat -xct 1200 > $save_dir/ioout &
io_pid=$!
mpstat 1200 > $save_dir/mpout &
mp_pid=$!
ps -ef > $save_dir/ps_beg
while :
do
    /usr/sbin/swap -s
    sleep 1200
done > $save_dir/swap_stats &
swap_pid=$!
netstat -k > $save_dir/kstat_beg
netstat -m > $save_dir/strstat_beg
netstat -i 1200 >$save_dir/netout &
net_pid=$!
capture $PATH_HOME/netmon/save.$pid &
cap_pid=$!
#/usr/sbin/vxstat -f sabvcfFM -i 1200 >$save_dir/vx_sabvcFM &
#vx_sabvcFM_pid=$!
#/usr/sbin/vxstat -f WROSCV -i 1200 >$save_dir/vx_WROSCV &
#vx_WROSCV_pid=$!
#/usr/sbin/vxstat -f sabvcfFM -g beadg -i 1200 >$save_dir/beadg_sabvcFM &
#beadg_sabvcFM_pid=$!
#/usr/sbin/vxstat -f WROSCV -g beadg -i 1200 >$save_dir/beadg_WROSCV &
#beadg_WROSCV_pid=$!
#/usr/sbin/vxstat -f sabvcfFM -g symmdg -i 1200 >$save_dir/symmdg_sabvcFM &
#symmdg_sabvcFM_pid=$!
#/usr/sbin/vxstat -f WROSCV -g symmdg -i 1200 >$save_dir/symmdg_WROSCV &
#symmdg_WROSCV_pid=$!
export io_pid swap_pid mp_pid net_pid cap_pid vx_sabvcFM_pid vx_WROSCV_pid symmdg_sabvcFM_pid symmdg_WROSCV_pid beadg_sabvcFM_pid beadg_WROSCV_pid
return
}
# ************************************************
# Function to terminate the monitors
#
end_stats() {
tosecs >$save_dir/end_time
kill -15 $swap_pid $mp_pid $net_pid $cap_pid $vx_sabvcFM_pid $vx_WROSCV_pid $symmdg_sabvcFM_pid $symmdg_WROSCV_pid $beadg_sabvcFM_pid $beadg_WROSCV_pid
sleep 2
kill -9 $sad_pid $io_pid $vm_pid $swap_pid $mp_pid $net_pid $vx_sabvcFM_pid $vx_WROSCV_pid $symmdg_sabvcFM_pid $symmdg_WROSCV_pid $beadg_sabvcFM_pid $beadg_WROSCV_pid
ps -ef > $save_dir/ps_end
cat `ls -t /var/adm/pacct*` | hisps -s `cat $save_dir/start_time` -e `cat $save_dir/end_time` > $save_dir/pacct.txt
netstat -k > $save_dir/kstat_end
netstat -m > $save_dir/strstat_end
#
# Faffing around to get the capture to shut down. $cap_pid doesn't seem to
# be the right one
#
mypid=$$
ps -ef | nawk "\$3 == $mypid { print \$2 }
/s[l]eep 50400/ { print \$2}
/s[n]oop / { print \$2}" | xargs kill -15
return
}
# ************************************************
# Function to ready the results for our standard Microsoft Office macros.
#
office_ise() {
cur_dir=`pwd`
cd $save_dir
start_time=`cat start_time`
end_time=`cat end_time`
cat /dev/null >excel.txt
#
# Process Accounting
#
if [ -f ps_beg -a -f ps_end -a -f pacct.txt ]
then
    sarprep -p ps_beg ps_end pacct.txt
    echo "RESOUT	resout.txt	resout_$pid.xls" >>excel.txt
fi
#
# Produce the sar output
#
if [ -f sad ]
then
cat /dev/null > stats
for i in u b d y c w a q v m p g r k l
do
   sar -$i -f sad >> stats
done 
sarprep stats
rm sar_td.txt
echo "SAR_DSK	sar_dsk.txt	dsk_$pid.xls" >>excel.txt
echo "SAR_CPU	sar_cpu.txt	cpu_$pid.xls" >>excel.txt
echo "SAR_MEM	sar_mem.txt	mem_$pid.xls" >>excel.txt
echo "SAR_SCR	sar_scr.txt	scr_$pid.xls" >>excel.txt
echo "SAR_TTY	sar_tty.txt	tty_$pid.xls" >>excel.txt
echo "SAR_RUNQ	sar_runq.txt	runq_$pid.xls" >>excel.txt
fi
#
# Produce the iostat output
#
#if [ -f ioout ]
#then
#sarprep -t 1200 -s $start_time -b ioout
#echo "IOOUT	ioout.txt	ioout_$pid.xls" >>excel.txt
#echo "IOCPU	iocpu.txt	iocpu_$pid.xls" >>excel.txt
#fi
#
# Produce the mpstat output
#
if [ -f mpout ]
then
    sarprep -t 1200 -s $start_time -m mpout
    echo "MPSTAT	mpout.txt	mpstat_$pid.xls" >>excel.txt
fi
#
# Produce the netstat output
#
if [ -f netout ]
then
    sarprep -t 1200 -s $start_time -n netout
fi
#
# Produce the vmstat output
#
if [ -f vmout ]
then
    sarprep -t 1200 -s $start_time -v vmout
    echo "IOCPU	vmout.txt	vmcpu_$pid.xls" >>excel.txt
    echo "SAR_SCR	vmscr.txt	vmscr_$pid.xls" >>excel.txt
    echo "SAR_RUNQ	vmrunq.txt	vmrunq_$pid.xls" >>excel.txt
fi
cd $cur_dir
return
}
# ****************************************************************************
# Main program starts here
# ****************************************************************************
# We only want one of these things running, so kill off any others.
#
mypid=$$
ps -ef | nawk "/e2[r]oot/ { if (\$2 != $mypid && \$3 != $mypid) print \$2 }
/s[l]eep 50400/ { print \$2}
/s[n]oop / { print \$2 }" | xargs kill -15
if [ "$1" = stop ]
then
    exit
fi
cd $PATH_HOME/unix
nxt=`cat nxtid.dat`
if [ -z "$nxt" ]
then
    lst=0
    nxt=1
else
    lst=$nxt
    nxt=`expr $nxt + 1`
    if [ "$nxt" -gt 30 ]
    then
	nxt=1
    fi
fi
echo $nxt > nxtid.dat
if [ -d save.$nxt ]
then
    rm -rf save.$nxt
fi
pid=$nxt
day=`date +%Y_%m_%d`
begin_stats $pid
trap "end_stats; office_ise" 15
sleep 50400
end_stats
office_ise
cd $PATH_HOME
tar cf unix_summary_${pid}_$day.tar unix/save.$pid/*.txt
tar cf netmon_summary_${pid}_$day.tar netmon/save.$pid/*.txt
bzip2 unix_summary_${pid}_$day.tar netmon_summary_${pid}_$day.tar
exit
