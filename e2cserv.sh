#!/bin/sh
#
# e2cserv.sh - Common Administrative Functions, defined here so that
# they can be invoked from within menus, or by batch jobs.
# ************************************************************************
#
# Make sure that the binaries etc. are accessible
#
e2com_ini_env() {
E2COM_HOME=${E2COM_HOME:-/users/home/path}
case $PATH in
*$E2COM_HOME/perfbin*)
    ;;
*)
    PATH=$E2COM_HOME/perfbin:$PATH
    export PATH
    ;;
esac
#
# Initialise the environment
#
    E2CAP_FIFO=$E2COM_HOME/test/e2cap_fifo
    export E2CAP_FIFO
    cd $E2COM_HOME/test
    return
}
# ************************************************************************
#
# Launch the database capture program
#
e2cap_launch() {
    e2cap perfmon/perfmon $E2CAP_FIFO &
    E2CAP_PID=$?
    export E2CAP_PID
    return
}
# ************************************************************************
#
# Launch the Communication program 
#
e2com_launch() {
while :
do
    e2com e2com_base e2com.log $* &
    E2COM_PID=$?
    export E2COM_PID
    cnt=0
    until [ -p e2com_fifo ]
    do
        echo Waiting for startup....
        sleep 5
        cnt=`expr $cnt + 1`
        if [ $cnt -gt 20 ]
        then
            break
        fi
    done
    if [ $cnt -lt 21 ]
    then
        break
    fi
done
#
# Break the lock, if there is one.
#
rm -f e2com_fifo.lck
#
# Make sure that the necessary connections are established.
e2sub -c 4 -n e2com_fifo
e2sub -c 3 -n e2com_fifo
e2sub -c 2 -n e2com_fifo
    return
}
# ************************************************************************
#
# Function to check that the FIFO exists
#
check_e2com_run () {
if [ ! -p $BASE_DIR/e2com_fifo  ]
then
    return 1
else
    return 0
fi
}
# ************************************************************************
#
# Function to submit a command to e2com
#
e2com_command () {
if check_e2com_run
then
echo "Issuing E2COM Command......" $*
#
# Issue the request
#
    e2sub $* -n e2com_fifo
    return 0
else
    return 1
fi
}
# ************************************************************************
#
# Function to shut down E2COM, if it seems to be running
#
e2com_shut() {
if [ ! -z "$E2COM_PID" ]
then
    e2com_command -s 0
fi
}
# ************************************************************************
#
# Shift the calendar forwards, then delete the performance database records
# that have slipped out of the window, i.e. any that are older than the oldest
# record of the appropriate type.
#
# The records that are deleted have been copied to pdb_purge, and can be
# preserved if desired.
#
e2com_adjust_calendar() {
sqlplus perfmon/perfmon << EOF
declare
procedure pdb_update_calendar
is
    dshift number;
begin
/*
 * Roll-forward slots
 */
    select (trunc(sysdate)+2) - max(start_time) - 1/72 into dshift
    from pdb_calendar
    where data_type = 'S';
    update pdb_calendar set start_time = start_time + dshift
    where data_type = 'S';
/*
 * Roll forward days
 */
    select (trunc(sysdate)+2) - max(start_time)  into dshift
    from pdb_calendar
    where data_type = 'D';
    update pdb_calendar set start_time = start_time + dshift
    where data_type = 'D';
/*
 * Roll-forward weeks
 */
    select trunc(next_day(sysdate, 'MONDAY')) - max(start_time) into dshift
    from pdb_calendar
    where data_type = 'W';
    update pdb_calendar set start_time = start_time + dshift
    where data_type = 'W';
/*
 * Roll-forward months
 */
    select add_months(trunc(sysdate, 'MON'),2)  - max(start_time) into dshift
    from pdb_calendar
    where data_type = 'M';
    update pdb_calendar set start_time = start_time + dshift
    where data_type = 'M';
    commit;
end pdb_update_calendar;
begin
    pdb_update_calendar;
end;
/
drop table pdb_purge;
create table pdb_purge
as select * from pdb_data
where 0 = 1;
declare
procedure pdb_purge_data ( ttype in char ) is
    ddate date;
begin
    select min(b.start_time)
      into ddate
      from pdb_calendar b 
      where b.data_type = ttype;
    loop
        insert into pdb_purge
          select * from pdb_data a
          where a.time_type = ttype
          and a.start_time < ddate;
        if SQL%NOTFOUND or (SQL%ROWCOUNT < 6999)
        then
        commit;
        set transaction use rollback segment r01;
            exit;
        end if;
        commit;
        set transaction use rollback segment r01;
    end loop;
    loop
        delete pdb_data a
        where a.time_type = ttype
          and a.start_time < ddate;
        if SQL%NOTFOUND or (SQL%ROWCOUNT < 6999)
        then
            commit;
            set transaction use rollback segment r01;
            exit;
        end if;
        commit;
        set transaction use rollback segment r01;
    end loop;
end pdb_purge_data;
begin
    set transaction use rollback segment r01;
    pdb_purge_data('S');
    pdb_purge_data('D');
    pdb_purge_data('W');
    pdb_purge_data('M');
end;
/
exit;
EOF
    return
}
# ************************************************************************
#
# Now purge the journal
#
e2com_journal_purge() {
#
# Get rid of files
#
nawk -F: 'BEGIN { while((getline<"e2com_control")>0)
    {
       link=$2 + 0
       tlow[ link ] = $12
       rlow[ link ] = $11
    }
}
/^SJ/ {
st =  $6
link = $2
seq = $3
if (st == "D" && $4 == "R" && substr($9,1,1) != " ")
{
    print "rm -f " $9
    getline
}
else
{
    getline
    if ((st == "D" || st == "S") && $1 == "46" && seq < tlow[link] )
        print "rm -f " $3
}
}' e2com_journal | sh
#
# Slim down the journal file
#
nawk -F: 'BEGIN { while((getline<"e2com_control")>0)
    {
       link=$2 + 0
       tlow[ link ] = $12
       rlow[ link ] = $11
    }
}
{
#
# Expect SJ/Other pairs.
#
    link = $2 + 0
    seq = $3 + 0
    tr = $4
    if ((tr == "R" && seq > rlow[link]) ||(tr == "T" && seq > tlow[link]))
    {
        if (tr == "R")
        {
            if (substr($9,1,1) == " " || !system("test -f " $9))
            {
                print $0
                getline
                print $0
            }
            else
                getline
        }
        else
        {
            sav = $0
            getline
            if ($1 != "46" || !system("test -f " $3))
            {
                print sav
                print $0
            }
        }
    }
    else
        getline
}' e2com_journal > e2com_journal.new
#
# Save the pre-purge journal, and keep a copy of the post-purge file
mv e2com_journal e2com_journal.sav
cp e2com_journal.new e2com_journal
#
# The previous redo log is not now useful, but we are keeping it to help
# with diagnosing a system problem.
mv e2com.log e2com.log.$$
return
}
# ************************************************************************
#
# Find out initially which servers are running
# 
get_server_pids () {
SERVER_PIDS=`ps -edaf | awk 'BEGIN {
e2cap_pid=99999
e2com_pid=99999
}
/e2cap/ {
if ( $2 < e2cap_pid)
    e2cap_pid = $2
}
/e2com/ {
if ( e2com_pid == 99999)
{
    e2com_pid = $2
}
}
END {
if (e2cap_pid != 99999)
    printf "%s ", "E2CAP_PID=" e2cap_pid
if (e2com_pid != 99999)
    printf "%s ", "E2COM_PID=" e2com_pid
}'`
export SERVER_PIDS
return;
}
# ************************************************************************
#
# Find out the PID for e2com
# 
get_e2com_pid () {
E2COM_PID=`echo $SERVER_PIDS | sed -n '/E2COM_PID/ {
s/.*E2COM_PID=\([1-9][0-9]*\).*/\1/
p
}'`
export E2COM_PID
return
}
# ************************************************************************
#
# Find out the PID for e2cap
# 
get_e2cap_pid () {
E2CAP_PID=`echo $SERVER_PIDS | sed -n '/E2CAP_PID/ {
s/.*E2CAP_PID=\([1-9][0-9]*\).*/\1/
p
}'`
export E2CAP_PID
return
}
