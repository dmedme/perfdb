 /***********************************************************************
 * e2com.c - Pyramid/SCO Master Communication Process
 * Copyright (c) E2 Systems 1995
 *
 * This program implements the Communications Master Process, specified
 * as follows.
 *
 * Communication Sub-system
 * ========================
 * Introduction
 * ============
 * The Network, in the following discussion, encompasses all the machines
 * that co-operate in order to service the NSHT HISS requirements.
 * 
 * This document discusses a general framework for Communication between
 * network nodes and re-synchronisation of systems after a machine rejoins
 * the network after a period in isolation (switched off, crashed or
 * simply disconnected). It is not mandatory; any two parties can
 * deviate from it by mutual agreement.
 * 
 * The following discussion covers a TCP/IP-based communication scheme.
 * 
 * General Ideas
 * =============
 * Communications and resynchronisation are co-ordinated by, on each
 * communicating node, a communications master process. This process
 * implements the 'e2com' service.
 * 
 * The following items need to be set up in the standard Internet
 * manner.
 *   - Hostnames of Peers (hold in /etc/hosts, and use gethostbyname()):
 *   - The service name:
 *     -  e2com 3001/tcp (hold in /etc/services, use getservbyname(),
 *                  getprotobyname())
 *     -  Note that we will also reserve 3002 to use for calling
 *     -  Note also that tcp (the protocol) will already be defined.
 *   - A username 'e2com' on each host, with .rhost entries to permit
 *     use of rcp (ie. password-less file transfer).
 * 
 * The Communications Control processes:
 *   - Communicate with each other a using small number of control  transactions
 *     order to co-ordinate the transport and processing of data files
 *   - And additionally service transactions sent by the SI to the Pyramid.
 * 
 * We would like all transactions to be:
 * - In clear ASCII text
 * - With no embedded nulls
 * 
 * Therefore, Data files would be human-readable
 * - line feeds at the end of each record
 * - Character filler (:,|, space or something) between each field
 * - No nulls
 * - We would Suggest mnemonic record types, to aid readability
 * - All trailer records should include a count of the number of items within
 *   the scope
 *  (*Why?
 *   - Very easy to dummy up
 *     - Valid test data
 *     - Various permutations of invalid test data
 *   - Our report generators will not embed nulls
 *   - The UNIX differential file comparator is line oriented (for
 *     comparing expected with actuals)
 *   - Do not need to write bespoke formatters or use the dump utilities.
 *  )
 * 
 * 
 * Normal Operation
 * ================
 * The e2com image is multi-threaded when servicing incoming files,
 * and single-threaded when processing:
 * -  Incoming transactions
 * -  Outgoing transactions.
 *
 * Application programs write data files with the data to be communicated;
 * when they have done so, they notify the Communications Control process
 * (on the Pyramid, through a named pipe, the mechanism we currently use for
 * our other products (Supernat).
 * 
 * Each transaction is allocated a transaction ID by the Communications
 * control process, with respect to the Communications end point,
 * and is logged in the Communications Journal.This will contain, for
 * each message
 * - Transaction ID
 * - Transmission/Reception indicator
 * - Status (Being created, SEND outstanding, RECEIVE outstanding,
 *   Being validated, Finished)
 * - Send File Name
 * - Receive File Name
 * - Message Data
 * - Success or failure
 * - Origin
 * - Destination
 * - Type
 * - Size
 * - Record count
 * 
 * The Control process maintains a Control Table, which holds
 *   - For each destination
 *     - Host Name (as per /etc/hosts)
 *     - Maximum simultaneous sessions
 *     - For out-going transactions
 *       - The Last transaction ID allocated
 *       - The Last transaction ID sent
 *     - For incoming messages and files
 *       - The Last transaction ID received.
 *     - Link up or down
 * 
 * If the Link is marked as Not Allowed to Call, the Control Process does
 * nothing.
 * 
 * If it is marked as Allowed to Call, The Communication Control Process
 * (makes contact with its peer, if no connexion exists, and)
 * sends any outstanding transactions.
 *
 * The main transaction between the Peers is a DATA message.
 * 
 * Data Messages are processed according to rules defined in the Configuration
 * files
 * 
 * - SEND FILE is a note to its recipient to the effect that the file is
 *   available for transfer.
 *   -  The request is logged in the journal at the receiving end,
 *      marked unfulfilled
 *   -  If the link was marked as down, there will be other
 *      unfulfilled items:
 *      -  Mark the link as up
 *      -  Attempt to accomplish delivery of same
 *   -  Physical delivery of a file is:
 *      -  Initiated by the recipient of the SEND FILE
 *      -  Accomplished by some form of remote copy:
 *         - rcp (if supported, otherwise ftp)
 *   - If the exit status of the rcp is non-zero (UNIX), the link is marked
 *     as down 
 *   - Otherwise:
 *     -  The request is marked as fulfilled
 *     -  If the record type indicates that a confirmation is to be sent,
 *        A CONFIRM is sent back to acknowledge the delivery
 * - It should be impossible to have gaps in the SEND FILE sequence. A
 *   gap should be flagged as a software error (see below).
 * - The recipient of a SEND FILE will return a CONFIRM immediately
 *   if the sequence number is already known, and has already been
 *   confirmed, and the transaction is marked as for confirmation.
 * - After any CONFIRM has been sent, the control program will launch
 *   an application to process the data that has arrived
 * - Any failure of this application represents a software error or an
 *   exceptional application condition:
 *   -  Mail the administrative user (see below)
 *   -  Flag the communication journal for this item
 *   -  Do not re-request the data; the error will presumably be repeated!
 * 
 * An asynchronous RPC facility is also supported, 
 * 
 * These are handled in a simpler way than the SEND FILE
 * - If the sequence number is not known, the message is actioned,
 *   by calling a function looked up in a table mapping message_types
 *   to functions
 * - The CONFIRM is sent back to acknowledge same, if required.
 * 
 * Any unexpected messages:
 * - Wrong message type
 * - Confirms for things that have not been sent
 * will be logged, and discarded.
 * 
 * We need to agree the medium by which serious conditions can be brought
 * to the attention of a human. Suggestions are:
 * - Writing on the console? To the error log?
 * - UNIX mail
 * 
 * Startup and Link Re-establishment
 * =================================
 * When it starts, the Communication control program reads the configuration
 * information, (from a configuration file, or off the command line, or out
 * of environment variables) which will tell it:
 * - Directories to use,
 * - Applications to run to process various file types
 * - The names of the control and journal files (non-ORACLE) or the ORACLE
 *   user_id/password (Pyramid)
 * - Whether or not it should initiate contact (in the case of the SI, there
 *   are two candidate machines; therefore it should make all initial
 *   contacts).
 * 
 * It then establishes its incoming socket(s) (listen(), select() (and 
 * named pipe, for Pyramid)).
 * 
 * If it is allowed to, it attempts to call peers on outgoing sockets.
 * 
 * Whenever first contact is made (ie. whenever a call arrives on the
 * listen() socket rather than coming through the connexion) it exchanges
 * control information with peer, by means of the EXCHANGE STATUS message.
 * 
 * When a Control Program receives an EXCHANGE STATUS, it responds
 * with an EXCHANGE STATUS. In other words, EXCHANGE STATUS is its own
 * confirm.
 * 
 * The EXCHANGE STATUS contents are, essentially, the Control Table entry
 * for this link. The Control Information is:
 * - Last sequence sent versus last sequence received, from the point of
 *   view of both ends
 * 
 * From the point of view of either process
 * - If last item sent sent > last item received received, re-initiate
 *   transfers to catch up.
 * - If last message sent sent < last message received received, we are in
 *   serious trouble; we have failed to recover after error as far as
 *   our peer
 *   - Flag a serious error; manual fix-up is required.
 * - If match, OK.
 * - If last message sent sent > last message received received
 *   recovery is in order:
 *   - The missing data should be sufficient to sort out the DB.
 *   - Mark the items in question as unconfirmed in the journal
 *   - Resend
 * - If last file received sent < last file sent received
 *   - Don't do anything
 *   - Expect to get things that are missing
 * 
 * If there is work to be done, do it.
 * 
 * Go into the steady state, waking up when events occur, or when
 * 'have a look round' time comes up (if you are the SI, and the links
 * are down).
 *
 * Link Closedown
 * ==============
 * At any time, a peer can send a SHUTDOWN message on a link. In this
 * case:
 * - Both ends close their connexions
 * - Neither end will attempt to re-initiate the connexion automatically
 *   (the link is marked 'Allowed to Call' = 'N')
 *
 * On the Pyramid, this is accomplished by sending '-s Link_ID' to the FIFO.
 *
 * -s 0 is equivalent to kill -USR1, and leads to an orderly shutdown.
 *
 * -c link_id will lead to an attempt to connect on a link.
 *  
 * Message Formats
 * ===============
 * The following are approximate only; see siinrec.h for the
 * definitive statement.
 *
 * EXCHANGE STATUS
 * - Message Type
 * - Transaction ID
 * - Link ID
 * - Host
 * - Maximum simultaneous sessions prepared to accept
 * - For out-going messages and files
 *   - The Last sequence number sent
 * - For incoming messages and files
 *   - The Last sequence number received.
 *
 * SHUTDOWN
 * - Message Type
 * - Transaction ID
 * - Link ID
 *
 * DATA
 * - Message Type
 * - Transaction ID
 * - Body; a transaction packet.
 * 
 * Within the body, the following sub-records can occur.
 *
 * SEND FILE
 * - Message Type
 * - Type of file (eg. run-actual)
 * - File name to send
 * 
 * CONFIRM
 * - Message Type
 * - Transaction being confirmed
 * - Success or Fail (information only; do not re-action if failed)
 * - Explanation
 * 
 * 
 * Processing Incoming Data
 * ========================
 * On the Pyramid
 * - C Structures corresponding to the incoming record layouts
 *   are defined in siinrec.h
 * - A general purpose read routine, siinrec() is defined that will
 *   - Read any of the defined records
 *   - Populate the C structure based on the record type
 *   - Return a pointer to the record type found, or NULL if in error.
 *
 * DATA messages consist of a leading envelope followed by the
 * message body; these are read with two calls to siinrec().
 * 
 * 
 * The application programs have the same overall structure.
 * - Argument 1 is the name of the communication file
 * - Open the file
 * - Read through it once, using the siinrec() routine, and
 *   check that the trailer counts are correct
 * - If not, exit(1) (and the control program will flag the error)
 * - Reset to the beginning of the file
 * - Read through it again, this time processing the records that
 *   are found:
 *   - Use ORACLE Array Processing (for performance reasons)
 *   - Build up the data in arrays that correspond to the fields
 *     in the corresponding Screen-based facility
 *   - Apply validation rules as per the screen
 *   - If it passes, apply the updates
 *   - If it does not pass:
 *     - Report the exception
 *     - Try to process further records in the file
 *   - Be prepared to process the same file twice, ignoring records
 *     that were successfully processed the first time.
 * - If there were no errors, only duplicate records, exit(0) (success)
 * - Else exit(1) (failure)
 *
 * The sub-routines:
 * - Are called with two arguments:
 *   - The record structure.
 *   - The address of pointer to an error message.
 * - Return either success (1) or failure (0).
 * - Have no need of array processing, simply because they can
 *   only process a single record per transaction unit.
 * - Must ensure that their SQL statements are not re-parsed each
 *   time (HOLD_CURSOR=YES, RELEASE_CURSOR=NO, MAXOPENCURSORS=large number)
 * - MUST INCLUDE A TRANSACTION ROLLBACK, in the event of an error
 * - MUST NOT INCLUDE A TRANSACTION COMMIT.
 *
 **************************************************************************
 * This program has been put together from two programs Copyright E2
 * Systems.
 *
 * - natrun  - the job scheduler at the heart of the E2 Systems UNIX SUPERNAT
 *             product, together with its associated libraries.
 * - catcher - a E2 Systems TCP/IP Network Trace Utility.
 *
 * The fragments of code that are derived from these sources
 * remain E2 Systems Trade Secrets.
 * - The source must not be disclosed to third parties without the express
 *   permission in writing of E2 Systems or its appointed representatives
 * - The copyright notices are maintained.
 * - These conditions are imposed on the third parties.
 *
 * The e2com_base.debug_level controls the level of diagnostics.
 *                   
 */ 
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1995\n";
static char * orig_id="@(#)natrun.c 1.11 91/10/30 19:10:30 91/10/30 18:59:30\nCopyright (C) E2 Systems 1990";
/*
 * The code set off trying to be POSIX where there is a difference, but
 * POSIX didn't always work on the Pyramid.
 */
#include <sys/types.h>
#ifdef AIX
#include <sys/wait.h>
#else
#ifdef SCO
#include <sys/wait.h>
#else
#ifdef NT4
#include <sys/wait.h>
#else
#include <wait.h>
#endif
#endif
#endif
#include <sys/file.h>
#ifdef AIX
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#endif
#ifdef MINGW32
typedef char * caddr_t;
#endif
#ifndef VCC2003
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#else
#include <WinSock2.h>
#endif
#include <sys/ioctl.h>
#ifdef SOLAR
#include <sys/un.h>
#endif
#include <sys/time.h>
#ifdef SOLAR
/*
 * This hideous construct works round a bug in the SUN C compilation system,
 * where the first file that generates machine instructions names all the
 * symbols from the master source. stat.h on the SUN generates stat(), mknod()
 * etc.; it must therefore be included after e2com.c has generated some code
 */
unsigned long umask();
#else
#include <sys/stat.h>
#endif
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <termio.h>
#include "hashlib.h"
#include "e2file.h"
#include "siinrec.h"
#ifdef SCO
#ifndef V4
#define NEEDSPIPE
#endif
#endif
#ifdef NEEDSPIPE
#include <sys/stream.h>
#include <sys/stropts.h>
/**************************************************************************
 * This horror is needed because normal SCO pipes do not support the 
 * poll() system call. There seems to be a limit of 64 pairs of these
 * things, as the kernel is configured at present. They appear to function
 * correctly, but there is some doubt as to whether they are deallocated
 * after the last close. However, we only need to allocate a pair of them,
 * and the process runs all the time.
 *
 * In order to allocate them, you need to create a device /dev/sp with
 * clone's major number and sp's major number as its minor number.
 *
 * eg mknod c 60 40
 *
 * stream I_FDINSERT ioctl format
 *
 * struct strfdinsert {
 *  struct strbuf ctlbuf;
 *  struct strbuf databuf;
 *  long          flags;
 *  int       fildes;
 *  int       offset;
 * };
 */
static int e2pipe(fds)
int * fds;
{
struct strfdinsert fdcon;
char * buf;
    if ((*fds = open("/dev/sp", O_RDONLY)) < 0)
        return -1;
    else
    if ((*(fds + 1) = open("/dev/sp", O_WRONLY)) < 0)
        return -1;
    else
    {
/*
 * Fill the buffer with the details of the read end, and send it on the write
 * end
 */
       fdcon.ctlbuf.len = sizeof(buf);
       fdcon.ctlbuf.maxlen = sizeof(buf);
       fdcon.ctlbuf.buf = (char *) (&buf);
       fdcon.databuf.len = -1;
       fdcon.databuf.maxlen = 0;
       fdcon.databuf.buf = (char *) NULL;
       fdcon.flags = 0;
       fdcon.fildes = *fds;
       fdcon.offset = 0;
       if (ioctl(*(fds+1),I_FDINSERT,(char *) &fdcon) < 0)
           return -1;
       else
           return 0;
    }
}
#else
#define e2pipe pipe
#endif
/* enum poll_flag {POLL,NOPOLL}; */
/*****************************************************************************
 *  Future event management
 */
