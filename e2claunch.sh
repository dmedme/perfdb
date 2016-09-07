#!/bin/sh
#
# e2claunch.sh - Kick off the Performance Database Programs
#
# Make sure that the binaries are accessible
#
# Once SUPERNAT is properly set up, it should be a SUPERNAT job.
#
set -x
. e2cserv.sh
e2com_ini_env
myhost=`uname -n`
trycnt=10
#
# Do the cleanup.
#
while :
do
get_server_pids
get_e2com_pid
get_e2cap_pid
# ***************************************************************************
# First, the database program
#
case "$myhost" in
*case*)
#
# Shut down the database capture program
#
# Lock out file submission whilst we are sorting out the
# database. If we cannot get in in 10 goes, something must be wrong;
# use force.
#
    i=0
    until semlock -s $E2CAP_FIFO.lck
    do
        echo "Waiting to lockout Capture sub-system......"
        sleep 10
        i=`expr $i + 1`
        if [ $i -gt $trycnt ]
        then
            ps -e | awk '/e2sub/ { print "kill -9 " $1 }' | sh
            rm -f $E2CAP_FIFO.lck        
        fi
    done
    get_e2cap_pid
    if [ -n "$E2CAP_PID" ]
    then
        kill -USR1 $E2CAP_PID
    fi
    rm -f $E2CAP_FIFO
#
    e2com_adjust_calendar
#
    ;;
*)
    ;;
esac
sleep 5
#
# Kill them all regardless
#
ps -e | awk '/e2cap/ { print "kill -9 " $1 }' | sh
#
# Now shut down the communications process
#
if [ -n "$E2COM_PID" ]
then
    e2sub -s 0 -n e2com_fifo &
    i=0
    while [ -p e2com_fifo ]
    do
        echo "Waiting to shut down communications sub-system......"
        sleep 10
        i=`expr $i + 1`
        if [ $i -gt $trycnt ]
        then
            rm -f e2com_fifo
        fi
    done
else
    rm -f e2com_fifo e2com_fifo.lck
fi
#
i=0
until semlock -s e2com_fifo.lck
do
    echo "Waiting to lockout communication sub-system......"
    sleep 10
    i=`expr $i + 1`
    if [ $i -gt $trycnt ]
    then
        ps -e | awk '/e2sub/ { print "kill -9 " $1 }' | sh
        rm -f e2com_fifo.lck
    fi
done
#
# Make sure it has gone
#
get_e2com_pid
if [ -n "$E2COM_PID" ]
then
    kill -USR1 $E2COM_PID
    sleep 5
fi
#
# Kill them all regardless
#
ps -e | awk '/e2com/ { print "kill -9 " $1 }' | sh
#
# Now purge the journal
#
cat /dev/null> e2com_fifo.lck
e2com_journal_purge
# ***************************************************************************
# Restart
#
case "$myhost" in
*case*)
#
# Launch the database capture program
#
    e2cap_launch > e2cap_err$$ 2>&1
    ;;
*)
    ;;
esac
#
# Launch the communications program
#
# Make sure we do not get address in use errors
#
sleep 60
e2com_launch > e2com_err$$ 2>&1
#
# Remove the locks to let others in
#
rm -f e2com_fifo.lck $E2CAP_FIFO.lck
#
# ***************************************************************************
# Watch dog code. Hang around here for 24 hours or so, or until we have lost
# the things we are minding.
#
get_server_pids
get_e2com_pid
get_e2cap_pid
if [ -n "$E2COM_PID" ]
then
    i=0
    while [ $i -lt 288 ]
    do
        sleep 300
        case "$myhost" in
        *case*)
            if ps -p "$E2COM_PID" | grep "$E2COM_PID"
            then
                if ps -p "$E2CAP_PID" | grep "$E2CAP_PID"
                then
                    :
                else
                    break
                fi
            else
                break
            fi
            ;;
        *)
            if [ ! -f /proc/$E2COM_PID ]
            then
                break
            fi
            ;;
        esac
        i=`expr $i + 1`
    done
fi
done
exit
