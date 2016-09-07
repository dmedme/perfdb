#!/bin/ksh
# detmon.sh - run system monitors appropriate for this system
# ************************************************
.  /cedar/e2/path_web/fdvars.sh
E2_HOME=/cedar/e2
export E2_HOME
cd $E2_HOME/monscripts/perfdb
# ************************************************
# Functions to manage the monitoring
# ************************************************
# Start
begin_stats() {
runref=$1
pid=$2
export pid
save_dir=$E2_HOME/monscripts/unix/save.$runref.$pid
export save_dir
mkdir $save_dir
chmod 0777 $save_dir
/usr/lib/acct/accton
cat /dev/null >$save_dir/pacct
/usr/lib/acct/accton $save_dir/pacct
tosecs >$save_dir/start_time
/usr/lib/sa/sadc 60 120 $save_dir/sad &
sad_pid=$!
vmstat 60 > $save_dir/vmout &
vm_pid=$!
iostat -xct 60 > $save_dir/ioout &
io_pid=$!
mpstat 60 > $save_dir/mpout &
mp_pid=$!
ps -ef > $save_dir/ps_beg
while :
do
    /usr/sbin/swap -s
    sleep 60
done > $save_dir/swap_stats &
swap_pid=$!
netstat -k > $save_dir/kstat_beg
netstat -m > $save_dir/strstat_beg
netstat -i -I bge1 60 >$save_dir/netout &
net_pid=$!
#su - oracle -c $E2_HOME/monscripts/perfdb/e2monitor.sh >/dev/null 2>&1 &
export sad_pid vm_pid io_pid swap_pid mp_pid net_pid
return
}
# ************************************************
# Function to terminate the monitors
#
end_stats() {
tosecs >$save_dir/end_time
kill -15 $sad_pid $io_pid $vm_pid $swap_pid $mp_pid $net_pid $cap_pid $lock_pid
sleep 1
kill -9 $sad_pid $io_pid $vm_pid $swap_pid $mp_pid $net_pid $cap_pid $lock_pid
ps -ef > $save_dir/ps_end
hisps -s `cat $save_dir/start_time` -e `cat $save_dir/end_time` <$save_dir/pacct > $save_dir/pacct.txt
netstat -k > $save_dir/kstat_end
netstat -m > $save_dir/strstat_end
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
    echo "RESOUT	q:\\resout.txt	q:\\resout_${runid}_$pid.xls" >>excel.txt
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
echo "SAR_DSK	q:\\sar_dsk.txt	q:\\dsk_${runid}_$pid.xls" >>excel.txt
echo "SAR_CPU	q:\\sar_cpu.txt	q:\\cpu_${runid}_$pid.xls" >>excel.txt
echo "SAR_MEM	q:\\sar_mem.txt	q:\\mem_${runid}_$pid.xls" >>excel.txt
echo "SAR_SCR	q:\\sar_scr.txt	q:\\scr_${runid}_$pid.xls" >>excel.txt
echo "SAR_TTY	q:\\sar_tty.txt	q:\\tty_${runid}_$pid.xls" >>excel.txt
echo "SAR_RUNQ	q:\\sar_runq.txt	q:\\runq_${runid}_$pid.xls" >>excel.txt
fi
#
# Produce the iostat output
#
if [ -f ioout ]
then
sarprep -t 60 -s $start_time -b ioout
echo "IOOUT	q:\\ioout.txt	q:\\ioout_${runid}_$pid.xls" >>excel.txt
echo "IOCPU	q:\\iocpu.txt	q:\\iocpu_${runid}_$pid.xls" >>excel.txt
fi
#
# Produce the mpstat output
#
if [ -f mpout ]
then
    sarprep -t 60 -s $start_time -m mpout
    echo "MPSTAT	mpout.txt	mpstat_${runid}_$pid.xls" >>excel.txt
fi
#
# Produce the netstat output
#
if [ -f netout ]
then
    sarprep -t 60 -s $start_time -n netout
    echo "NETOUT	q:\\netout.txt	q:\\netout_${runid}_$pid.xls" >>excel.txt
fi
#
# Produce the vmstat output
#
if [ -f vmout ]
then
    sarprep -t 60 -s $start_time -v vmout
    echo "IOCPU	q:\\vmout.txt	q:\\vmcpu_${runid}_$pid.xls" >>excel.txt
    echo "SAR_SCR	q:\\vmscr.txt	q:\\vmscr_${runid}_$pid.xls" >>excel.txt
    echo "SAR_RUNQ	q:\\vmrunq.txt	q:\\vmrunq_${runid}_$pid.xls" >>excel.txt
fi
cd $cur_dir
return
}
# ****************************************************************************
# Main program starts here
#
runid=ros-efin-v34
pid=1
while :
do
    if [ -d $E2_HOME/monscripts/unix/save.$runid.$pid ]
    then
        pid=`expr $pid + 1`
    else
        break
    fi
done
export runid
begin_stats $runid $pid
sleep 7200
end_stats
office_ise