static struct go_time
{
    LINK * link;
    long go_time;
} go_time[MAXLINKS];
static short int head=0, tail=0, clock_running=0;
static int child_death=0;
static int our_pid;
static struct timeval poll_int = {10,0};    /* wait 10 seconds timeval structure */
static long alarm_save;                 /* What's in the alarm clock; timer */
static void (*prev_alrm)();             /* Whatever is previously installed
                                           (either rem_time() or nothing) */

void reset_time();           /* reset the clock system */
void add_time();             /* manage a circular buffer of alarm clock calls */
void rem_time();
void alarm_preempt();        /* Put in a read timeout, in place of whatever is
                              * currently in the alarm clock
                              */
void alarm_restore();        /* Restore the previous clock value */
static void ini_link();      /* reads in link array                       */
static void dump_link();     /* lists out the link array                  */
char * link_read();          /* Reads a message off a link                */
static void dump_children(); /* lists out the currently executing msgs     */
void do_things();            /* process requests whilst still alive */
void do_exchange_status();   /* process an incoming EXCHANGE_STATUS */
void do_shutdown();          /* process an incoming SHUTDOWN */
void do_data();   /* process an incoming DATA */
void add_child();            /* administer the spawned processes */
void rem_child();
void die();             /* catch terminate signal */
void scarper();         /* exit, tidying up */
void chld_sig();        /* catch the death of a child */
void io_sig();          /* catch a communications event */
void seg_viol();        /* catch a segmentation violation event */
void chld_catcher();    /* reap children */
void proc_args();       /* process arguments */
void fifo_spawn();      /* Start the process that services the FIFO */
int enable_file_io();   /* Associate FILE's with fd's */
void fifo_check();      /* see what's doing */
void ackmsg();          /* Acknowledge an incoming message on a link */
void shutmsg();         /* Generate a shutdown message for a link */
void begin_shutdown();  /* Initiate e2com shutdown processing */
RUN_CHILD * find_msg();
void e2com_listen();  /* Set up the socket that listens for link connect
                           requests */
void link_boot();       /* Get the processing of messages on a link going
                           after a successful connexion is made */
void link_replay();     /* Restart incomplete transactions after a link comes
                           back up */
void proc_send_file();  /* Do a step in the processing of an incoming
                           SEND_FILE message */
void ftp_spawn();       /* Initiate an ftp, and return a sensible exit status */
void child_sig_clear(); /* Clear unwanted signal handlers in forked children */
void read_timeout();    /* Make sure that loss of synchronisation on the link
                           doesn't hang us up */
void proc_other_msg();  /* Process any other incoming message type */
int link_open();        /* Set up a link, ready to make a connexion */
E2MSG * link_fetch();     /* Get the next message for transmission on a link */
LINK * link_find();     /* Look up a link from an ID */
int link_add();         /* Queue another message for a link */
void link_close();      /* Clear link */
void link_shut();       /* Prepare to shut down link */

#define SIMUL 3
                        /* Default maximum number of children */
#define NORMAL 0
                        /* Shutdown processing control; Normal Running */
#define SHUTDOWN_PENDING 1
                        /* Shutdown processing control; haven't yet queued
                           the shutdown messages */
#define SHUTDOWN_PROGRESSING 2
                        /* Shutdown processing control; waiting for the
                           quiescent state */

static char fifo_buf[BUFSIZ];      /* fclose() does not appear to free the
                                    * buffer
                                    */
/*************************************************************
 * Data to from the FIFO
 */
static int ctl_pipe[2]; /* For control commands sent to the FIFO */
static int data_pipe[2]; /* For messages sent to the FIFO */
static int fifo_pid;     /* PID of the FIFO child */
static FILE * ctl_read;
static FILE * data_read;
static FILE * ctl_write;
static FILE * data_write;

static LINK link_det[MAXLINKS],
                             /* list of links in the input file */
         * link_cur_ptr = link_det,
         * link_max_ptr = &link_det[MAXLINKS-1];

static RUN_CHILD child_stat[MAXLINKS * SIMUL];

static short int glob_state = NORMAL;   /* ie. do not shut down! */
 
static int child_cnt;
#define DEFAULT_LINK 2
                                   /* Submissions that do not specify a link */
static int listen_fd;
static struct sockaddr_in listen_sock;
/***********************************************************************
 * Main Program Starts Here
 * VVVVVVVVVVVVVVVVVVVVVVVV
 */
main(argc,argv,envp)
int argc;
char * argv[];
char * envp[];
{
/****************************************************
 *    Initialise
 */
    e2com_base.errout = stderr;
    perf_global.errout = stderr;
    e2com_base.debug_level = 0;
    child_cnt = 0;
    sciinit(argc,argv);               /* Initialise control tables etc. */
    (void) sigset(SIGUSR1,die);       /* in order to exit */
    (void) sigset(SIGTERM,die);       /* in order to exit */
    (void) sigset(SIGCLD,chld_sig);
#ifdef SIGIO
    (void) sigset(SIGIO,SIG_IGN);     /* Only set for waiting on the fifo */
#endif
    (void) sigset(SIGPIPE,SIG_IGN);   /* So we don't crash out */
    (void) sigset(SIGHUP,SIG_IGN);    /* So we don't crash out */
    (void) sigset(SIGINT,SIG_IGN);    /* So we don't crash out */
    our_pid = getpid();               /* for SIGIO stuff       */
    perf_global.fifo_name =
              filename("%s/%s",e2com_base.work_directory,E2COM_FIFO);
    perf_global.lock_name =
              filename("%s/%s.lck",e2com_base.work_directory,E2COM_FIFO);
    e2com_listen();                   /* Open the listen port  */
    ini_link();                       /* Initialise all the links */
    (void) umask(0);                  /* Allow anyone to submit */
#ifdef SOLAR
    if ((e2com_base.listen_fd = fifo_listen(perf_global.fifo_name)) < 0)
    {
        char * x=
    "Failed to open the FIFO listen socket; aborting";
        (void) fprintf(e2com_base.errout,"Error: %d\n",errno);
        perror("Cannot Open FIFO using File descriptor");
        scilog(1,&x);
        (void) unlink(perf_global.fifo_name);
        sciabort();
    }
#else
    if (mknod(perf_global.fifo_name,0010666,0) < 0)
    { /* create the input FIFO */
        char * x ="Failed to create the FIFO; aborting";
        (void) fprintf(e2com_base.errout,"Error: %d\n",errno);
        perror("Cannot Create FIFO");
        scilog(1,&x);
        (void) unlink(perf_global.fifo_name);
        (void) unlink(perf_global.lock_name);
        sciabort();
    }
#endif
    fifo_spawn(argc,argv);      /* Set up child servicing the FIFO    */
    do_things();                /* process requests until nothing to do
                                 * DOES NOT RETURN
                                 */
    exit(0);
}
#ifdef SOLAR
#include <sys/stat.h>
#endif
/*
 * Exit, tidying up
 */
void scarper()
{
    char * x ="Termination Request Received; shutting down";
    (void) unlink(perf_global.fifo_name);
    (void) unlink(perf_global.lock_name);
    (void) fprintf(e2com_base.errout,"Termination Request Received\n");
    scilog(1,&x);
    if (fifo_pid)
        kill(fifo_pid,SIGUSR1);    /* Get rid of the child */
    scifinish();      /* Does not return */
}
/*****************************************************************
 * Service Shutdown Requests
 */
void die()
{
    if (fifo_pid)
        glob_state = SHUTDOWN_PENDING;       /* All links */
    else
        exit(0);                 /* No point in hanging around */
}
/*
 * Routine to kick of the child to service the FIFO
 */
void fifo_spawn(argc,argv)
int argc;
char ** argv[];
{
    if (e2pipe(data_pipe) == -1)
    {
        char * x ="Cannot open data pipe\n";
        (void) unlink(perf_global.fifo_name);
        (void) unlink(perf_global.lock_name);
        perror("Data e2pipe() Failed");
        scilog(1,&x);
        sciabort();      /* Does not return */
    }
    if (e2pipe(ctl_pipe) == -1)
    {
        char * x ="Cannot open control pipe\n";
        (void) unlink(perf_global.fifo_name);
        (void) unlink(perf_global.lock_name);
        perror("Data e2pipe() Failed");
        scilog(1,&x);
        sciabort();      /* Does not return */
    }
    if ((fifo_pid = fork()) > 0)
    {      /* PARENT success */
        if ((ctl_read = (FILE *) fdopen(ctl_pipe[0],"r")) == (FILE *) NULL)
        {
            char * x ="Cannot fdopen read control pipe\n";
            perror("fdopen() ctl_pipe read Failed");
            scilog(1,&x);
            scarper();      /* Does not return */
        }
        if ((data_read = (FILE *) fdopen(data_pipe[0],"r")) == (FILE *) NULL)
        {
            char * x ="Cannot fdopen read data pipe\n";
            perror("fdopen() data_pipe read Failed");
            scilog(1,&x);
            scarper();      /* Does not return */
        }
        (void) setbuf(ctl_read,NULL);
        (void) setbuf(data_read,NULL);
        return;
    }
    else if (fifo_pid < 0)
    {      /* Parent Failed */
        char * x ="Cannot fork() FIFO child\n";
        (void) unlink(perf_global.fifo_name);
        (void) unlink(perf_global.lock_name);
        perror("Cannot fork() FIFO child\n");
        scilog(1,&x);
        sciabort();      /* Does not return */
    }
/*
 * CHILD
 */
    child_sig_clear();
    sigset (SIGUSR1,die);
    if ((ctl_write = (FILE *) fdopen(ctl_pipe[1],"w")) == (FILE *) NULL)
    {
        char * x ="Cannot fdopen write control pipe\n";
        perror("fdopen() ctl_pipe write Failed");
        scilog(1,&x);
        die();      /* Does not return */
    }
    if ((data_write = (FILE *) fdopen(data_pipe[1],"w")) == (FILE *) NULL)
    {
        char * x ="Cannot fdopen write data pipe\n";
        perror("fdopen() data_pipe write Failed");
        scilog(1,&x);
        die();      /* Does not return */
    }
    (void) setbuf(ctl_write,NULL);
    (void) setbuf(data_write,NULL);
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,
                   "fifo_spawn() Child: Control FD: %d Data FD: %d\n",
                   ctl_pipe[1],data_pipe[1]);
    proc_args(argc,argv);
/*
 * FIFO Child Main Process; service User requests until signalled to stop
 */
    for(;;)
    {
#ifdef SOLAR
        e2com_base.fifo_fd =
                 fifo_accept(e2com_base.work_directory, e2com_base.listen_fd);
        perf_global.fifo = fdopen(e2com_base.fifo_fd,"r");
        setbuf(perf_global.fifo,fifo_buf);
#else
        e2com_base.fifo_fd = fifo_open();
#endif
        if (e2com_base.fifo_fd != -1)
            fifo_check();
                                   /* get back to a state of readiness */
    }
}
/*
 * Process arguments on the FIFO, including initial ones
 *
 * The same routine is used both by the FIFO child and its
 * parent; fifo_pid == 0 in the child.
 */
