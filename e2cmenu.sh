#!/bin/sh
# e2cmenu.sh - Master Administrative Menu for e2com
# @(#) $Name$ $Id$
# Copyright (c) E2 Systems Limited 1993
#
# Hacked from:
# natmenu.sh - master administrative menu for supernat
# Copyright (c) E2 Systems, 1990
#
# This file contains definitions of Menus intended to be useful
# whilst administering e2com.
#
# It relies on the presence of natmenu, the menu driver for the E2
# Systems SUPERNAT Product.
#
# The order of items is important
# - execution always starts with the first item
# - forward references to return menus are not allowed
#
# ##################################################################
#
trap 'echo "Abort Command Given"
exit 0' 1 2 3
#
# Initialise the environment and define service routines
#
. e2cserv.sh
e2com_ini_env
#
# Get the base directory for e2com
#
BASE_DIR=`sed 's/SB[| :]\([^:| ]*\).*$/\1/' e2com_base`
export BASE_DIR
# **************************************************************************
# Function to allow a user to select from a list of files
# Arguments:
# 1 - The File Selection Header
# 2 - The File Selection String
#
file_select () {
FILE_LIST=`(
echo HEAD=$1 
echo PROMPT=Select Files, and Press Return
echo SEL_YES/COMM_NO/SYSTEM
echo SCROLL
ls -lt $2 2>&1 | sed '/\// s=\(.* \)\([^ ]*/\)\([^/]*\)$=\1\3/\2\3=
/\//!{
s=.* \([^ ]*\)$=&/\1=
}'
echo
) | natmenu 3<&0 4>&1 </dev/tty >/dev/tty`
if [ "$FILE_LIST" = " " -o "$FILE_LIST" = "EXIT:" ]
then 
    FILE_LIST=""
fi
export FILE_LIST
return
}
# **************************************************************************
#
# Function to allow a user to select the action to apply to selected files
#
# Arguments:
# 1 - The Action Selection Header
#
action_select () {
ACTION=` natmenu 3<<EOF 4>&1 </dev/tty >/dev/tty
HEAD=$1 
PROMPT=Select Action, and Press Return
SEL_YES/COMM_NO/MENU
SCROLL
BROWSE: View the Selected Items/BROWSE:
PRINT : Print the Selected Items/PRINT:
DELETE: Delete the Selected Items/DELETE:
EXIT  : Exit without doing anything/EXIT:

EOF
`
export ACTION
return
}
# **************************************************************************
#
# Function to start up e2com with some sensible options
#
# Arguments:
# 1 - The Action Selection Header
#
e2com_start () {
e2com_shut
rm -f $BASE_DIR/e2com_fifo $BASE_DIR/e2com_fifo.lck
options=` natmenu 3<<EOF 4>&1 </dev/tty >/dev/tty
HEAD=START:    Start the E2COM Server 
PARENT=APPL:
PROMPT=Fill in the desired fields and then Execute
COMMAND=START:
SEL_NO/COMM_NO/SYSTEM
SCROLL
Enter the DEBUG level (0-4)

EOF
`
set -- $options
if [ $# = 0 ]
then 
    return
else
    shift
    if [ $# = 0 ]
    then 
        debargs=""
    else
        debargs=-d$1
    fi
    e2com_launch $debargs > $BASE_DIR/e2com_err$$ 2>&1
    e2cap_launch >$BASE_DIR/e2cap_err$$ 2>&1
fi
echo 'Waiting for start up...'
sleep 5
get_server_pids
get_e2com_pid
if [ -z "$E2COM_PID" ]
then
#
# Problem; FIFO still exists, but no process!
#
echo Problem - FIFO still exists, but no process!
echo $SERVER_PIDS
sleep 5
rm -f $BASE_DIR/e2com_fifo
STATE="E2COM is DOWN"
E2COM_PID=""
else
STATE="E2COM is UP"
fi
export STATE E2COM_PID
return
}
# **************************************************************************
#
# Function to select a printer
#
# Arguments:
# 1 - The user
#
printer_select () {
THISUSER=`lpstat -a| awk '/accepting/ { print $1 }'`
set -- $THIS_USER
#
#
# If there is only one printer in UDGA.User.permissions then use it
# otherwise ask for printer.
#
if [ $# = 1 ]
then
	# Only one printer selection so use that printer
    CHOICES=$1
else
CHOICES=`(
echo HEAD=The Printer to send to 
echo PROMPT=Select Printer, and Press Return
echo SEL_YES/COMM_NO/MENU
echo SCROLL
for i in $*
do
    echo $i/$i
done
echo
) | natmenu 3<&0 4>&1 </dev/tty >/dev/tty `
fi
export CHOICES
return
}
# **************************************************************************
#
# Function to allow browsing of files, with no SHELL escape
# Arguments:
# 1 - List of files to browse
#
file_browse () {
BROWSE_FILES=$1
export BROWSE_FILES
(
savterm=$TERM
TERM=vt220-w
export TERM
tput rs1
SHELL=/bin/true
export SHELL
pg $BROWSE_FILES
TERM=$savterm
export TERM
tput rs1
)
return
}
# **************************************************************************
#
# Function to reprocess the file
# Arguments
# 1 - The program to run
# 2 - The File Selection Header
# 3 - The File Selection String
#
reprocess() {
file_select "$2" "$3"
for i in $FILE_LIST
do
(
SHELL=/bin/true
export SHELL
$1 e2com_base $i 2>&1 | pg
)
done
return
}
get_server_pids
if check_e2com_run
then
STATE="E2COM is UP"
get_e2com_pid
if [ -z "$E2COM_PID" ]
then
#
# Problem; FIFO still exists, but no process!
#
echo Problem - FIFO still exists, but no process!
echo $SERVER_PIDS
sleep 5
rm -f $BASE_DIR/e2com_fifo
STATE="E2COM is DOWN"
E2COM_PID=""
fi
else
STATE="E2COM is DOWN"
E2COM_PID=""
fi
export STATE E2COM_PID
# **************************************************************************
#
# Main Processing; loop following User Instructions until
# exit requested
#
while true
do
NOW=`date`
export NOW
opt=`natmenu 3<<EOF 4>&1 </dev/tty >/dev/tty
HEAD=MAIN:  Management ($STATE at $NOW)
PROMPT=Select Menu Option
SEL_YES/COMM_YES/MENU
SCROLL
COMMS:     Communications Link Control/COMMS:
TRACE:     Inspection of Diagnostics/TRACE:
APPL:      Manually Initiate Application Features/APPL:
PURGE:     Purge E2COM History/PURGE:
EXIT:      Exit/EXIT:

HEAD=COMMS: Communications Link Control 
PARENT=MAIN:
PROMPT=Select Menu Option
SEL_YES/COMM_YES/MENU
SCROLL
COMMSHELP: Communications Link Help/COMMSHELP:
START:     Start the E2COM server/START:
CLINK1:    Close the Link/CLINK1:
STOP:      Shut down the E2COM Server/STOP:
INFO:      Dump some internal data to the Error Output/INFO:
OFF:       Set Debug Level to 0 (Off, the initial state)/OFF:
DEBUG1:    Set Debug Level to 1 (A few extra diagnostics)/DEBUG1:
DEBUG2:    Set Debug Level to 2 (Most function calls are traced)/DEBUG2:
DEBUG3:    Set Debug Level to 3 (Amost everything)/DEBUG3:
DEBUG4:    Set Debug Level to 4 (Absolutely everything)/DEBUG4:
OLINK4:    Open the Loop-back (Test) Link/OLINK4:
REINIT:    Re-initialise all Control Structures/sqlplus -s perfmon/perfmon @pdb
EXIT:      Return/EXIT:

HEAD=TRACE: Inspect the System Status 
PROMPT=Select Menu Option
PARENT=MAIN:
SEL_YES/COMM_YES/MENU
SCROLL
TRACEHELP: E2COM Status Help/TRACEHELP:
PROC:      View the E2COM Processes/oneproc \\\`echo \$SERVER_PIDS|sed 's/[A-Z_]*=//g'\\\`;ls -lt $BASE_DIR/*_fifo 
ACT:       Inspect the Activity Log/cat $BASE_DIR/Activity
ERROR:     Inspect the Error Output/cat \\\` ls -t $BASE_DIR/*_err* | sed -n '1,2 p' \\\`
SOCK:      View E2COM socket status/netstat -f inet -a | grep e2com
MESS:      Inspect Message Records or Static Parameters/MESS:
FLIST:     List Incoming Files and FTP logs/ls -lot $BASE_DIR/R*
ELIST:     List E2COM Error Files/ls -lot $BASE_DIR/*_err*
FVIEW:     Inspect Files/FVIEW:
EXIT:      Return/EXIT:

HEAD=APPL: Application Facilities 
PARENT=MAIN:
PROMPT=Select Menu Option
SEL_YES/COMM_YES/MENU
SCROLL
APPLHELP:  E2COM Application Help/APPLHELP:
PRINT:     Print E2COM Print files/PRINT:
SUBMIT:    Feed a File to the Link/SUBMIT:
PICK:      Feed Files to the Link picked from the Directory/PICK:
TRANS:     Feed a File of HISS Transactions through the Loop-back/TRANS:
EXIT:      Return/EXIT:

HEAD=MESS: Inspect System Status
PARENT=MAIN:
PROMPT=Select Menu Option
SEL_YES/COMM_YES/MENU
SCROLL
BASE:      E2COM Configuration Data/cat e2com_base
LINK:      E2COM Link Statuses/cat e2com_control
TYPES:     E2COM Recognised Data Types/cat e2com_mess
JOURNAL:   E2COM Messages/cat e2com_journal
EXIT:      Return/EXIT:

HEAD=PURGE: Purge E2COM History 
PARENT=MAIN:
PROMPT=Select Menu Option
SEL_YES/COMM_YES/MENU
SCROLL
PURGHELP:  E2COM Purge Help/PURGHELP:
PURGEM:    Purge Messages and Files/PURGEM:
PURERR:    Purge Non-Current Error Files:/cd $BASE_DIR; rm \\\` ls -t e2com_err*  | sed '1,2 d' \\\`
TRUNCACT:  Truncate the Activity Log/cd $BASE_DIR; mv Activity a$$.tmp; tail a$$.tmp > Activity ; rm a$$.tmp
EXIT:      Return/EXIT:

HEAD=COMMSHELP: Communications Link Help 
PARENT=COMMS:
PROMPT=Any Command to Return
SEL_YES/COMM_YES/MENU
SCROLL
START:     Start the E2COM server/NULL:
   This option shuts down the E2COM Server if it is running,/NULL:
   by sending '-s 0' to the FIFO, and then restarts it./NULL:
CLINK1:    Close the Link/NULL:
   If the E2COM server is running, this option shuts down any/NULL:
   connexion to it by sending '-s 1' to the FIFO./NULL:
   It is assumed that all links will be established through Link 1./NULL:
STOP:      Shut down the E2COM Server/NULL:
   This option shuts down the E2COM Server if it is running,/NULL:
   by sending '-s 0' to the FIFO. If this does not work:/NULL:
   -  Use ps to identify any still-running e2com-related processes/NULL:
      These could potentially be:/NULL:
      -   e2com/NULL:
      -   ftp/NULL:
      -   e2cap/NULL:
      -   oracle/NULL:
   -  use 'kill' to get rid of them. kill -USR1 is preferred for/NULL:
      e2com; otherwise kill -15 or kill -9./NULL:
INFO:      Dump some internal data to the Error Output/INFO:
   Some internal status information, particularly the ftp userid and/NULL:
   password passed from the HISS, is dumped to the error output/NULL:
OFF:       Set Debug Level to 0 (Off, the initial state)/NULL:
DEBUG1:    Set Debug Level to 1 (A few extra diagnostics)/NULL:
DEBUG2:    Set Debug Level to 2 (Most function calls are traced)/NULL:
DEBUG3:    Set Debug Level to 3 (Amost everything)/NULL:
DEBUG4:    Set Debug Level to 4 (Absolutely everything)/NULL:/NULL:
   The communications master process reports events to the Activity/NULL:
   Log, and to stderr. The level of verbosity is switchable. These/NULL:
   options change that level, by passing -d level on the FIFO./NULL:
OLINK4:    Open the Loop-back (Test) Link/NULL:
   If e2com is running, this option opens the Loop Back connexion,/NULL:
   used for:/NULL:
   -  Standalone testing/NULL:
   -  Processing of transactions and data passed from the HISS on/NULL:
      diskette, in the event of a disastrous loss of communications/NULL:
      between the Pyramid and the HISS,/NULL:
   by sending '-c 4' to the FIFO. /NULL:
REINIT:    Re-initialise all Control Structures/NULL:
   This command completely resets the E2COM Oracle Control/NULL:
   structures, by executing/NULL:
      sqlplus -s perfmon/perfmon @pdb/NULL:
   It ignores any Operating System Files, which it is assumed may/NULL:
   need to be resubmitted manually./NULL:
   It is used during testing./NULL:
   USE WITH EXTREME CAUTION./NULL:

HEAD=TRACEHELP: Help on the Diagnostic facilities 
PARENT=TRACE:
PROMPT=Any Command to Return
SEL_YES/COMM_YES/MENU
SCROLL
PROC:      View the E2COM Processes/NULL:
   This menu option uses the oneproc command to view the/NULL:
   current trees of processes rooted with e2com, and the other/NULL:
   related server processes: e2cap/NULL:
   It also lists the FIFOs that are used to communicate with them./NULL:
   Servers or users will hang if the FIFOs exist, but the servers do not./NULL:
ACT:       Inspect the Activity Log/NULL:
   This menu option uses the oneproc command to view the/NULL:
   current tree of processes rooted with E2COM./NULL:
ERROR:     Inspect the Error Output/NULL:
   This allows inspection of the latest error file./NULL:
SOCK:      View E2COM sockets/NULL:
   This menu option executes netstat to enable/NULL:
   inspection of e2com communication sockets./NULL:
MESS:      Inspect or Change Message Records, Message Types,/NULL:
           Link Definitions and the Base Parameters/NULL:
   This menu option enables inspection of Communications messages,/NULL:
   Link definitions and certain Configuration options./NULL:
   By playing with message statuses, it is possible to cause/NULL:
   E2COM to:/NULL:
   -   Ignore some messages/NULL:
   -   Process messages twice./NULL:
   READ THE MANUAL BEFORE CHANGING THESE. FOR BEST RESULTS,/NULL:
   ONLY CHANGE WHEN THE E2COM SERVER IS NOT RUNNING./NULL:
FLIST:     List Incoming Files and FTP logs/NULL:
   This option uses ls to identify the incoming files, sizes, and/NULL:
   the corresponding ftp dialog output./NULL:
FVIEW:     Browse E2COM Print Files/NULL:
   This option allows the user to browse any file in the E2COM Directory/NULL:
PRINT:     Print E2COM Print Files/NULL:
   This option allows the user to select an print various E2COM files/NULL:

HEAD=APPLHELP: Help on E2COM Application Facilities 
PARENT=APPL:
PROMPT=Any Command to Return
SEL_YES/COMM_YES/MENU
SCROLL
FAILED:    List Failed HISS Files/NULL:
   This option lists receive files that have not been/NULL:
   successfully processed./NULL:
SUBMIT:    Feed a File to the Link/NULL:
   This option enables an operator to identify:/NULL:
   -   A submission link (1 to feed to the HISS; 4 to feed a loop back/NULL:
       file obtained on a Floppy Disk)/NULL:
   -   A File Type (SP, Performance Data)/NULL:
   -   A list of file names to process/NULL:
   See the e2com_mess file for a current list of incoming types./NULL:
PICK:      Feed Files to the Link picked from the directory/NULL:
TRANS:     Feed a file of Transaction to E2COM/NULL:
   This option submits files of transactions through the loop/NULL:
   back. Provide the name of the file./NULL:

HEAD=PURGHELP: E2COM Purge Help 
PARENT=PURGE:
PROMPT=Any Command to Return
SEL_YES/COMM_YES/MENU
SCROLL
PURGE:  Purge Fully Processed Received Files/NULL:
    This menu option  removes all incoming files that are marked in the/NULL:
    e2com_journal table as having been successfully processed./NULL:
    and removes all outgoing files whose sequence numbers/NULL:
    are less than the 'earliest_out_unread' values for their/NULL:
    respective links (ie. all files that have been transferred). /NULL:
    and removes successfully processed incoming messages/NULL:
    whose sequence numbers are less than the 'last_in_rec' values for their/NULL:
    respective links./NULL:
    and all incoming messages whose sequence numbers/NULL:
    are less than the 'last_in_rec' values for their respective links, without/NULL:
    regard for SUCCESS or FAILURE./NULL:
    and removes all outgoing messages whose sequence numbers/NULL:
    are less than the 'earliest_out_unread' values for their respective links,/NULL:
    without regard for SUCCESS or FAILURE./NULL:
PURERR:    Purge Non-Current Error Files/NULL:
    This option uses a piece of shell to remove all but the latest/NULL:
    E2COM stdout-stderr file (starting from this menu is assumed)/NULL:
TRUNCACT:  Truncate the Activity Log/NULL:
    This option uses a piece of shell to remove all but the last 10 lines/NULL:
    from the Activity file./NULL:

HEAD=SUBMIT:    Feed a File to the Link 
PARENT=APPL:
PROMPT=Fill in the desired fields and then Execute
COMMAND=SUBMIT:
SEL_NO/COMM_NO/SYSTEM
SCROLL
Link 
File Type (always SP)
Filename (as many as desired)

HEAD=TRANS:     Feed a File of HISS Transactions through the Loop-back 
PARENT=APPL:
PROMPT=Fill in the desired fields and then Execute
COMMAND=TRANS:
SEL_NO/COMM_NO/SYSTEM
SCROLL
Filename (as many as desired)

EOF
`
set -- $opt
#
# Process the commands that cannot be handled directly from within
# the menu handler
#
case $1 in
FVIEW:)
file_select "Pick files to browse (do not pick the named pipes)"  "$BASE_DIR/*"
if [ "$FILE_LIST" != "" ]
then
    file_browse "$FILE_LIST"
fi
;;
PRINT:)
file_select "Pick files to print" "$BASE_DIR/*.lis"
if [ "$FILE_LIST" != "" ]
then
    printer_select $LOGNAME
    for i in $FILE_LIST
    do
        lp -d$CHOICES < $i
    done 
fi
;;
START:)
#    Start the E2COM server
e2com_start
;;
PICK:)
file_select "PICK: List and Select Files for Resubmission" "$BASE_DIR/*"
if [ "$FILE_LIST" != ""  ]
then
for i in $FILE_LIST
do
if [ ! -z "$E2COM_PID"  -a "$FILE_LIST" != "EXIT:" ]
then
    e2com_command -t SP -l 1 -f $i