void proc_args(argc,argv)
int argc;
char ** argv;
{
    int c;
    E2MSG * input_request;
    union all_records data_envelope;
    char err_buf[BUFSIZ];
    FILE * errout_save;
    errout_save =  e2com_base.errout;
/*
 * Process the arguments
 */
    
    if (e2com_base.debug_level > 1)
    {
        (void) fprintf(e2com_base.errout,"proc_args() in %s\n",
                   (fifo_pid == 0)?"Child":"Parent");
        scilog(argc,argv);
    }
    input_request = (E2MSG *) NULL;   /* Clear the message */

    while ( ( c = getopt ( argc, argv, "as:hd:ik:f:t:l:r:c:e:" ) ) != EOF )
    {
        switch ( c )
        {
        case 'a' :
/*
 * Start over
 */
            if (fifo_pid)
            {             /* PARENT */
                LINK * cur_link;
                reset_time();                           /* tidy up the data
                                                       structures */
                for (cur_link = link_det;
                        cur_link->link_id != 0;
                            cur_link++)
                     link_close(cur_link);             /* Zap everything */
                scicommit();
                glob_state = NORMAL;
                ini_link();                             /* Totally restart */
            }
            else
                (void) fprintf(ctl_write,"-a\n");
            break;
        case 'h' :
            (void) fprintf(e2com_base.errout,"e2com: SI Communication Control process\n\
You ought to put this program in /etc/rc\n\
Options:\n\
 -h prints this message on e2com_base.errout\n\
 -a Activate\n\
 -c link_id; attempt to connect on a link\n\
 -s link_id; shut a link; 0 shuts all links, and terminates\n\
 -l Submit a SEND_FILE message for this link (default link 2)\n\
 -f Submit this file to be sent by a SEND_FILE\n\
 -t Submit a SEND_FILE message of this type\n\
 -r time; set new link retry delay\n\
 -d set the debug level (between 0 and 4)\n\
 -i dump out the link status information on e2com_base.errout\n\
 -k abort processing of the specified SEND_FILE message\n");
            break;
        case 'r':
            if (fifo_pid)
            {             /* PARENT */
                int r;
                if ((r = atoi(optarg)) != 0)
                e2com_base.retry_time = r;
            }
            else
                (void) fprintf(ctl_write,"-r %s\n",optarg);
            break;
        case 'c':
            {
                static char * x[2] = {"Invalid Link ID for connection",""};
                x[1] = optarg;
                if (fifo_pid)
                {             /* PARENT */
                    LINK * cur_link;
                    if ((cur_link = link_find(atoi(optarg))) == (LINK *) NULL)
                        scilog(2,x);
                    else
                    {
                        cur_link->allowed_to_call = 'Y';
                        scilinkupd(cur_link);
                        scicommit();
                        link_open(cur_link);
                    }
                }
                else
                if (atoi(optarg) > 0)
                    (void) fprintf(ctl_write,"-c %s\n",optarg);
                else
                    scilog(2,x);
            }
            break;
        case 's':
            {
                static char * x[3] = {"Shut Down","",""};
                if (fifo_pid)
                {             /* PARENT */
                    LINK * cur_link;
                    int link_to_close;
                    if ((link_to_close = atoi(optarg)) == 0)
                    {
                        x[1] = ":";
                        x[2] =  "e2com: System Suspended\n";
                        begin_shutdown();
                    }
                    else
                    {
                        if ((cur_link=link_find(link_to_close))==(LINK *) NULL)
                        {          /* Cannot call on wild-card link! */
                            x[1] = "Invalid Link ID";
                            x[2] = optarg;
                        }
                        else
                        {
                            x[1] = "Link";
                            x[2] = optarg;
                            link_shut(cur_link);
                            scicommit();
                        }
                        link_to_close = NORMAL;
                    }
                    scilog(3,x);
                }
                else
                if (atoi(optarg) > -1)
                    (void) fprintf(ctl_write,"-s %s\n",optarg);
                else
                {
                    x[1] = "Invalid Link ID";
                    x[2] = optarg;
                    scilog(3,x);
                }
            }
            break;
        case 'e':
/*
 * Change the disposition of error output
 */
               sighold(SIGALRM);
               sighold(SIGCHLD);
               sigset(SIGPIPE,chld_sig);
#ifdef SOLAR
               if ((e2com_base.errout = fdopen(e2com_base.fifo_fd,"w"))
                       == (FILE *) NULL)
               {
                   perror("Response Connect() failed");
                   e2com_base.errout = stderr;
               }
               else
                   setbuf(e2com_base.errout,err_buf);
#else
               if ((e2com_base.errout = fopen(optarg,"w")) == (FILE *) NULL)
               {
                   perror("Response FIFO open failed");
                   e2com_base.errout = stderr;
               }
               else
                   setbuf(e2com_base.errout,err_buf);
#endif
               sigrelse(SIGALRM);
               sigrelse(SIGCHLD);
            break;
        case 'k':
            if (fifo_pid)
            {             /* PARENT */
               int seq;
               if ((seq = atoi(optarg)) != 0)
               {
                   RUN_CHILD * kill_msg;
               if ((kill_msg = find_msg(seq))
                             != (RUN_CHILD *) NULL)
                   {
               (void) kill(kill_msg->child_pid,SIGUSR1);
                       /*
                        * we will find out soon enough if it has been killed
                        */
                   }
               }
            }
            else
                (void) fprintf(ctl_write,"-k %s\n",optarg);
            break;
        case 'i':
            if (fifo_pid)
            {             /* PARENT */
                dump_link();
                dump_children();
            }
            else
                (void) fprintf(ctl_write,"-i\n");
            break;
        case 'd':
            if (fifo_pid)
            {             /* PARENT */
                static char * x[2]={"Debug Level Set to"};
                e2com_base.debug_level = atoi(optarg);
                x[1] = optarg;
                scilog(2,x);
            }
            else
                (void) fprintf(ctl_write,"-d %s\n",optarg);
            break;
        case 'l':
/*
 * Identify the link to send the message on
 *
 * We need to default this to 1.
 *
 * The E2MSG must be linked to the LINK structure.
 */
            if (!fifo_pid)
            {             /* CHILD only */
                if (input_request == (E2MSG *) NULL)
                    input_request = msg_struct_ini(SEND_FILE_TYPE,TRANSMIT);
                input_request->link_id = atoi(optarg);
            }
            break;
        case 'f':
/*
 * the name of the file to transfer
 */
            if (!fifo_pid)
            {             /* CHILD only */
                struct stat stat_buf;
                if (input_request == (E2MSG *) NULL)
                    input_request = msg_struct_ini(SEND_FILE_TYPE,TRANSMIT);
                (void) sprintf(
                         input_request->msg_data->send_file.send_file_name,
                         "%-*.*s", SEND_FILE_NAME_LEN, SEND_FILE_NAME_LEN,
                        optarg);
                input_request->send_file_name = strdup(optarg);
                if (stat(input_request->send_file_name,&stat_buf) < 0)
                {
                    static char * x[2] ={
                                 "Unable to establish the length of the file\n",
                                 ""};
                    x[1] = input_request->send_file_name;
                    scilog(2,x);
                    (void) free( input_request->send_file_name);
                    input_request->send_file_name = (char *) NULL;
                }
                else if (stat_buf.st_size == 0L)
                {
                    static char * x[2] ={
                                 "Cannot transmit zero length file\n",
                                 ""};
                    x[1] = input_request->send_file_name;
                    scilog(2,x);
                    (void) free( input_request->send_file_name);
                    input_request->send_file_name = (char *) NULL;
                }
                else
                    (void) sprintf(
                            input_request->msg_data->send_file.send_file_length,
                                 "%-*.1u", SEQUENCE_LEN, stat_buf.st_size);
            }
            break;
        case 't':
            if (!fifo_pid)
            {             /* CHILD only */
                if (input_request == (E2MSG *) NULL)
                    input_request = msg_struct_ini(SEND_FILE_TYPE,TRANSMIT);
                (void) sprintf(
                         input_request->msg_data->send_file.send_file_type,
                         "%-*.*s", SEND_FILE_TYPE_LEN, SEND_FILE_TYPE_LEN,
                        optarg);
                input_request->msg_type = strdup(optarg);
            }
            break;
        default:
        case '?' : /* Default - invalid opt.*/
               (void) fprintf(e2com_base.errout,"Invalid argument; try -h\n");
               if (fifo_pid)
               {             /* Parent only */
                  if (input_request != (E2MSG *) NULL)
                      msg_destroy(input_request);
                  scirollback();
               }
        break;
        } 
    }
    if (input_request != (E2MSG *) NULL && fifo_pid == 0)
    {
        if (input_request->link_id == 0)
            input_request->link_id = DEFAULT_LINK;
    if (e2com_base.debug_level > 0)
    (void) fprintf(e2com_base.errout,"Processing FIFO SEND_FILE Link %d\n",
                   input_request->link_id);
        if (input_request->msg_type == (char *) NULL)
        {
            char * x = "Incomplete File Transfer Request; must identify Type\n";
            scilog(1,&x);
        }
        else
        if (input_request->send_file_name == (char *) NULL)
        {
            char * x = "Incomplete File Transfer Request; must identify File\n";
            scilog(1,&x);
        }
        else       /* Send the message to the parent */
        {
            (void) sprintf(data_envelope.data.record_type,
                           "%-*s",RECORD_TYPE_LEN,DATA_TYPE);
            (void) sprintf(data_envelope.data.sequence,
                           "%-*d",SEQUENCE_LEN,input_request->link_id);
            (void) sioutrec(data_write,&data_envelope, (E2_FILE *) NULL);
            (void) sioutrec(data_write,input_request->msg_data,
                             (E2_FILE *) NULL);
            fprintf(e2com_base.errout,"\nOK\n");
            fflush(e2com_base.errout);
        }
        msg_destroy(input_request);
    }
/*
 * Restore the error file pointer
 */
    if (e2com_base.errout != errout_save)
        (void) fclose(e2com_base.errout);
    e2com_base.errout = errout_save;
    return;
}
/*******************************************************8
 * Check the FIFO for something to do
 * Only called from the FIFO child
 */
void fifo_check()
{
char * so_far;
char * fifo_args[13];                           /* Dummy arguments to process */
char fifo_line[BUFLEN];                         /* Dummy arguments to process */
int read_cnt=0;                                 /* Number of bytes read */
int c;
register char * x;
short int i;
/*
 struct stat stat_buf;
 *
 * Is there anything doing on the FIFO front?
 *
 * Cannot change the strategy regarding the FIFO to a select() on:
 * - The fifo
 * - The listen() sockets
 * - The connexion sockets
 * because select() is not useful!
 *
 *    if (stat(perf_global.lock_name,&stat_buf) < 0 && (poll_flag == POLL || timer_exp))
 *      return;                         /o still things to do o/
 *
 * The following piece of code handles messages in two different
 * formats.
 *
 * The first is getopt() args format
 * - The code caters for sending control commands as well as message
 *   data.
 * - The data can easily be sent by UNIX shell procedures.
 * However, using this mechanism:
 * - Only SEND_FILE messages are queued.
 * This is in contrast to the processing of incoming messages, which
 * is highly generalised, and readily extensible.
 *
 * Getopts() format is recognised by having a single "-" as the
 * first character.
 *
 * The other method is by using the siinrec() routine, also used for
 * reading incoming messages on the links.
 *
 * The SEQUENCE field must be filled in with the link that the
 * message is to be directed to.
 */
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"fifo_check()\n");
    if (fifo_pid)
    {
        char * x =
         "Logic Error: fifo_check() should only be called from the child\n";
        scilog(1,&x);
    }
    else
    if ((c = getc(perf_global.fifo)) != EOF)
    {
        ungetc(c,perf_global.fifo);           /* Put the character back */
        if (c == '-')
        {
        
            for (read_cnt = 0,
                 x=fifo_line,
                 so_far = x;
                ((x + read_cnt) < (fifo_line + sizeof(fifo_line) - 1)) &&
                ((read_cnt=fread (x, sizeof(char),
                   sizeof(fifo_line) - (x - fifo_line) -1, perf_global.fifo)) > 0);
                        x +=read_cnt)
#ifdef SOLAR
        {
            char * x1, *x2;
            x2 = x + read_cnt;
            *x2 = '\0';                  /* put an end marker */
            for (;so_far < x2; so_far = x1+1)
                if ((x1=strchr(so_far,(int) '\n')) == (char *) NULL)
                     break;
                else
                {
                    *x1='\0';
                    if (!strcmp(so_far,LOOK_STR))
                    {
                        *so_far = '\0';
                        x = so_far;
                        goto pseudoeof;
                    }
                    else
                    {
                        *x1='\n';
                         so_far = x1;
                    }
                }
        }
pseudoeof:
        if (errno)
            perror("User Request");
#ifdef DEBUG
        fprintf(stderr,"fifo_line:\n%s\n",fifo_line);
        fflush(stderr);
#endif
#else
           ;
            (void) fclose(perf_global.fifo);
#endif
                        
            (void) unlink(perf_global.lock_name);
            *x = '\0';                             /* terminate the string */

/*
 * Process the arguments in the string that has been read
 */
            if ((fifo_args[0]=strtok(fifo_line,"  \n"))==NULL) return;
/*
 * Generate an argument vector
 */
            for (i=1; (fifo_args[i]=strtok(NULL," \n")) != NULL; i++);
 
            opterr=1;                /* turn off getopt() messages */
            optind=0;                /* reset to start of list */
            proc_args(i,fifo_args);
        }
        else   /* Expect Communications Format Data */
        {
            union all_records in_buf;
            alarm_preempt();
            while ((siinrec(perf_global.fifo,&in_buf, (E2_FILE *) NULL)) != (char *) NULL)
            {
                alarm_restore();
                (void) sioutrec(data_write,&in_buf, (E2_FILE *) NULL);
                alarm_preempt();
            }
                                  /* Queue it for transmission, if possible */
            alarm_restore();
            (void) fclose(perf_global.fifo);
            (void) unlink(perf_global.lock_name);
        }
    }
    else
    {
        if (e2com_base.debug_level > 0)
            (void) fprintf(e2com_base.errout,
                    "fifo_check(): ready but no data\n");
        (void) fclose(perf_global.fifo);
        (void) unlink(perf_global.lock_name);
    }
    return;
}
/*
 * chld_sig(); interrupt the select() or whatever.
 */
void chld_sig()
{
    child_death++;
    return;
}
/*
 * read_timeout(); interrupt a network read that is taking too long
 */
void read_timeout()
{
    return;
}
/*
 * chld_catcher(); reap children as and when
 * PYRAMID Problems:
 * - waitpid() doesn't work at all
 * - wait3() doesn't like being called when the child signal handler is
 *   installed; be sure that the signal handler has gone off before
 *   calling (and we will still disable it).
 */
void chld_catcher(hang_state)
int hang_state;
{
    int pid;
#ifdef POSIX
    int
#else
    union wait
#endif
    pidstatus;

    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"chld_catcher(); Looking for Children....\n");
    (void) sigset(SIGCLD,SIG_DFL); /* Avoid nasties with the chld_sig/wait3()
                                       interaction */
    while ((pid=wait3(&pidstatus, hang_state, 0)) > 0)
    {
        child_death--;
        rem_child(pid,pidstatus);
    }
    child_death = 0;
    (void) sigset(SIGCLD,chld_sig); /* re-install */
    return;
}
/*
 * Routine to enable the delivery of SIGIOs to the process
 */
int set_io_pid(fd)
int fd;
{
#ifdef SIGIO
    int i;
    char * x = "1";
    if (fcntl(fd,F_SETOWN,our_pid) == -1)
    {
        char * x = "Couldn't set pid to receive event notification\n";
        scilog(1,&x);
        perror("fcntl() pid set failed");
        return 0;
    }
#endif
    return 1;
} 
/*
 * Routine to read a message on the link, tidying up if things go wrong
 */
char * link_read ( cur_link, write_mask, in_buf)
LINK * cur_link;
int * write_mask;
union all_records * in_buf;
{
    char * rec_type;
    alarm_preempt();     /* Set read timeout */
    rec_type = siinrec(cur_link->read_connect_file,in_buf, (E2_FILE *) NULL);
    alarm_restore();       /* Put it back to what it was */
    if (rec_type == (char *) NULL)
    {                      /* Garbage on the link */
        static char * x[]= { "Garbage on link","",
                             " re-initialising\n"};
        char x1[10];
        (void) sprintf(x1,"%d",cur_link->link_id);
        x[1] = x1;
        scilog(3,x);
        write_mask[(cur_link->connect_fd/32)]
                          &= ~(1 << (cur_link->connect_fd % 32));
        link_close(cur_link);
        scicommit();
        if ( cur_link->allowed_to_call == 'Y')
        {
            cur_link->allowed_to_call = 'N';
            add_time(cur_link,e2com_base.retry_time);
        }
        link_open(cur_link);    /* start again */
    }
    return rec_type;
}
/*
 * Function to handle requests, honouring simultaneity limits, until there
 * is absolutely nothing more to do for the moment
 */
void do_things()
{
/*
 * Put a pointer to this struct as the last argument to select() to
 * get it to poll
 */
    int ctl_read_fd;
    int data_read_fd;
    int read_mask[2];
    int write_mask[2];
    int except_mask[2];
/*
 * Main processing:
 *      - wake up if prodded
 *      - prods can come from the communication link, or from an application
 */
    LINK * cur_link;
    E2MSG * cur_msg;

    int i;
    data_read_fd = data_pipe[0];
    ctl_read_fd = ctl_pipe[0];
/*
 * Process forever (death is by signal SIGUSR1)
 */
for (;;)
{
/*
 * Make sure that signals will be delivered
 */
    sigrelse(SIGALRM);
    sigrelse(SIGUSR1);
    sigrelse(SIGCLD);
/*
 * If we have been signalled to shut down, queue the shutdown messages
 */
     if (glob_state == SHUTDOWN_PENDING)
         begin_shutdown();
/*
 * Initialise the the select masks
 */
     read_mask[(listen_fd/32)] = (1 << (listen_fd % 32));
                                     /* always look for the listen */
     read_mask[(listen_fd/32)?0:1] = 0;
     read_mask[(ctl_read_fd/32)] |= (1 << (ctl_read_fd % 32));
                                     /* always look for the control */
     read_mask[(data_read_fd/32)] |= (1 << (data_read_fd % 32));
                                     /* always look for the FIFO data */
     write_mask[0] = 0;
     write_mask[1] = 0;              /* only look for writes if there
                                      * is anything to do
                                      */
/*
 * Now look for file descriptors for the links; count the open links
 * as we go
 */
     for (i = 0, cur_link = link_det;
              cur_link->link_id  != 0;
                  cur_link++)
     {
         if (cur_link->connect_fd != -1)
         {
             i++;
             if (cur_link->max_simul > cur_link->cur_simul)
             {      /* ignore data if we are too busy */
                 read_mask[(cur_link->connect_fd/32)]
                        |= (1 << (cur_link->connect_fd %32));
             }
/*
 *  Each pass, inspect the links for chained messages
 *
 * Only interested in select() for write if:
 * - Connexion is pending
 * - Or there is data to send
 */
             if (cur_link->in_out == 'O' ||
                     cur_link->first_msg != (E2MSG *) NULL)
                 write_mask[(cur_link->connect_fd)/32]
                     |= (1 << (cur_link->connect_fd %32));
         }
     }
     except_mask[0] = read_mask[0] | write_mask[0];
     except_mask[1] = read_mask[1] | write_mask[1];
/*
 * Exit if Shutdown processing is complete (no more open links)
 */
     if (i == 0 && glob_state == SHUTDOWN_PROGRESSING)
         scarper();
/*
 * See if anything has happened interrupt-wise whilst the main
 * processing is going on NOTE THE RACE CONDITION WITH THE SIGNAL
 * HANDLER HERE.
 */
    if (e2com_base.debug_level > 2)
    {
        (void) fprintf(e2com_base.errout,"File Descriptor Service Loop\n");
        (void) fprintf(e2com_base.errout,"============================\n");
        fprintf(e2com_base.errout,"listen_fd: %d Position 0x%x\n",listen_fd,
                      (1 << listen_fd));
        fprintf(e2com_base.errout,"ctl_read_fd: %d Position 0x%x\n",
                    ctl_read_fd,(1 << ctl_read_fd));
        fprintf(e2com_base.errout,"data_read_fd: %d Position 0x%x\n",
                    data_read_fd,(1 << data_read_fd));
        if (link_det[0].connect_fd != -1)
            fprintf(e2com_base.errout,"link 1 fd: %d Position 0x%x\n",
                   link_det[0].connect_fd,
                   (1 << link_det[0].connect_fd));
        if (link_det[1].connect_fd != -1)
            fprintf(e2com_base.errout,"link 2 fd: %d Position 0x%x\n",
                   link_det[1].connect_fd,
                   (1 << link_det[1].connect_fd));
        fprintf(e2com_base.errout,
          "read_mask: 0x%x:%x  write_mask: 0x%x:%x  except_mask: 0x%x:%x\n",
                   read_mask[0],read_mask[1],write_mask[0],
                  write_mask[1],except_mask[0],except_mask[1]);
    }
    if (child_death)
        chld_catcher(WNOHANG);
/*
 * Block until something interesting happens
 * Put &poll_int as the last argument to get it to poll.
 */
    i = select(64,read_mask,write_mask,except_mask,0);
    if (i < 0 && errno == EINTR)
    {                             /* Death of Child or Alarm clock */
        if (child_death)
            chld_catcher(WNOHANG);
                                 /* Alarm clock is completely serviced
                                  * by its signal catcher, so do
                                  * not need to do anything more.
                                  * IO signals will be picked up by the
                                  * select, hopefully.
                                  * Terminate signals (USR1) are picked
                                  * up at the top. The reason for calling
                                  * chld_catcher() more often is that the
                                  * Pyramid has an annoying tendency to
                                  * bin the Zombies before wait3() can get
                                  * them.
                                  */
    }
/**************************************************************************
 * Poll processing; Not Activated; a child process manages the fifo
 *
 *  else if (i == 0 || errno == EINPROGRESS)
 *  {                                     /o Nothing to do at the moment o/
 *      int fifo_fd;
 *      fifo_fd = fifo_open();
 *      if (fifo_fd != -1)
 *          fifo_check();
 *                                 /o get back to a state of readiness o/
 *  }
 */
    else if (i > 0)
    {       /* process the descriptors in turn */
        if (e2com_base.debug_level > 2)
        {
            (void) fprintf(e2com_base.errout,"Select Results\n");
            (void) fprintf(e2com_base.errout,"==============\n");
            fprintf(e2com_base.errout,
              "read_mask: 0x%x:%x  write_mask: 0x%x:%x  except_mask: 0x%x:%x\n",
                       read_mask[0],read_mask[1],write_mask[0],
                      write_mask[1],except_mask[0],except_mask[1]);
        }
/*
 * Stop signals messing up the reads and writes
 */
        sighold(SIGALRM);
        sighold(SIGUSR1);
        sighold(SIGCLD);
        if ( except_mask[(ctl_read_fd/32)] & (1 << (ctl_read_fd % 32)))
        {
            perror("select() set exception");
                (void) fprintf(e2com_base.errout,
                  "Exception on FIFO child Control\n");
            abort();
        }
        else
        if ( read_mask[(ctl_read_fd/32)] & (1 << (ctl_read_fd % 32)))
        {                   /* Control command from the FIFO */ 
            char * fifo_args[13];         /* Dummy arguments to process */
            short int i;

            if (e2com_base.debug_level > 0)
                (void) fprintf(e2com_base.errout,
                                "Command arrived from the FIFO child\n");
            if (fgets(fifo_buf,sizeof(fifo_buf) - 1,ctl_read) == (char *) NULL)
            {
                char *x="FIFO Read select()ed but no characters read!";
                scilog(1,&x);
            }
/*
 * Process the arguments in the string that has been read
 */
            else
            if ((fifo_args[0]=strtok(fifo_buf,"  \n"))!=NULL)
            {
/*
 * Generate an argument vector
 */
                for (i=1; (fifo_args[i]=strtok(NULL," \n")) != NULL; i++);
 
                opterr=1;                /* turn off getopt() messages */
                optind=0;                /* reset to start of list */
                proc_args(i,fifo_args);
            }
        }
        if ( except_mask[(data_read_fd/32)] & (1 << (data_read_fd % 32)))
        {
            perror("select() set exception");
                (void) fprintf(e2com_base.errout,
                  "Exception on FIFO child Data\n");
            abort();
        }
        if (read_mask[(data_read_fd/32)] & (1 << (data_read_fd % 32)))
        {                   /* Data from the FIFO */ 
            union all_records in_buf;
            union all_records in_rest;
            char * x = "Lost Synchronisation on the FIFO Data pipe\n";
            if (e2com_base.debug_level > 1)
                (void) fprintf(e2com_base.errout,
                            "Data arrived from the FIFO child\n");
            alarm_preempt();      /* Prevent loss of sunchronisation hanging
                                     the system */
            if (siinrec(data_read, &in_buf, (E2_FILE *) NULL) == (char *) NULL)
            {
                alarm_restore();
                scilog(1,&x);
            }
            else
            {
                alarm_restore();
                alarm_preempt();      /* Prevent loss of sunchronisation hanging
                                         the system */
                if (strcmp(in_buf.data.record_type,DATA_TYPE) ||
                    siinrec(data_read, &in_rest, (E2_FILE *) NULL)
                                     == (char *) NULL)
                {
                    alarm_restore();
                    scilog(1,&x);
                }
                else
                {
                    alarm_restore();
                    cur_msg = msg_struct_ini(in_rest.send_file.record_type,
                                             TRANSMIT);
                    cur_msg->link_id = atoi(in_buf.data.sequence);
                    *(cur_msg->msg_data) = in_rest;
                    in_buf = in_rest;
                    if (!strcmp(in_buf.send_file.record_type,SEND_FILE_TYPE))
                    {                 /* Extra information stored for these */
                        cur_msg->msg_type=
                                    strdup(in_buf.send_file.send_file_type);
                        cur_msg->send_file_name=
                                    strdup(in_buf.send_file.send_file_name);
                    }
                    link_add(cur_msg);
                    createmsg(cur_msg);
                    scicommit();
                }
            }
        }
        if ((listen_fd != -1 &&
             read_mask[(listen_fd/32)] & (1 << (listen_fd % 32))))
        {                   /* Incoming call on the connect */ 
            struct sockaddr_in calling_sock;
            int calladdrlength;
            int acc_fd;

            calladdrlength = sizeof(calling_sock);
            if ((acc_fd = accept(listen_fd, &calling_sock,
                                      &calladdrlength)) < 0)
            {
                char * x = "Accept failed\n";
                scilog(1,&x);
                perror("Accept on link failed");
            }
            else
            if (set_io_pid(acc_fd))
            {
/* We need to see if:
 * - We recognise who is calling us
 * - We already thought we were connected
 * Loop - process the links in turn to see if we recognise who
 * is calling us.
 */
                if (e2com_base.debug_level > 2)
                    (void) fprintf(e2com_base.errout,
                         "Incoming Accept() succeeded\n");
                for (cur_link = link_det;
                         cur_link->link_id != 0;
                             cur_link++)
                {
                     struct hostent * callhost;
/*
 * The conditions are:
 * 1. A call comes in for a link that has an associated host name
 * 2. A call comes in for a link that doesn't have an associated host name.
 *
 * Case 1 is easy:
 * - Compare the calling host with what we expect
 * - No match, reject
 * - Otherwise succeed.
 *
 * Case 2 requires a lot more work to do properly, since we do not know the
 * incoming link for certain until the EXCHANGE STATUS message arrives; we
 * would have to provisionally allocate a link, and then confirm it or bin
 * it when the EXCHANGE STATUS is sent.
 * 
 * We impose a restriction. Any incoming request that has not matched
 * a named host link matches the wild-card link.
 *
 * The consequences of this approach are:
 * - Only one wild-card link is meaningful
 * - It should have the highest link_id allocated
 * - Links with higher link numbers than the wild-card link will never
 *   receive incoming calls.
 * - If
 *   - There was a connexion on the wild-card link
 *   - Somehow an erroneous connexion request came in
 *     the connexion will have been canned unnecessarily.
 *
 * For loop-back testing, however, the wild-card link comes first.
 * Otherwise, the system will match the incoming request to the link
 * that sent it!
 */ 
                    if ((cur_link->dest_host_name == (char *) NULL ||
                                strlen(cur_link->dest_host_name) == 0))
                                        /* The wild-card link */
                        memcpy((char *) &cur_link->connect_sock,
                               (char *) &calling_sock,
                                  calladdrlength);
                    else if (memcmp((char *) &calling_sock.sin_addr,
                                     (char *) &cur_link->connect_sock.sin_addr,
                                  sizeof(calling_sock.sin_addr)))
                        continue; /* Not the correct link */
                    else
                        memcpy((char *) &cur_link->connect_sock,
                               (char *) &calling_sock,
                                  calladdrlength);

                    if (e2com_base.debug_level > 2)
                        (void) fprintf(e2com_base.errout,
                               "Matched link %d\n",cur_link->link_id);
                    if (cur_link->connect_fd != -1)
                    {               /* We thought we had a connexion! */
                        except_mask[(cur_link->connect_fd/32)]
                                    &= ~(1 << (cur_link->connect_fd % 32));
                        write_mask[(cur_link->connect_fd/32)]
                                    &= ~(1 << (cur_link->connect_fd % 32));
                        read_mask[(cur_link->connect_fd/32)]
                                    &= ~(1 << (cur_link->connect_fd % 32));
                        (void) close(cur_link->connect_fd);
                        cur_link->connect_fd = -1;
                        while ((cur_msg = link_fetch(cur_link)) != (E2MSG *) NULL)
                             msg_destroy(cur_msg);   /* Bin any outstanding
                                                        messages; these will
                                                        be requeued
                                                     */
                    }
                    if ((callhost = gethostbyaddr((char *)
                                      &cur_link->connect_sock.sin_addr,
                             sizeof(cur_link->connect_sock.sin_addr),
                                        AF_INET)) == (struct hostent *) NULL)
                    {
                        char * inet_ntoa();
                        static char * x[]= {"Unknown Calling Host","",
                           "Have you forgotten to run /etc/mkhosts?"};
                        x[1] = inet_ntoa(cur_link->connect_sock.sin_addr);
                        scilog(3,x);
                        cur_link->cur_host_name = strdup(x[1]);
                        perror("Unexpected gethostbyaddr() failure");
                    }
                    else
                    {
                        cur_link->cur_host_name = strdup(callhost->h_name);
                    }
                    cur_link->connect_fd = acc_fd; 
                    if (enable_file_io(cur_link))
                        link_boot(cur_link);
                    break;                          /* Should only zap one */
                }
            }
        }
 /*
  * Loop - process the links in turn, for read descriptors
  */
        for (cur_link = link_det;
                  cur_link->link_id != 0;
                      cur_link++)
        {
            if (cur_link->connect_fd != -1 &&
                (read_mask[(cur_link->connect_fd/32)] &
                 (1 << (cur_link->connect_fd % 32))))
            {
                union all_records in_buf;
                union all_records in_rest;
                char * rec_type;
                except_mask[(cur_link->connect_fd/32)]
                            &= ~(1 << (cur_link->connect_fd % 32));
                if (e2com_base.debug_level > 2)
                    (void) fprintf(e2com_base.errout,
                           "Data Ready on Link: %d\n", cur_link->link_id);
                rec_type = link_read(cur_link,write_mask,&in_buf);
                                              /* Read it off the link */
                if (rec_type != (char *) NULL)
                {                  /* siinrec() recognised the message */
/*
 * The code following code has knowedge of the three message types:
 * - EXCHANGE_STATUS
 * - DATA
 * - SHUTDOWN
 *
 * In addition, it knows about the sub-records in DATA messages.
 * It can handle confirmation records, in and out.
 *
 * The processing of EXCHANGE_STATUS and SHUTDOWN are completely hard coded
 * here.
 *
 * The code knows how to handle a SEND_FILE, but what it does
 * depends on entries in the E2COM_MESS_TYPES table.
 *
 * To process an incoming SEND_FILE:
 * -  There must exist a row in E2COM_MESS_TYPES with the incoming
 *    send_file_type (eg. 'run-actual') in the msg_type column.
 * -  The sub_y_or_n field must be 'N'.
 * -  An ftp is initiated.
 * -  If that completes successfully:
 *    -  If the confirm_record_type in E2COM_MESS_TYPES is not null,
 *       a CONFIRM message of this type is sent
 *    -  If the sequence is one more that the last_in_rec for the link,
 *       this is updated
 *    -  Completions out of sequence (which are possible) will be sorted
 *       out at the next link start up, by scilinksyn()
 *    -  The message status is sent to MACKED
 *    -  The program indicated in E2COM_MESS_TYPES prog_to_fire is run
 *       with argument 1 the ORACLE User_ID/Password, and argument 2 the file
 *       to process.
 *       -  If that completes
 *          -  The message status is set to MDONE.
 *          -  If it was successful, the success code is set to 'S'
 *          -  Otherwise it is set to 'F', fail.
 *
 * To process an incoming CONFIRM
 * -  The RECORD_TYPE must match the msg_type column in E2COM_MESS_TYPES
 * -  The sub_y_or_n field must be NULL.
 *
 * To set up to process any other RECORD_TYPE value
 * -  There must be defined a structure for the record layout, in
 *    siinrec.h
 * -  The routine siinrec() in siinrec.c must be modified to recognise
 *    this record type (using mess_read_edit.sh on the record layout
 *    will generate the appropriate code fragment for inclusion in
 *    siinrec.c)
 * -  The routine sioutrec() in siinrec.c must be modified to recognise
 *    this record type (using mess_write_edit.sh on the record layout
 *    will generate the appropriate code fragment for inclusion in
 *    siinrec.c)
 * -  The RECORD_TYPE must match the msg_type column in E2COM_MESS_TYPES
 * -  The sub_y_or_n field must be 'Y'.
 * -  A function must be written which:
 *    -  Takes the record, in union all_records, as input.
 *    -  DOES NOT DO A COMMIT    
 *    -  Returns either success or failure
 * -  The Confirm record type, if it exists, must be present in
 *    E2COM_MESS_TYPES
 * -  The RECORD_TYPE and the name of the function must be added to the
 *    sub_links array, defined in scilib.pc
 * -  e2com.c, siinrec.c, scilib.pc and all of the functions itemised
 *    in sub_links must be compiled and linked together in the e2com
 *    executable.
 *
 * Incoming RECORD_TYPES with sub-routines defined are processed as follows.
 * -  The RECORD_TYPE is found in the E2COM_MESS_TYPES array.
 * -  The function is called with the incoming record as an argument. 
 * -  When the function returns:
 *    - The message is marked MDONE
 *    - A confirmation is sent, if confirm_record_type is defined.
 */
                    NEW(E2MSG,cur_msg);
                    cur_msg->trans_rec_ind = RECEIVE;
                    cur_msg->msg_link = cur_link;
                    cur_msg->link_id = cur_link->link_id;
                    cur_msg->seq = atoi(in_buf.data.sequence);
                    if (readmsg(cur_msg))
                    {                     /* The message is already known */
                        static char * x[4] ={ "Duplicate Message from","",
                                             "Sequence"};
                        x[1] = cur_link->cur_host_name;
                        x[3] = in_buf.data.sequence;
                        scilog(4,x);
                        if (!strcmp(in_buf.data.record_type, DATA_TYPE))
                                    /* Skip the sub-record */
                            (void) link_read(cur_link,write_mask,&in_buf);
                        msg_destroy(cur_msg);   /* Finished with it */
                        continue;         /* Re-processing is done off the
                                             Journal */
                    }
                    msg_destroy(cur_msg);   /* Start over */
                    cur_msg = msg_struct_ini(in_buf.data.record_type,RECEIVE);
                    cur_msg->link_id = cur_link->link_id;
                    cur_msg->msg_link = cur_link;
                    *(cur_msg->msg_data) = in_buf;
                    cur_msg->seq = atoi(in_buf.exchange_status.sequence);
                                   /* Rely on the sequence always being
                                      in the same place */
                    if (!strcmp(in_buf.data.record_type,EXCHANGE_STATUS_TYPE))
                        do_exchange_status(cur_msg);
                    else
                    if (!strcmp(in_buf.data.record_type,SHUTDOWN_TYPE))
                        do_shutdown(cur_msg);
                    else
                    {
/*
 * It must be a DATA. Note that because of a design compromise with 4C,
 * who wanted a single DATA message type with the other message types
 * encapsulated, the data messages are actually treated
 * as pairs of messages; a DATA, which carries the transaction ID,
 * and a further message, which inherits its ID (seq) from the DATA
 */
                        alarm_preempt();
                        if ((rec_type = siinrec(cur_link->read_connect_file,
                                        &in_rest, (E2_FILE *) NULL))
                               == (char *) NULL)
                        {                      /* Garbage on the link */
                            static char * x[]= { "Couldn't find data on link",
                                                 "",
                                                 " re-initialising\n"};
                            char x1[10];
                            int i = ~(1 << (cur_link->connect_fd % 32));
                            perror("Unexpected siinrec() failure");
                            alarm_restore();
                            (void) sprintf(x1,"%d",cur_link->link_id);
                            x[1] = x1;
                            scilog(3,x);
                            write_mask[(cur_link->connect_fd/32)] &= i;
                            except_mask[(cur_link->connect_fd/32)] &= i;
                            link_close(cur_link);
                            scicommit();
                            if ( cur_link->allowed_to_call == 'Y')
                            {
                                cur_link->allowed_to_call = 'N';
                                add_time(cur_link,e2com_base.retry_time);
                            }
                            link_open(cur_link);    /* start again */
                            msg_destroy(cur_msg);   /* Finished with it */
                        }
                        else
                        {
                            alarm_restore();
                            *(cur_msg->msg_data) = in_rest;
                            cur_msg->msg_type = strdup(rec_type);
                            do_data(cur_msg);
                        }
                    }
                } 
            }
        }
 /*
  * Loop - process the links in turn, for write descriptors
  */
        for (cur_link = link_det;
                   cur_link->link_id != 0;
                             cur_link++)
        {
            if (cur_link->connect_fd != -1 &&
                 (write_mask[(cur_link->connect_fd/32)]
                              & (1 << (cur_link->connect_fd % 32))))
            {
                except_mask[(cur_link->connect_fd/32)]
                              &= ~(1 << (cur_link->connect_fd % 32));
                if (cur_link->in_out == 'I')
                {             /* We can send something */
                    if ((cur_msg = link_fetch(cur_link)) == (E2MSG *) NULL)
                    {
                        static char * x[]= {
          "Logic Error: selected for write with nothing to send, link ",
                                             "" };
                        char x1[10];
                        (void) sprintf(x1,"%d",cur_link->link_id);
                        x[1] = x1;
                        scilog(2,x);
                        write_mask[(cur_link->connect_fd/32)]
                                     &= ~(1 << (cur_link->connect_fd % 32));
                        continue;
                    }
                    cur_msg->status = MSENT;
                    changemsg(cur_msg);
                    if (cur_link->last_out_sent + 1
                                 == cur_msg->seq)
                    {
                        cur_msg->msg_link->last_out_sent++;
                        scilinkupd(cur_link);
                    }
                    scicommit();
/*
 * DATA messages go in two tranches
 */
                    if (strcmp(cur_msg->msg_type,SHUTDOWN_TYPE) &&
                        strcmp(cur_msg->msg_type,EXCHANGE_STATUS_TYPE))
                    {
                        union all_records out_data;

                        strcpy(out_data.data.record_type, DATA_TYPE);
                        (void) sprintf(out_data.data.sequence,
                                       "%-*d", SEQUENCE_LEN, cur_msg->seq);
                        (void) sioutrec(cur_link->write_connect_file,
                                  &out_data, (E2_FILE *) NULL);
                    }
                    if (e2com_base.debug_level > 1)
                        (void) fprintf(e2com_base.errout,
                              "Message Type %s sent on %d\n",
                                  cur_msg->msg_data->send_file.record_type,
                          cur_link->link_id);
                    if (!sioutrec(cur_link->write_connect_file,
                                  cur_msg->msg_data, (E2_FILE *) NULL) ||
                        !strcmp(cur_msg->msg_type,SHUTDOWN_TYPE))
                    {         /* Write failed; restart;
                                 Shutdown, do not restart */
                        if (!strcmp(cur_msg->msg_type,SHUTDOWN_TYPE))
                            cur_link->allowed_to_call = 'N';
                        link_close(cur_link);
                        scicommit();
                        if (strcmp(cur_msg->msg_type,SHUTDOWN_TYPE))
                        {
                            if ( cur_link->allowed_to_call == 'Y')
                            {
                                cur_link->allowed_to_call = 'N';
                                add_time(cur_link,e2com_base.retry_time);
                            }
                            link_open(cur_link);
                        }
                    }
                    msg_destroy(cur_msg);
                }             /* Else, we have a connect() succeeding */
                else
                     link_boot(cur_link);
            }
        }
 /*
  * Loop - process the links in turn, for except descriptors
  * (if there are going to be any?)
  */
        for (cur_link = link_det;
                   cur_link->link_id != 0;
                             cur_link++)
        {
            if (cur_link->connect_fd != -1 &&
                 (except_mask[(cur_link->connect_fd/32)]
                          & (1 << (cur_link->connect_fd % 32))))
            {                        /* Bounce the link */
                if (e2com_base.debug_level > 0)
                    (void) fprintf(e2com_base.errout,"Exception Condition on %d\n",
                          cur_link->link_id);
                link_close(cur_link);
                scicommit();
                if ( cur_link->allowed_to_call == 'Y')
                {
                    cur_link->allowed_to_call = 'N';
                    add_time(cur_link,e2com_base.retry_time);
                }
                link_open(cur_link);
            }
        }
    }   /* End of File Descriptor Processing */
    else
    if (i == 0 && errno != EINTR )
    {    /* Unexpected select() return of zero!? */

        perror("Unexpected select() exit");
        (void) fprintf(e2com_base.errout,"Select Returned Zero without Poll\n");
        (void) fprintf(e2com_base.errout,"=================================\n");
        fprintf(e2com_base.errout,
          "read_mask: 0x%x:%x  write_mask: 0x%x:%x  except_mask: 0x%x:%x\n",
                   read_mask[0],read_mask[1],write_mask[0],
                  write_mask[1],except_mask[0],except_mask[1]);
    }
    else
    {    /* Unexpected select() event */
        char * x ="Unexpected select() exit\n";
        perror("Unexpected select() exit");
        scilog(1,&x);
    }
}   /* Bottom of Infinite for loop */
}
/*
 * Process an incoming EXCHANGE_STATUS
 */