fi
done
fi
;;
CLINK1:)
#   Close the Link
if [ ! -z "$E2COM_PID" ]
then
    e2com_command -s 1
fi
;;
STOP:)
#     Shut down the E2COM Server
e2com_shut
;;
INFO:)
    e2com_command -i
;;
OFF:)
    e2com_command -d 0
;;
DEBUG1:)
    e2com_command -d 1
;;
DEBUG2:)
    e2com_command -d 2
;;
DEBUG3:)
    e2com_command -d 3
;;
DEBUG4:)
    e2com_command -d 4
;;
OLINK4:)
#   Open the Loop-back (Test) Link
    e2com_command -c 4
;;
FAILED:)
#   List Failed File Transfers
(
SHELL=/bin/true
export SHELL
nawk -F: '/^SJ/ {
st =  $6
getline
if (st != "D" && $1 == "46")
    print $3
}' | pg
)
;;
PURGEM:)
#    Purge Fully Processed Elements
e2com_shut
e2com_journal_purge
;;
SUBMIT:)
#
# Process a File Submission
#
shift
if [ ! -z "$1" ]
then
    link=$1
    shift
    if [ ! -z "$1" ]
    then
        type=$1
        shift
        for i in $*
        do
        e2com_command -f $i -t $type -l $link
        done
    fi
fi
;;
TRANSMIT:)
#
# Process a set of files to be submitted directly (probably for
# the loop-back)
#
shift
cat $* > e2com_fifo
;;
EXIT:)
break
;;
*)
echo "Unrecognised Options..." $opt
sleep 5
exit
;;
esac
done
exit