void do_exchange_status(cur_msg)
E2MSG * cur_msg;
{      /* See if we are in or out of synchronisation */
    LINK * cur_link;
    LINK other_link;
    static char * x[5] = { "Error on Link ","",
                  " EXCHANGE STATUS, their sequence ","","" };
    x[1] = cur_msg->msg_data->exchange_status.link_id;
    x[3] = cur_msg->msg_data->exchange_status.sequence;
    cur_link = cur_msg->msg_link;

    if (e2com_base.debug_level > 1)
    (void) fprintf(e2com_base.errout,"EXCHANGE STATUS recognised on %d\n",
      cur_link->link_id);

    createmsg(cur_msg); /* Save it on the database */
    other_link.link_id =
        atoi(cur_msg->msg_data->exchange_status.link_id);
    other_link.last_out_sent =
        atoi(cur_msg->msg_data->exchange_status.last_out_sent);
    other_link.last_out_alloc =
        atoi(cur_msg->msg_data->exchange_status.last_out_alloc);
    other_link.last_in_rec =
        atoi(cur_msg->msg_data->exchange_status.last_in_rec);
    other_link.earliest_in_unread =
        atoi(cur_msg->msg_data->exchange_status.last_file_unread);
    other_link.dest_ftp_user_id =
        strdup(cur_msg->msg_data->exchange_status.dest_ftp_user_id);
    other_link.dest_ftp_pass =
        strdup(cur_msg->msg_data->exchange_status.dest_ftp_pass);
    if (other_link.last_out_alloc < cur_link->last_in_rec)
    {
        x[4]="We have more than the Other End has created!";
        scilog(5,x);

        if (e2com_base.debug_level > 0)
            (void) fprintf(e2com_base.errout,
                        "Link %d We Received: %d They Created: %d\n",
                    cur_link->link_id,
                    cur_link->last_in_rec,other_link.last_out_sent);
    }
    if (other_link.last_in_rec > cur_link->last_out_alloc)
    {
        x[4]="Other End has got more than we have created";
        scilog(5,x);
        if (e2com_base.debug_level > 0)
            (void) fprintf(e2com_base.errout,
                  "Link %d We Created: %d They Received: %d\n",
                    cur_link->link_id,
                    cur_link->last_out_alloc,other_link.last_in_rec);
    }
/*
 * This needs thought.
 */
    if (other_link.last_out_alloc < cur_link->last_in_rec
    ||  other_link.last_in_rec > cur_link->last_out_alloc)
    {  /* Cannot restart. Human intervention is needed */
        cur_link->allowed_to_call = 'N';
                    /* Needs manual fix-up; stop retries */
        (void) free(other_link.dest_ftp_user_id);
        (void) free(other_link.dest_ftp_pass);
        link_close(cur_link);
        cur_msg->status = MDONE;
        changemsg(cur_msg);
        scicommit();
        link_open(cur_link);
    }
    else
    {   /* Get the show back on the road */
        E2MSG * res_msg;
        cur_link->last_out_sent =
                  other_link.last_in_rec;
        cur_link->earliest_out_unread =
                  other_link.earliest_in_unread;
        cur_link->cur_ftp_user_id =
                  other_link.dest_ftp_user_id;
        cur_link->cur_ftp_pass =
                  other_link.dest_ftp_pass;
        scilinkres(cur_link);
        NEW(E2MSG,res_msg);
        cur_link->t_offset = 0;
        res_msg->link_id = cur_link->link_id;
        res_msg->msg_link = cur_link;
        while(getnextmsg(res_msg))
        {
            if (!strcmp(res_msg->msg_type, EXCHANGE_STATUS_TYPE)
             || !strcmp(res_msg->msg_type, SHUTDOWN_TYPE))
            {
                res_msg->status = MDONE;
                changemsg(res_msg);
                msg_destroy(res_msg);
                           /* Don't resend these! */
            }
            else
                link_add(res_msg);
            NEW(E2MSG,res_msg);
            res_msg->link_id = cur_link->link_id;
            res_msg->msg_link = cur_link;
        }
        msg_destroy(res_msg);
        cur_msg->status = MDONE;
        changemsg(cur_msg);
        scicommit();
        link_replay(cur_link);
                        /* re-run incomplete transactions at our end */
    }
    msg_destroy(cur_msg);
                        /* we have no further use for it */
    return;
}
/*
 * Process an incoming SHUTDOWN
 */
void do_shutdown(cur_msg)
E2MSG * cur_msg;
{      /* Link Close; perform an orderly shutdown */
    LINK other_link, *cur_link;
    static char * x[5] = { "Error on Link ","",
                  "SHUTDOWN, their sequence ","","" };
    x[1] = cur_msg->msg_data->shutdown.link_id;
    x[3] = cur_msg->msg_data->shutdown.sequence;
    cur_link = cur_msg->msg_link;
    if (e2com_base.debug_level > 1)
    (void) fprintf(e2com_base.errout,"SHUTDOWN recognised on %d\n",
      cur_link->link_id);
    createmsg(cur_msg); /* Save it on the database */
    other_link.link_id = atoi(cur_msg->msg_data->shutdown.link_id);
    link_close(cur_link);
    cur_msg->status = MDONE;
    changemsg(cur_msg);
    scicommit();
    msg_destroy(cur_msg);
                        /* we have no further use for it */
    return;
}
/*
 * Process the body of a DATA message
 */
void do_data(cur_msg)
E2MSG * cur_msg;
{
    LINK * cur_link = cur_msg->msg_link ;
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,
                  "DATA Envelope %s recognised on %d\n",
                   DATA_TYPE,
                   cur_link->link_id);
    if (!strcmp(cur_msg->msg_type,SEND_FILE_TYPE))
    {
        if (e2com_base.debug_level > 2)
            (void) fprintf(e2com_base.errout,
                   "SEND FILE recognised on %d\n",
                   cur_link->link_id);
        cur_msg->send_file_name =
             strdup(cur_msg->msg_data->send_file.send_file_name);
        (void) free(cur_msg->msg_type);
        cur_msg->msg_type =
             strdup(cur_msg->msg_data->send_file.send_file_type);
        createmsg(cur_msg);
                          /* Save it on the database */
        proc_send_file(cur_msg);
        scicommit();
    }
    else
    {
        if (e2com_base.debug_level > 2)
            (void) fprintf(e2com_base.errout,
             "Message Type %s recognised on %d\n",
                  cur_msg->msg_data->send_file.record_type,
                  cur_link->link_id);
        createmsg(cur_msg);
        proc_other_msg(cur_msg);   /* Does a Commit */
    }
    return;
}
/***************************************************************************
 * add a child process to the list;
 * overflow handled safely, if with degraded functionality
 */
void add_child(pid,msg)
int pid;
E2MSG * msg;
{
register RUN_CHILD * cur_child_ptr=child_stat, * max_child_ptr
    = &child_stat[MAXLINKS *SIMUL -1];
LINK * link_ptr;

    if (msg == (E2MSG *) NULL || msg->msg_link == (LINK *) NULL)
    {
        char * x = "Logic Error: add_child() called with NULL msg or link";
        scilog(1,&x);
        return;
    }
    link_ptr = msg->msg_link;
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"add_child() for link %d\n",
                          link_ptr->link_id);
    while(cur_child_ptr < max_child_ptr)
    {
         if (cur_child_ptr->child_pid == 0)
         {
             cur_child_ptr->child_pid = pid;
             link_ptr->cur_simul++;
             cur_child_ptr->own_msg = msg;
             child_cnt++;
             return;
         }
         cur_child_ptr++;
    }
    return;
}
/*
 * Routine to generate an acknowledgement message
 *
 * All confirms have the same basic layout, so do not need to know them all
 */
void ackmsg(msg)
E2MSG * msg;
{
    E2MSG * ack_msg;
    if (msg == (E2MSG *) NULL)
    {
        char * x = "Logic Error: ackmsg() called with NULL msg";
        scilog(1,&x);
        return;
    }
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"ackmsg() for link %d\n",
                          msg->msg_link->link_id);
    ack_msg = msg_struct_ini(msg->mtp->confirm_record_type, TRANSMIT);
    ack_msg->link_id = msg->link_id;
    ack_msg->msg_link = msg->msg_link;
    ack_msg->success = msg->success;
    ack_msg->their_seq = msg->seq;
    (void) sprintf(ack_msg->msg_data->confirm.sequence_confirmed,
                        "%-*d", CONFIRM_SEQUENCE_LEN, ack_msg->their_seq);
    (void) sprintf(ack_msg->msg_data->confirm.success,
                        "%c", ack_msg->success);
    if (msg->error_message == (char *) NULL)
        (void) sprintf(ack_msg->msg_data->confirm.explanation,
                        "%-*s", EXPLANATION_LEN,"");
    else
        (void) sprintf(ack_msg->msg_data->confirm.explanation,
                        "%-*s", EXPLANATION_LEN,msg->error_message);
    link_add(ack_msg);
    createmsg(ack_msg);
    return;
}
/*
 * Routine to generate a SHUTDOWN message
 */
void shutmsg(link)
LINK * link;
{
    E2MSG * shut_msg;
    if (link == (LINK *) NULL)
    {
        char * x = "Logic Error: shutmsg() called with NULL LINK";
        scilog(1,&x);
        return;
    }
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"shutmsg(%d)\n", link->link_id);
    shut_msg = msg_struct_ini(SHUTDOWN_TYPE, TRANSMIT);
    shut_msg->link_id = link->link_id;
    shut_msg->msg_link = link;
    shut_msg->their_seq = 0;
    shut_msg->success = SUCCESS;
    (void) sprintf(shut_msg->msg_data->shutdown.link_id,
                        "%-*d", LINK_ID_LEN,shut_msg->link_id);
    link_add(shut_msg);
    createmsg(shut_msg);
    return;
}
/*
 * Routine to feed SHUTDOWN messages to all links
 */
void begin_shutdown()
{
    LINK * link;
    reset_time();   /* Kill any potential retries */
    for (link = link_det;
            link->link_id != 0;
                link++)
        link_shut(link);
    scicommit();
    glob_state = SHUTDOWN_PROGRESSING;
    return;
}
/*
 * remove a child process from the list;
 *
 * overflow handled safely, if with degraded functionality
 *
 * - fprintf() and scilog() may lead to some funnies, since working on
 *   static data
 *
 * rem_child() works with proc_send_file().
 *
 * rem_child():
 * - Updates the message status
 * - Queues a Confirm, if one is defined
 * - Calls proc_send_file() to initiate the next step.
 */
void rem_child(pid,pidstatus)
int pid;
#ifdef POSIX
int
#else
union wait
#endif
pidstatus;
{
E2MSG *msg;
register RUN_CHILD * cur_child_ptr=child_stat, * max_child_ptr
    = &child_stat[MAXLINKS *SIMUL -1];
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"rem_child() for pid %d\n", pid);
    if (pid == fifo_pid)
    {
        char * x = "FIFO handler shut down\n";
        scilog(1,&x);
    }
    else
    while(cur_child_ptr < max_child_ptr)
    {
         if (cur_child_ptr->child_pid == pid)
         {
             char *x;
             char * mess;
             int y;
             if (WIFEXITED(pidstatus))
             {
                 mess = "exiting with status";
#ifdef POSIX
                 y = WEXITSTATUS(pidstatus);
#else
                 y = pidstatus.w_retcode;
#endif
             }
             else /* Terminated by signal */
             {
                 mess = "terminated by signal";
#ifdef POSIX
                 y = WTERMSIG(pidstatus);
#else
                 y = pidstatus.w_termsig;
#endif
             }
             msg = cur_child_ptr->own_msg;
             cur_child_ptr->child_pid = 0;
             cur_child_ptr->own_msg = (E2MSG *) NULL;
             (void) sprintf(fifo_buf,
                   "pid %d:SEND_FILE %s from %s:%s finished %s %d",
                   pid,
                   msg->receive_file_name,
                   (msg->msg_link->cur_host_name != (char *) NULL) ?
                   msg->msg_link->cur_host_name: "unspecified",
                   msg->send_file_name,
                   mess, y);
             (void) fwrite(fifo_buf,sizeof(char),strlen(fifo_buf),
                           e2com_base.errout);
             (void) fprintf(e2com_base.errout,"\n");
             x=fifo_buf;
             scilog(1,&x);
             msg->msg_link->cur_simul--;
             child_cnt--;
#ifdef POSIX
             if (WEXITSTATUS(pidstatus))
#else
             if (pidstatus.w_status)
#endif
             {          /* Failed; wait for a restart before retrying */
                 msg_destroy(msg);
             }
/*
 * Now see what has to be done next
 */
             else
             if (msg->status == MCREATED)
             {        /* rcp has finished */
                 msg->success = SUCCESS;
                 if (msg->error_message != (char *) NULL)
                 {
                     (void) free(msg->error_message);
                     msg->error_message = (char *) NULL;
                 }
                 if (msg->mtp->confirm_record_type != (char *) NULL &&
                     strlen(msg->mtp->confirm_record_type)) 
                      ackmsg(msg);  /* Null error message */
                 msg->status = MACKED;
                 changemsg(msg);
                 if (msg->seq == msg->msg_link->last_in_rec + 1)
                 {
                     msg->msg_link->last_in_rec++;
                     scilinkupd(msg->msg_link);
                 }
                 scicommit();
                 proc_send_file(msg);         /* Initiate the next step */
             }
             else /* MACKED */
             {        /* prog_to_fire has finished */
                 msg->status = MDONE;
                 changemsg(msg);
                 scicommit();
                 msg_destroy(msg);
             }
             return;
         }
         cur_child_ptr++;
    }
    return;
}
/*
 * Find the child processing a msg in the list
 * Not fool-proof if multiple links; matches only on the message sequence.
 * Probably won't be used, however.
 */
RUN_CHILD * find_msg(seq)
int seq;
{
register RUN_CHILD * cur_child_ptr=child_stat, * max_child_ptr
    = &child_stat[MAXLINKS *SIMUL -1];
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"find_msg(%d)\n", seq);
    while(cur_child_ptr < max_child_ptr)
    {
         if (cur_child_ptr->own_msg != (E2MSG *) NULL &&
                 cur_child_ptr->own_msg->seq == seq)
             return cur_child_ptr;
         cur_child_ptr++;
    }
    return (RUN_CHILD *) NULL;
}
/***************************************************************************
 * Clock functions
 *
 * add_time();  add a new time for the link; start the clock if not running
 * This function moves the buffer head, but not its tail.
 *  - new_time is an absolute time in seconds since 1970.
 *  - do not add it if the link is already queued for a retry.
 */
void add_time(link,delta)
LINK * link;
int delta;
{
    short int cur_ind;
    long t;
    struct go_time sav_time;
    long new_time;
    t = time((long *) 0);
    new_time = t + delta;
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"add_time(): link %d delta %d\n",
                  (link == (LINK *) NULL)? 0 : link->link_id,
                  delta);
    for (cur_ind = tail;
             cur_ind !=head && go_time[cur_ind].go_time < new_time;
                 cur_ind = (cur_ind + 1) % MAXLINKS)
         if (go_time[cur_ind].link == link) return;  /* shouldn't happen */
    for (; cur_ind != head; cur_ind = (cur_ind + 1) % MAXLINKS)
    {
        sav_time = go_time[cur_ind];
        go_time[cur_ind].go_time = new_time;
        go_time[cur_ind].link = link;
        new_time = sav_time.go_time;
        link = sav_time.link;
    }
    if (tail != (head + 1) % MAXLINKS)
    {
        go_time[head].go_time = new_time;
        go_time[head].link = link;
        head = (head + 1) % MAXLINKS;
    }
    sighold(SIGALRM);
    if (clock_running != 0)
        alarm(0);
    sigrelse(SIGALRM);
    clock_running = 0;
    rem_time();
    return;
} 
/*
 * rem_time(); tidy up the list, removing times from the tail as they
 * expire. Start the clock if there is anything in the link. Apart from
 * reset_time(), nothing else moves the tail.
 *
 * This function is NEVER called if the clock is running.
 *
 * I don't think there will be be any problems with the link_open() call messing
 * up whatever was executing when the alarm clock rang. If there are,
 * then we will make the alarm clock signal routine set a flag that
 * can be inspected by the main line code at a safe working point. This had
 * to be done for the death of child processing, and would be essential
 * if this code or any that it called attempted to access the ORACLE database.
 */
void rem_time()
{
    short int cur_ind;
    int sleep_int;
    long cur_time = time((long *) 0);
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"rem_time(): Clock Running %d\n",clock_running);
    for (cur_ind = tail;
             cur_ind !=head && go_time[cur_ind].go_time <= cur_time;
                 cur_ind = (cur_ind + 1) % MAXLINKS,
                 tail = cur_ind)
    {                      /* Attempt to reconnect on the links indicated */
        if (go_time[cur_ind].link != (LINK *) NULL)
        {
            (go_time[cur_ind].link)->allowed_to_call = 'Y';
            (void) link_open(go_time[cur_ind].link);
        }
    }
    if (tail != head)
    {
        (void) sigset(SIGALRM,rem_time);
        sleep_int = go_time[tail].go_time - cur_time;
        clock_running ++;
        (void) alarm(sleep_int);
    }
    return;
} 
/*
 * Reset the time buffers
 */
void reset_time()
{
    alarm(0);
    sigset(SIGALRM,SIG_IGN);
    tail = head;
    clock_running = 0;
    return;
}
/*
 * Routine to temporarily pre-empt the normal clock handling
 */
void alarm_preempt()
{
    prev_alrm = sigset(SIGALRM,read_timeout);
    alarm_save = alarm(poll_int.tv_sec);
    return;
}
/*
 * Routine to restore it
 */
void alarm_restore()
{
    alarm(0);
    (void) sigset(SIGALRM,prev_alrm);
    if (clock_running)
    {
        sighold(SIGALRM);
        (void) alarm(alarm_save);
    }
    return;
}

/****************************************************
 * Populate the link_det array
 */
static void ini_link()
{
    LINK * w_link;

    scilinkin();
    for ( link_cur_ptr = link_det;

               (w_link = getlinkent()) != (LINK *) NULL; )
    {
        *(link_cur_ptr) = *w_link;
        (void) free(w_link);
        link_cur_ptr->cur_simul = 0;
        link_cur_ptr->first_msg = (E2MSG *) NULL;
        link_cur_ptr->last_msg = (E2MSG *) NULL;
        link_open(link_cur_ptr);
        if (link_cur_ptr++ == link_max_ptr) break;
    }
    link_cur_ptr->link_id = 0;      /* zero link number marks end of list */
    if (e2com_base.debug_level > 1)
    dump_link();
    return;
}
/*
 * Function to print out data about the links
 */
static void dump_link()
{
    short int j;
    E2MSG * msg;
    for (j=0; link_det[j].link_id != 0; j++)
    {
(void) fprintf(e2com_base.errout,"link_id: %d\n",link_det[j].link_id);
(void) fprintf(e2com_base.errout,"dest_host_name: %s\n",(link_det[j].dest_host_name
        == (char *) NULL)? "":link_det[j].dest_host_name);
(void) fprintf(e2com_base.errout,"dest_ftp_user_id: %s\n",link_det[j].dest_ftp_user_id);
(void) fprintf(e2com_base.errout,"dest_ftp_pass: %s\n",link_det[j].dest_ftp_pass);
(void) fprintf(e2com_base.errout,"cur_host_name: %s\n",(link_det[j].cur_host_name ==
        (char *) NULL)?"": link_det[j].cur_host_name);
(void) fprintf(e2com_base.errout,"cur_ftp_user_id: %s\n",(link_det[j].cur_ftp_user_id ==
        (char *) NULL)?"": link_det[j].cur_ftp_user_id);
(void) fprintf(e2com_base.errout,"cur_ftp_pass: %s\n",(link_det[j].cur_ftp_pass ==
        (char *) NULL)?"": link_det[j].cur_ftp_pass);
(void) fprintf(e2com_base.errout,"allowed_to_call: %c\n",link_det[j].allowed_to_call);
(void) fprintf(e2com_base.errout,"max_simul: %d\n",link_det[j].max_simul);
(void) fprintf(e2com_base.errout,"cur_simul: %d\n",link_det[j].cur_simul);
(void) fprintf(e2com_base.errout,"last_out_alloc: %d\n",link_det[j].last_out_alloc);
(void) fprintf(e2com_base.errout,"last_out_sent: %d\n",link_det[j].last_out_sent);
(void) fprintf(e2com_base.errout,"last_in_rec: %d\n",link_det[j].last_in_rec);
(void) fprintf(e2com_base.errout,"link_up_y_or_n: %c\n",link_det[j].link_up_y_or_n);
(void) fprintf(e2com_base.errout,"in_out: %c\n",link_det[j].in_out);
(void) fprintf(e2com_base.errout,"connect_fd: %d\n",link_det[j].connect_fd);
(void) fprintf(e2com_base.errout,"Outgoing Messages........\n");
(void) fprintf(e2com_base.errout,"=========================\n");
        for (msg = link_det[j].first_msg;
               msg != (E2MSG *) NULL;
                    msg = msg->next_msg)
          (void) fprintf(e2com_base.errout,"%10.1ld %-16.16s\n", msg->seq,
                                 msg->msg_data->send_file.record_type);
        (void) fprintf(e2com_base.errout,"=========================\n");
    }
    return;
}
/*
 * Function to dump running msg processes
 */
static void dump_children()
{
register RUN_CHILD * cur_child_ptr=child_stat, * max_child_ptr
    = &child_stat[MAXLINKS *SIMUL -1];
short int i;
    
(void) fprintf(e2com_base.errout,
"Host                             Link        Sequence   Type            PID\n");
    for (i=0;cur_child_ptr < max_child_ptr;cur_child_ptr++)
    {
         if (cur_child_ptr->child_pid != 0)
         {
             i++;
            (void) fprintf(e2com_base.errout,"%-33.33s%10.1ld %10.1ld %-16.16s %10.1ld\n",
                   cur_child_ptr->own_msg->msg_link->dest_host_name,
                   cur_child_ptr->own_msg->link_id,
                   cur_child_ptr->own_msg->seq,
                   cur_child_ptr->own_msg->msg_data->send_file.record_type,
                   cur_child_ptr->child_pid);
         }
    }
    if (child_cnt != i)
         (void) fprintf(e2com_base.errout,
               "Warning: Link children %d != internal count %d\n",
                  i, child_cnt);
    return;
}
/***********************************************************************
 * natqlib; Routines to open, fetch and close a link
 *
 * This code is here, rather than with the service routines, e2clib.c to
 * limit the whereabouts of the socket-related system calls.
 *
 */ 
static char * orig2_id="@(#)natqlib.c   1.1 91/07/04 19:00:45 91/07/03 18:59:54\nCopyright (C) SO systems 1991";

/************************************************************************
 * listen set up
 */
void e2com_listen()
{
    struct servent *e2com_servent,
    *getservbyname();

    struct hostent *e2com_host,
    *gethostbyname();

    struct protoent *e2com_prot,
    *getprotobyname();

    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"e2com_listen()\n");
    e2com_host =gethostbyname(e2com_base.home_host);
    e2com_prot=getprotobyname(e2com_base.e2com_protocol);
    if (e2com_host == (struct hostent *) NULL ||
        e2com_prot == (struct protoent *) NULL)
    { 
        char * x = "Logic Error; no e2com host or protocol!\n";
        scilog(1,&x);
        (void) unlink(perf_global.fifo_name);
        (void) unlink(perf_global.lock_name);
        sciabort();
    }
    /*    Construct the Socket Addresses    */
    /*    The socket to listen on        */

    memset((char *) &listen_sock,0,sizeof(listen_sock));
    listen_sock.sin_family = e2com_host->h_addrtype;
    if ((e2com_servent = getservbyname(e2com_base.e2com_service,
           e2com_base.e2com_protocol))
        == (struct servent *) NULL)
    { 
        char * x = "Logic Error; no e2com service!\n";
        scilog(1,&x);
        (void) unlink(perf_global.fifo_name);
        (void) unlink(perf_global.lock_name);
        sciabort();
    }
    listen_sock.sin_port   = e2com_servent->s_port;
    listen_sock.sin_addr.s_addr = INADDR_ANY;

    /*    Now create the socket to listen on    */

    if ((listen_fd=
         socket(AF_INET,SOCK_STREAM,e2com_prot->p_proto))<0)
    { 
        char * x = "Listen socket create failed\n" ;
        scilog(1,&x);
        perror("Listen socket create failed"); 
        (void) unlink(perf_global.fifo_name);
        (void) unlink(perf_global.lock_name);
        sciabort();
    }
    setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,1,0);
    /*    Bind its name to it            */
    if (bind(listen_fd,
             &listen_sock,sizeof(listen_sock)))
    { 
        char * x = "Listen bind failed\n"; 
        scilog(1,&x);
        perror("Listen bind failed"); 
        (void) unlink(perf_global.fifo_name);
        (void) unlink(perf_global.lock_name);
        sciabort();
    }

    /*    Declare it ready to accept calls    */

    if (listen(listen_fd,MAXLINKS))
    {
        char * x = "Listen failed\n";
        scilog(1,&x);
        perror("Listen failed"); 
        (void) unlink(perf_global.fifo_name);
        (void) unlink(perf_global.lock_name);
        sciabort();
    }
    (void) set_io_pid(listen_fd);
    return;
}
/************************************************************************
 * Find the link, given the link_id
 */
LINK * link_find(link_id)
int link_id;
{
    LINK * cur_link;
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"link_find(%d)\n",link_id);
    if (link_id == 0)
        return (LINK *) NULL;
    for (cur_link = link_det;
                cur_link->link_id != 0;
                     cur_link++)
    if (link_id == cur_link->link_id)
        return cur_link;
    return (LINK *) NULL;
}
/************************************************************************
 * Open a link to get at the msg records
 * - Returns 0 if fail, 1 if success
 * - Fills in the socket stuff.
 * - Sets up a calling socket if it is allowed to.
 */
int link_open(link)
LINK * link;
{
    /*    Initialise - use input parameters to set up listen port, and
          address of port to connect to
        -    Data Definitions */

    struct hostent  *connect_host,
    *gethostbyname();

    struct protoent *e2com_prot,
    *getprotobyname();
    if (link == (LINK *) NULL)
    {
        char * x = "Logic Error: link_open() called with NULL link";
        scilog(1,&x);
        return 0;
    }
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"link_open()\n");

    e2com_prot=getprotobyname(e2com_base.e2com_protocol);

    link->connect_fd = -1;
    if (link->dest_host_name == (char *) NULL || !strlen(link->dest_host_name))
        return 0;
    connect_host=gethostbyname(link->dest_host_name);
    if (connect_host == NULL)
    { 
        static char * x[2] = {"Logic Error; no link host\n",""};
        x[1]=link->dest_host_name;
        scilog(2,x);
        link->allowed_to_call = 'N';
    }
    else
    {
/*    The socket to connect to */

         memset((char *) &link->connect_sock,0,sizeof(link->connect_sock));
         link->connect_sock.sin_family = connect_host->h_addrtype;
         link->connect_sock.sin_port   = listen_sock.sin_port;
         memcpy((char *) &link->connect_sock.sin_addr,
                (char *) connect_host->h_addr, 
                    connect_host->h_length);

    }
    if (link->allowed_to_call == 'Y')
    {
        int fflags;
        /*    Now create the socket to output on    */
        if ((link->connect_fd =
              socket(AF_INET,SOCK_STREAM,e2com_prot->p_proto)) < 0)
        {
            char * x = "Output create failed\n";
            scilog(1,&x);
            perror("Output create failed");
            link->allowed_to_call = 'N';
            add_time(link,e2com_base.retry_time);
        }
        else
        {
            fflags = fcntl(link->connect_fd,F_GETFL,0);
        /* Set non-blocking, so can select() for write after the connect() */
            if (fcntl(link->connect_fd,F_SETFL,fflags | O_NDELAY) == -1)
            {         /* FNDELAY is not defined if POSIX;
                         this has the same value as FNDELAY, O_NONBLOCK */
                char * x = "Output fcntl() Non-blocking failed";
                scilog(1,&x);
                perror("Output fcntl() Non-blocking failed");
                link->allowed_to_call = 'N';
                (void) close(link->connect_fd);
                link->connect_fd = -1;
                add_time(link,e2com_base.retry_time);
            }
            else
            {                  /*    Start the Connect to the destination */
                if (connect(link->connect_fd,
                   &link->connect_sock,sizeof(link->connect_sock)) &&
                      errno != EINPROGRESS)
                {
                     char * x = "Initial connect() failure\n";
                     scilog(1,&x);
                     perror("connect() failed");
                     (void) close(link->connect_fd);
                     link->connect_fd = -1;
                     link->allowed_to_call = 'N';
                     add_time(link,e2com_base.retry_time);
                                        /* Queue another attempt */
                }
                else
                {
                    fflags = fcntl(link->connect_fd,F_GETFL,0);
                    if (fcntl(link->connect_fd,F_SETFL,fflags & ~O_NDELAY)
                              == -1)
                    {         /* Want to put it back to block, so do not
                                 get zero reads every time from the select() */
                        char * x = "Output fcntl() Blocking failed";
                        scilog(1,&x);
                        perror("Output fcntl() Non-blocking failed");
                        link->allowed_to_call = 'N';
                        (void) close(link->connect_fd);
                        link->connect_fd = -1;
                        add_time(link,e2com_base.retry_time);
                    }
                    else
                    if (enable_file_io(link))
                    {
                        link->in_out = 'O';    /* so we know the file descriptor
                                                  is connect() and not accept()
                                                */
                        (void) set_io_pid(link->connect_fd);
                    }
                }
            }
        }
        return 1;
    }
    else
        return 0;
}
/*
 * Routine to set up FILE pointers for a connect_fd on a link
 */
int enable_file_io(link)
LINK * link;
{
    if (link == (LINK *) NULL)
    {
        char * x = "Logic Error: enable_file_io() called with NULL link";
        scilog(1,&x);
        return 0;
    }
    if ((link->read_connect_file = fdopen(link->connect_fd,"r")) ==
                (FILE *) NULL)
    { 
        char * x = "Failed to associate read FILE with fd\n";
        scilog(1,&x);
        perror("Failed to associate read FILE with fd");
        link->allowed_to_call = 'N';
        (void) close(link->connect_fd);
        link->connect_fd = -1;
        return 0;
    }
/*    Associate a FILE pointer with it, so that sioutrec() can be used on it */
    else if ((link->write_connect_file =
              fdopen(link->connect_fd,"w")) == (FILE *) NULL)
    { 
        char * x = "Failed to associate write FILE with fd\n";
        scilog(1,&x);
        perror("Failed to associate write FILE with fd");
        link->allowed_to_call = 'N';
        (void) fclose(link->read_connect_file);
        link->connect_fd = -1;
        return 0;
    }
    else
    {
        (void) setbuf(link->read_connect_file,(char *) NULL);
                            /* Unbuffered Input */
        (void) setbuf(link->write_connect_file,(char *) NULL);
                            /* Unbuffered Output */
    }
    return 1;
}
/*
 * Routine to get a link on the road, after a connection has been made.
 */
void link_boot(link)
LINK * link;
{
    E2MSG * cur_msg;
    if (link == (LINK *) NULL)
    {
        char * x = "Logic Error: link_boot() called with NULL link";
        scilog(1,&x);
        return;
    }
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"link_boot(): Booting Link %d\n",link->link_id);
    if (link->cur_host_name == (char *) NULL)
        if (link == (LINK *) NULL)
        {
            char * x = "Logic Error: link_boot() called with NULL host";
            scilog(1,&x);
            return;
        }
        else link->cur_host_name = strdup(link->dest_host_name);
    link->in_out = 'I';
    link->link_up_y_or_n = 'Y';
    scilinksyn(link);     /* Tidy up e2com_control and e2com_journal */
    while ((cur_msg = link_fetch(link)) != (E2MSG *) NULL)
       msg_destroy(cur_msg);      /* Bin any messages that are still hanging
                                     around */
    cur_msg = msg_struct_ini(EXCHANGE_STATUS_TYPE,TRANSMIT);
    (void) sprintf(cur_msg->msg_data->exchange_status.link_id,
                   "%-*d",LINK_ID_LEN,
                   link->link_id);
    (void) sprintf(cur_msg->msg_data->exchange_status.host_name,
                   "%-*s",HOST_NAME_LEN,e2com_base.home_host);
    (void) sprintf(
               cur_msg->msg_data->exchange_status.max_simul,
               "%-*d",MAX_SIMUL_LEN,link->max_simul);
    (void) sprintf(
            cur_msg->msg_data->exchange_status.last_out_alloc,
            "%-*d",SEQUENCE_LEN,link->last_out_alloc);
    (void) sprintf(
            cur_msg->msg_data->exchange_status.last_out_sent,
            "%-*d",SEQUENCE_LEN,link->last_out_sent);
    (void) sprintf(
            cur_msg->msg_data->exchange_status.last_in_rec,
            "%-*d",SEQUENCE_LEN,link->last_in_rec);
    (void) sprintf(
            cur_msg->msg_data->exchange_status.last_file_unread,
            "%-*d",SEQUENCE_LEN,link->earliest_in_unread);
    (void) sprintf(cur_msg->msg_data->exchange_status.dest_ftp_user_id,
                   "%-*s",FTP_USER_ID_LEN,link->dest_ftp_user_id);
    (void) sprintf(cur_msg->msg_data->exchange_status.dest_ftp_pass,
                   "%-*s",FTP_PASS_LEN,link->dest_ftp_pass);
    cur_msg->link_id = link->link_id;
    cur_msg->msg_link = link;
    link_add(cur_msg);
    createmsg(cur_msg);
    scicommit();
    return;
}
/*
 * Replay the unactioned messages on a link (particularly incomplete
 * file transfers)
 */
void link_replay(link)
LINK * link;
{
    E2MSG * cur_msg;
    scilinksfo(link);     /* Get ready to re-action the backlog of messages */
    NEW(E2MSG,cur_msg);
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"link_replay(): Replaying Link %d\n",
                       link->link_id);
    cur_msg->msg_link = link;
    cur_msg->link_id = link->link_id;
    while(getnextsf(cur_msg))
    {
        if (!strcmp(cur_msg->msg_data->send_file.record_type,
                    SEND_FILE_TYPE))
        {
            if (link->max_simul <= link->cur_simul)
                sigpause(SIGCLD);
            chld_catcher(WNOHANG);
            sighold(SIGCLD);
            proc_send_file(cur_msg);
            if (e2com_base.debug_level > 2)
            (void) fprintf(e2com_base.errout,"link: %d cur_simul: %d max_simul %d\n",
                           cur_msg->link_id, cur_msg->msg_link->cur_simul,
                              cur_msg->msg_link->max_simul);
        }
        else
            proc_other_msg(cur_msg);
        NEW(E2MSG,cur_msg);
        cur_msg->link_id = link->link_id;
        cur_msg->msg_link = link;
    }
    msg_destroy(cur_msg);
    scicommit();
}
/*
 * Return the next msg chained on the link. Detach it as you do so.
 */
E2MSG * link_fetch(link)
LINK * link;
{
    E2MSG * msg;
    if (link == (LINK *) NULL)
    {
        char * x = "Logic Error: link_fetch() called with NULL link";
        scilog(1,&x);
        return;
    }
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"link_fetch()\n");
    if ((msg = link->first_msg) != (E2MSG *) NULL)
    {
        link->first_msg = msg->next_msg;
        if (link->first_msg == (E2MSG *) NULL)
            link->last_msg = (E2MSG *) NULL;
    }
    return msg;
}
/*
 * Add a further message to the link, at the end, and put it in the database
 * - return 1 if successful
 * - return 0 if the link does not exist
 */
int link_add(msg)
E2MSG * msg;
{
    if (msg == (E2MSG *) NULL)
    {
        char * x = "Logic Error: link_add() called with NULL msg";
        scilog(1,&x);
        return 0;
    }
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"link_add()\n");
    if (msg->msg_link == (LINK *) NULL)
    {
        if ((msg->msg_link = link_find(msg->link_id)) == (LINK *) NULL)
            return 0;
    }
    msg->next_msg = (E2MSG *) NULL;
    if (msg->msg_link->last_msg == (E2MSG *) NULL)
        msg->msg_link->first_msg = msg;
    else
        msg->msg_link->last_msg->next_msg = msg;
    msg->msg_link->last_msg = msg;
    return 1;
}
/* Routine to process an incoming SEND_FILE:
 * In General
 * -  There must exist a row in E2COM_MESS_TYPES with the incoming
 *    send_file_type (eg. 'SP') in the msg_type column.
 * -  The sub_y_or_n field must be 'N'.
 * -  An ftp is initiated.
 * -  If that completes successfully:
 *    -  If the confirm_record_type in E2COM_MESS_TYPES is not null,
 *       a CONFIRM message of this type is sent
 *    -  If the sequence is one more that the last_in_rec for the link,
 *       this is updated
 *    -  Completions out of sequence (which are possible) will be sorted
 *       out at the next link start up, by scilinksyn()
 *    -  The message status is sent to MACKED
 *    -  The program indicated in E2COM_MESS_TYPES prog_to_fire is run
 *       -  If that completes
 *          -  The message status is set to MDONE.
 *          -  If it was successful, the success code is set to 'S'
 *          -  Otherwise it is set to 'F', fail.
 *
 * This routine is called whenever a SEND_FILE needs some action:
 * - When it is initiated
 * - When it is restarted (from link_boot())
 * - When one of the processing steps finishes (from rem_child()).
 *
 * Note that rem_child() drives the change in message status, not
 * this routine.
 */
void proc_send_file(msg)
E2MSG * msg;
{
    char buf[BUFSIZ];
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,
                       "Processing Incoming SEND_FILE Link %d Sequence %d\n",
                       msg->msg_link->link_id, msg->seq);
    if (msg->mtp == (E2COM_MESS_TYPES *) NULL &&
       (msg->mtp = mtfind(msg->msg_type)) == (E2COM_MESS_TYPES *) NULL)
    {
        char * x[3];
        x[0] = "Unknown File Type";
        x[1] = msg->msg_type;
        x[2] = "in SEND_FILE";
        scilog(3,x);
        msg_destroy(msg);   /* Finished with it */
        return;
    } 
    if (msg->receive_file_name == (char *) NULL
    || strlen(msg->receive_file_name) == 0)
    {
        msg->receive_file_name = filename("R%d.%d.%s",
                   (char *) msg->link_id, (char *)  msg->seq, msg->msg_type);
    }
    if (msg->msg_type  == (char *) NULL ||
        strlen(msg->msg_type) == 0 ||
        !strcmp(msg->msg_type,SEND_FILE_TYPE))
    {
        if (msg->msg_type != (char *) NULL)
            free(msg->msg_type);
        msg->msg_type = strnsave(msg->msg_data->send_file.send_file_type,
                                 SEND_FILE_TYPE_LEN);
    }
    if (msg->status == MCREATED ||
        msg->status == MACKED)
    {
        int i;
        if ((i=fork()) == 0)
        {
/*
 * CHILD
 */
            child_sig_clear();
/*
 * Step one; want to copy the file.
 */
            if (msg->status == MCREATED)
            {
                if (msg->msg_link->cur_host_name != (char *) NULL)
                    ftp_spawn(msg);     /* Should not return */
                exit(1);   /* exec() failed: Should not happen */
            }
            else
            {                    /* MACKED */
                char *argv[5];
                char fname[BUFSIZ];
                struct stat stat_buf;
                (void) sprintf(fname,"%s/%s",e2com_base.work_directory,
                                msg->receive_file_name);
                argv[0] = msg->mtp->prog_to_fire;
                argv[1] = fname;
                argv[2] = (char *) NULL;
                scilog(2,argv);
                if (stat(fname,&stat_buf) < 0)
                    exit(0);    /* Assume file has been processed manually */
                else
                    execvp(argv[0], argv);
                exit(1);   /* exec() failed: Should not happen */
            }
        }
        else
/*
 * PARENT
 */
        if (i<0)
        {
            char * x = "Fork to process msg failed\n";
            perror("Fork to process msg failed");
            scilog(1,&x);
            msg_destroy(msg);
        }
        else
            add_child(i,msg);
    }
    else     /* Must be finished */
        msg_destroy(msg);
    return;
}
void finish() {
    sciabort();
}
/*
 * Run an ftp and return a proper exit status
 *
 * The problem is that the rcp command does not work on QNX; therefore
 * we must use ftp.
 */
void ftp_spawn(msg)
E2MSG * msg;
{
    int try_count = 3;
    int inp[2];
    int ftp_pid;
    char fname[128];
    char buf[128];
    (void) sprintf(fname,"%s/%s",e2com_base.work_directory,
                                msg->receive_file_name);
    close(0);
    close(1);
    while (try_count--)
    {
        (void) unlink(fname);
        if (pipe(inp) == -1)
        {
            char * x ="Cannot open ftp input pipe\n";
            perror("ftp input e2pipe() Failed");
            scilog(1,&x);
            exit(1);      /* Does not return */
        }
        if ((ftp_pid = fork()) > 0)
        {      /* PARENT success */
            static char * x[2] = {"ftp command returned:",""};
#ifdef POSIX
        int
#else
        union wait
#endif
        pid_status;
        int len;
        int i,j;
            if (inp[1] != 1)
            {
                (void) dup2(inp[1],1);
                (void) close(inp[1]);
            }
            if (inp[0] != 1)
                (void) close(inp[0]);
            (void) close(0);
            (void) sigset(SIGPIPE,read_timeout); /* ie catch harmlessly */
            len = sprintf(buf, "user %s %s\n\
binary\n\
get %s %s\n\
quit\n",
                  msg->msg_link->cur_ftp_user_id,
                  msg->msg_link->cur_ftp_pass, 
                  msg->send_file_name,
                  fname);
            x[1] = buf;
            if (e2com_base.debug_level > 0)
                scilog(2,x);
            for (i = 0, j = 0; i < len && j > -1; i += j)
                j = write(1,buf+i,len -i);
            (void) close(1);
            ftp_pid = wait(&pid_status);
            if (e2com_base.debug_level > 2)
            {
                sprintf(buf,"After wait(): errno: %d ftp_pid: %d pid_status %x",
                      errno,ftp_pid,
                      (long)(*((long *)(&pid_status))));
                x[1] = buf;
                scilog(2,x);
            }
            if (WIFEXITED(pid_status))
            {
                if (e2com_base.debug_level > 2)
                {
                    sprintf(buf,"Before exit(): errno: %d w_retcode %d",
                          errno,
#ifdef POSIX
                        pid_status
#else
                        pid_status.w_retcode
#endif
                        );
                    x[1] = buf;
                    scilog(2,x);
                }
#ifdef POSIX
                if (pid_status)
                    continue;
#else
                if (pid_status.w_retcode)
                    continue;
#endif
                else
                {
                    struct stat stat_buf;
                    if (stat(fname,&stat_buf) < 0)
                    {
                        (void) sprintf(buf,
                               "Unable to establish the length of the file %s\n",
                               fname);
                        x[1]= buf;
                        scilog(2,x);
                        continue;
                    }
                    else if (stat_buf.st_size !=
                             atoi(msg->msg_data->send_file.send_file_length))
                    {
                        (void) sprintf(buf,
                                     "Send:%u/Receive:%u Length Mismatch on %s\n",
                             atoi(msg->msg_data->send_file.send_file_length),
                                      stat_buf.st_size, fname);
                        x[1] = buf;
                        scilog(2,x);
                        continue;
                    }
                    else
                    {
                        (void) sprintf(buf,"%s_ftplog",fname);
                        unlink(buf);
                        exit(0);
                    }
                }
            }
            else /* Terminated by signal */
            {
    
                if (e2com_base.debug_level > 0)
                {
                    (void) sprintf(buf,
                        "Killed by a signal: ftp_pid: %d errno: %d pid_status %lx",
                          ftp_pid,errno, (long)(*((long *)(&pid_status))));
                    x[1] = buf;
                    scilog(2,x);
                }
                continue;
            }
        }
        else if (ftp_pid < 0)
        {      /* Parent Failed */
            char * x ="Cannot fork() ftp child\n";
            perror("Cannot fork() ftp child\n");
            scilog(1,&x);
            continue;      /* Does not return */
        }
        else
        {      /* CHILD success */
            int t;
            char * x ="Cannot exec() ftp child\n";
            char *argv[5];
            argv[0] = "ftp";
            argv[1] = "-n";
            argv[2] = msg->msg_link->cur_host_name;
            argv[3] = (char *) NULL;
            scilog(3,argv);
            if (inp[0] != 0)
            {
                (void) dup2(inp[0],0);
                (void) close(inp[0]);
            }
            if (inp[1] != 0)
                (void) close(inp[1]);
            (void) sprintf(buf,"%s_ftplog",fname);
            if ((t = creat(buf,0755)) > -1)
            {
                (void) dup2(t,1);
                (void) dup2(t,2);
            }
            else
            {
                perror("ftp log file open failed");
                x = buf;
                scilog(1,&x);
            }
            (void) close(t);
#ifndef NT4
#ifdef SCO
            setsid();             /* void the terminal association, hopefully */
#else
            if ((t = open("/dev/tty",O_RDWR)) > -1)
            {           /* Stop ftp outputting to the control terminal */
                (void) ioctl(t,TIOCNOTTY,0);
                (void) close(t);
            }
#endif
#endif
            if (execvp(argv[0],argv))
                perror("Command exec failed");
            scilog(1,&x);
            exit(1);
        }
    }
    exit(1);
}
/*
 * Make sure that any forked process won't execute anything that it
 * shouldn't
 */
void child_sig_clear()
{
    LINK * cur_link;
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"chld_sig_clear()\n");
    for (cur_link = link_det;
                cur_link->link_id != 0;
                     cur_link++)
        if (cur_link->connect_fd != -1)
            (void) close(cur_link->connect_fd);
    reset_time();
    (void) close(listen_fd);
    (void) sigset(SIGHUP,SIG_IGN);  
    (void) sigset(SIGINT,SIG_IGN);
    (void) sigset(SIGUSR1,SIG_DFL);
    (void) sigset(SIGQUIT,SIG_IGN);
    (void) sigset(SIGTERM,SIG_DFL);
    (void) sigset(SIGCLD,SIG_DFL);
    (void) sigset(SIGPIPE,io_sig);
#ifdef SIGIO
    (void) sigset(SIGIO,io_sig);
#endif
    return;
}
/***********************************************************************
 * Process non-send_file messages.
 * Search the list of record types to see:
 * - If it is expected
 * - What to do
 * What to do may be:
 * - Call a sub-routine
 * - Store a CONFIRM
 */
void proc_other_msg(msg)
E2MSG * msg;
{
char *x[4];
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"Processing Other Message Link %d Sequence %d\n",
                   msg->msg_link->link_id, msg->seq);
    msg->mtp = mtfind(msg->msg_type);
    x[1] = msg->msg_type;
    if (msg->mtp == (E2COM_MESS_TYPES *) NULL)
    {
/*
 * Must have lost synchronisation somehow.
 */
        x[0] = "Unknown Record Type";
        scilog(2,x);
        link_close(msg->msg_link);
        scicommit();
        if ( msg->msg_link->allowed_to_call == 'Y')
        {
            msg->msg_link->allowed_to_call = 'N';
            add_time(msg->msg_link,e2com_base.retry_time);
        }
        link_open(msg->msg_link);    /* start again */
    }
    else
    if (msg->mtp->sub == 0 &&
        msg->mtp->sub_y_or_n == 'Y')
    {
        x[0] = "No Processing Function defined";
        scilog(2,x);
    }
    if (msg->msg_link->last_in_rec + 1 == msg->seq)
    {
        msg->msg_link->last_in_rec++;
        scilinkupd(msg->msg_link);
    }
    if (msg->mtp == (E2COM_MESS_TYPES *) NULL ||
        msg->mtp->sub == 0 ||
        msg->mtp->sub_y_or_n != 'Y')
    {
        if (msg->mtp != (E2COM_MESS_TYPES *) NULL &&
        msg->mtp->sub_y_or_n != 'Y')
        {           /* A confirm */
            E2MSG * con_msg;
            NEW(E2MSG,con_msg);
            con_msg->link_id = msg->link_id;
            con_msg->seq = atoi(msg->msg_data->
                          confirm.sequence_confirmed);
            con_msg->trans_rec_ind = TRANSMIT;
/*
 * Look for the matching message
 */
            if (readmsg(con_msg))
            {
                msg->their_seq = con_msg->seq;
                con_msg->status = MDONE;
                con_msg->success = msg->msg_data->
                                   confirm.success[0];
                con_msg->their_seq = msg->seq;
                con_msg->mtp = mtfind(con_msg->msg_type);
                changemsg(con_msg);
/*
 * If this was a file transmission, and it has been acknowledged, delete
 * the file
 */
                if (con_msg->mtp != (E2COM_MESS_TYPES *) NULL &&
                    con_msg->mtp->sub_y_or_n == 'N')
                    (void) unlink(con_msg->send_file_name);
/*
 * This only solves part of the problem. For real time transactions,
 * you would want this routed back to the submitter. The message type
 * definition would be extended to indicate whether real-time confirmation
 * is expected; and a hash table of outstanding transactions would be
 * built. No provision is made for this here; it is not needed by the NSHT
 * setup.
 */
            }
            else
            {
                static char * x[8] = { "Match failed for link","",
                              "Confirming",
                              "","Type","",
                              "Success"};
                x[1] = msg->msg_link->dest_host_name;
                x[3] =  msg->msg_data->confirm.sequence_confirmed;
                x[5] =  msg->msg_data->confirm.record_type;
                x[7] =  msg->msg_data->confirm.success;
                scilog(8,x);
            }
            msg_destroy(con_msg);
        }
        msg->status = MDONE;
        changemsg(msg);
    }
    scicommit();
    if (msg->mtp != (E2COM_MESS_TYPES *) NULL &&
        msg->mtp->sub != 0 &&
        msg->mtp->sub_y_or_n == 'Y')
    {            /* A sub-routine is defined */
        int r;
        char * error_message;
        r = (*(msg->mtp->sub))(msg->msg_data,&error_message);
                                      /* Call the sub-routine */
        msg->success = (r)?SUCCESS:FAILURE;
        if (msg->success == FAILURE)
        {
            (void) fprintf(e2com_base.errout,"Transaction Failed Link %d Message %c:%d\n",
                           msg->msg_link->link_id,msg->trans_rec_ind,msg->seq);
            (void) sioutrec(e2com_base.errout,msg->msg_data, (E2_FILE *) NULL);
            if ( error_message != (char *) NULL)
                (void) fprintf(e2com_base.errout,"Error : %s\n", error_message);
        }
        if (msg->error_message != (char *) NULL)
        {
            free(msg->error_message);
            msg->error_message = (char *) NULL;
        }
        if (error_message != (char *) NULL)
            msg->error_message = strdup(error_message);
        if (msg->mtp->confirm_record_type != (char *) NULL &&
            strlen(msg->mtp->confirm_record_type)) 
            ackmsg(msg);
        msg->status = MDONE;
        changemsg(msg);
        scicommit();
    }
    msg_destroy(msg);   /* Finished with it */
    return;
}
/************************************************************************
 * Shut a link; mark it as not a calling link, and queue a shut-down
 * message for it, if the link is up.
 */
void link_shut(link)
LINK * link;
{
    link->allowed_to_call = 'N';
    if (link->link_up_y_or_n == 'Y')
        shutmsg(link);
    scilinkupd(link);
    return;
}
/************************************************************************
 * Close a link; mainly a question of throwing away all the malloc'ed space
 */
void link_close(link)
LINK * link;
{
    E2MSG * msg;
    if (link == (LINK *) NULL)
    {
        char * x = "Logic Error: link_close() called with NULL link";
        scilog(1,&x);
        return;
    }
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout,"link_close(): Closing Link %d\n",link->link_id);
    while ((msg = link_fetch(link)) != (E2MSG *) NULL)
       msg_destroy(msg);
    if (link->connect_fd != -1)
    {
        (void) close(link->connect_fd);
        if (link->read_connect_file != (FILE *) NULL)
            (void) fclose(link->read_connect_file);
        if (link->write_connect_file != (FILE *) NULL)
            (void) fclose(link->write_connect_file);
        link->connect_fd = -1;
    }
    if (link->cur_host_name != (char *) NULL)
    {
        free(link->cur_host_name);
        link->cur_host_name = (char *) NULL;
    }
    if (link->cur_ftp_user_id != (char *) NULL)
    {
        free(link->cur_ftp_user_id);
        link->cur_ftp_user_id = (char *) NULL;
    }
    if (link->cur_ftp_pass != (char *) NULL)
    {
        free(link->cur_ftp_pass);
        link->cur_ftp_pass = (char *) NULL;
    }
    link->link_up_y_or_n = 'N';
    scilinkupd(link);        /* update the link with the new values */
    return;
}
