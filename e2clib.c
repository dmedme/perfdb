/*
 * e2clib.c - Service Routines for e2com.c
 */
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1990\n";

#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#ifdef ULTRIX
#include <fcntl.h>
#else
#ifdef AIX
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#endif
#endif
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
FILE * fdopen();
/* The following are to keep siinrec.h happy */

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
#include "hashlib.h"
#include "e2file.h"
#include "siinrec.h"

extern char * getenv();
/*
 * Data that was originally global. Get rid of this later.
 */
int link_id[5];
int link_rec_offset[5];
char dest_host_name[5][33];
char dest_ftp_user_id[5][33];
char dest_ftp_pass[5][33];
char allowed_to_call[5][2];
int max_simul[5];
int last_out_alloc[5];
int last_out_sent[5];
int last_in_rec[5];
int earliest_in_unread[5];
int earliest_out_unread[5];
char link_up_y_or_n[5][2];
char link_up_y_or_n[5][2];
/*
 * Static data
 * - The array of message types
 */
static E2COM_MESS_TYPES e2com_mess_types[MAXMESSTYPES];
static int mess_type_count;
/*
 * The known array of subroutines. This has to be filled in at link time.
 * Therefore, additions to it must be made by hand.
 *
 * The list is terminated with a NULL entry
 *
 * For standalone Testing, we define an empty routine.
 */
int null_sub(b,x)
union all_records * b;
char **x;
{
    *x = "This text is returned from the null() message processor";
    return 1;
}
static struct sub_link {
   char * record_type;
   int (*sub)();
} sub_link[MAXMESSTYPES] = {
{NULL,0}} ;

static int link_count;    /* The number of links */
static int link_ind;      /* The index into the list */

/*******************************************************************
 *  Log the arguments to the global log file.
 */
void
scilog(argc, argv)
int argc;
char    **argv;
{
        FILE    *fpw;
    char    buf[BUFSIZ];
    char    *cp;
    long    t;
    int i;
    if (e2com_base.debug_level > 3)
        (void) fprintf(e2com_base.errout, "scilog()\n");

    time(&t);
    cp = ctime(&t);
    cp[24] = 0;
    (void) sprintf(buf, "e2com, %s, %d, ", cp,getpid());
    for (i = 0 ; i < argc ; i++) {
        (void) strcat(buf, argv[i]);
        (void) strcat(buf, " ");
    }
    (void) strcat(buf, "\n");
        if ((fpw=openfile(e2com_base.work_directory,ACTLOG,"a")) != (FILE *) NULL)
        {
            (void) fwrite(buf,sizeof(char),strlen(buf), fpw);
            (void) fclose(fpw);
        }
        if (e2com_base.debug_level > 1)
            (void) fwrite(buf,sizeof(char),strlen(buf), e2com_base.errout);
        return;
}

/* Initialise username and e2com_base variables
 *      Log on to ORACLE.
 *  If logon fails, log it and exit the program.
 */
void
sciinit(argc, argv)
int argc;
char    **argv;
{
int c;
char * x;
char * sid;
FILE * f;
E2_FILE * e2f;
union all_records u;
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout, "sciinit()\n");
    u.e2com_base_rec.rec_offset = -1;
    if (argc < 3)
    {
        fprintf(e2com_base.errout,
             "Must provide file names for the configuration and log file\n");
        exit(1);
    }
    if ((e2com_base.e2con = e2init(argv[2],100,100)) == (E2_CON *) NULL)
    {
        fprintf(e2com_base.errout,
             "Failed to initialise the file handlers\n");
        exit(1);
    }
    if ((e2f = e2fopen(argv[1],"r")) == (E2_FILE *) NULL)
    {
        fprintf(e2com_base.errout,
             "Cannot open configuration file %s\n", argv[1]);
        exit(1);
    }
    if (siinrec((FILE *) NULL, &u, e2f) == (char *) NULL
        || strcmp(E2COM_BASE_REC_TYPE,u.e2com_base_rec.record_type))
    {
        fprintf(e2com_base.errout,
             "Format error in configuration file %s\n", argv[1]);
        exit(1);
    }
    (void) e2fclose(e2f);
/*
 * Save the returned values for future use
 */
    e2com_base.work_directory = strdup(u.e2com_base_rec.work_directory);
    e2com_base.home_host = strdup(u.e2com_base_rec.home_host);
    e2com_base.e2com_service = strdup(u.e2com_base_rec.e2com_service);
    e2com_base.e2com_protocol =strdup(u.e2com_base_rec.e2com_protocol);
    e2com_base.retry_time = atoi(u.e2com_base_rec.retry_time);
/*
 * Valid Message Types
 */
    x = filename("%s/%s",e2com_base.work_directory,"e2com_mess"); 
    if ((e2f = e2fopen(x, "r")) == (E2_FILE *) NULL)
    {
        fprintf(e2com_base.errout,
             "Cannot open message type file\n");
        exit(1);
    }
    free(x);
    c = 0;
    for (u.e2com_mess_type_rec.rec_offset = -1;
            siinrec((FILE *) NULL, &u, e2f) != (char *) NULL
            && !strcmp(E2COM_MESS_TYPE_REC_TYPE,u.e2com_mess_type_rec.record_type);
                u.e2com_mess_type_rec.rec_offset = -1)
    {
        e2com_mess_types[c].file_type =
              strdup(u.e2com_mess_type_rec.file_type);
        e2com_mess_types[c].prog_to_fire =
              strdup(u.e2com_mess_type_rec.prog_to_fire);
        e2com_mess_types[c].confirm_record_type =
              strdup(u.e2com_mess_type_rec.confirm_record_type);
        e2com_mess_types[c].sub_y_or_n = 
              u.e2com_mess_type_rec.sub_y_or_n[0];
        if (e2com_mess_types[c].sub_y_or_n == 'Y')
        {
        int i;
            for (i = 0;
                      sub_link[i].record_type != (char *) NULL &&
                      strcmp(sub_link[i].record_type,
                             e2com_mess_types[c].file_type);
                          i++);
            if (sub_link[i].record_type == (char *) NULL)
            {
                (void) fprintf(e2com_base.errout,
"Subroutine required for Message Type %s not found. Add the record_type and\n",
                             e2com_mess_types[c].file_type);
                (void) fprintf(e2com_base.errout,
"subroutine to the sub_link array defined in scilib.pc, recompile and link\n");
                exit(1);
            }
            e2com_mess_types[c].sub = sub_link[i].sub;
        }
        c++;
    }
    mess_type_count = c;
    (void) e2fclose(e2f);
/*
 * Open the link file
 */
    x = filename("%s/%s",e2com_base.work_directory,"e2com_control"); 
    if ((e2com_base.e2fsc = e2fopen(x, "r+")) == (E2_FILE *) NULL)
    {
        fprintf(e2com_base.errout,
             "Cannot open links file\n");
        exit(1);
    }
    free(x);
/*
 * Open the Journal file
 */
    x = filename("%s/%s",e2com_base.work_directory,"e2com_journal"); 
    if ((e2com_base.e2fsj = e2fopen(x, "r+")) == (E2_FILE *) NULL)
    {
        fprintf(e2com_base.errout,
             "Cannot open journal file\n");
        exit(1);
    }
    if ((e2com_base.e2fsju = e2fopen(x, "r+")) == (E2_FILE *) NULL)
    {
        fprintf(e2com_base.errout,
             "Cannot open journal file second time\n");
        exit(1);
    }
    if ((e2com_base.e2fsjr = e2fopen(x, "r+")) == (E2_FILE *) NULL)
    {
        fprintf(e2com_base.errout,
             "Cannot open journal file third time\n");
        exit(1);
    }
    if ((e2com_base.e2fsjt = e2fopen(x, "r+")) == (E2_FILE *) NULL)
    {
        fprintf(e2com_base.errout,
             "Cannot open journal file fourth time\n");
        exit(1);
    }
    free(x);
    scilog(argc,argv);
    if (argc > 3)
        optind = 3;
    return;
}
/*
 * Function to locate a message type, or not as the case may be
 */
E2COM_MESS_TYPES * mtfind(msg_type)
char * msg_type;
{
    int i = mess_type_count;
    E2COM_MESS_TYPES * x = e2com_mess_types;
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout, "mtfind(%s)\n",msg_type);
    for (i = mess_type_count; i--; x++)
        if (!strcmp(x->file_type,msg_type))
            return x;
    return (E2COM_MESS_TYPES *) NULL;
}
/*
 * (Re)-read the Link Definitions
 * These are not set up fully here; rather, they are read in,
 * and then placed in the arrays later. This is for compatibility
 * with the original program; it should reduce the need for editing
 * and debugging.
 */
void scilinkin()
{
char *x;
union all_records u;
    if (e2com_base.debug_level > 2)
        (void) fprintf(e2com_base.errout, "scilinkin()\n");
    link_count = 0;
    (void) e2rewind(e2com_base.e2fsc);
    for (u.e2com_control_rec.rec_offset = -1;
             link_count < MAXLINKS &&
             (siinrec((FILE *) NULL, &u, e2com_base.e2fsc) != (char *) NULL
           &&!strcmp(E2COM_CONTROL_REC_TYPE,u.e2com_control_rec.record_type));
                u.e2com_control_rec.rec_offset = -1)
    {
        link_id[link_count] = atoi(u.e2com_control_rec.link_id);
        strcpy(&dest_host_name[link_count][0],
                   u.e2com_control_rec.dest_host_name);
        strcpy(&dest_ftp_user_id[link_count][0],
                   u.e2com_control_rec.dest_ftp_user_id);
        strcpy(&dest_ftp_pass[link_count][0],
                   u.e2com_control_rec.dest_ftp_pass);
        allowed_to_call[link_count][0] = u.e2com_control_rec.allowed_to_call[0];
        max_simul[link_count] = atoi(u.e2com_control_rec.max_simul);
        last_out_alloc[link_count] =
                    atoi(u.e2com_control_rec.last_out_alloc);
        last_out_sent[link_count] = 
                    atoi(u.e2com_control_rec.last_out_sent);
        last_in_rec[link_count] =
                    atoi(u.e2com_control_rec.last_in_rec);
        earliest_in_unread[link_count] =
                    atoi(u.e2com_control_rec.earliest_in_unread);
        earliest_out_unread[link_count] =
                    atoi(u.e2com_control_rec.earliest_out_unread);
        strcpy(&link_up_y_or_n[link_count][0],
                   u.e2com_control_rec.link_up_y_or_n);
        link_rec_offset[link_count] = u.e2com_control_rec.rec_offset;
        link_count ++;
    }
    link_ind = 0;
    return;
}
/*
 * Skip the data portion of a message in the journal
 *
 * Return the record type skipped.
 */
static char * skipmsg(e2fp,up)
E2_FILE *e2fp;
union all_records * up;
{
/*
 * Skip the second record of the pair
 */
    up->e2com_journal_rec.rec_offset = -1;
    if (siinrec((FILE *) NULL, up, e2fp) == (char *) NULL)
    {
        (void) fprintf(e2com_base.errout, "Unexpected Journal EOF\n");
        return (char *) NULL;
    }
    else
/*
 * If the record is one of a triple, skip the third.
 */
    if (!strcmp(DATA_TYPE,up->e2com_journal_rec.record_type))
    {
        up->e2com_journal_rec.rec_offset = -1;
        if (siinrec((FILE *) NULL, up, e2fp) == (char *) NULL)
        {
            (void) fprintf(e2com_base.errout, "Unexpected Journal EOF\n");
            return (char *) NULL;
        }
    }
    return &(up->e2com_journal_rec.record_type[0]);
}
/*
 * Re-synchronise the link and control information
 * ===============================================
 * This allows the journal to be amended manually to recover from various
 * problems.
 * We aim to establish this link:
 * - The last out sent
 * - The earliest in unread
 * - The last in received
 * For our purposes, EXCHANGE_STATUS and SHUTDOWN messages do not count.
 */
void scilinksyn(link)
LINK * link;
{
union all_records u, u1;
long min_t_c;
long min_r_c_n;
long max_t_sad_min_t_c;
long max_r;
long seq;
long link_id;

    if (link == (LINK *) NULL)
    {
        char * x = "Logic Error: scilinksyn() called with NULL link";
        scilog(1,&x);
        return;
    }
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout, "scilinksyn(%d)\n",link->link_id);
    min_t_c = link->last_out_alloc + 1;
    min_r_c_n = link->last_in_rec;
    max_r = 0;
/*
 * First pass: - pick up as much as possible
 */
    (void) e2rewind(e2com_base.e2fsju);
    for (u.e2com_journal_rec.rec_offset = -1;
             (siinrec((FILE *) NULL, &u, e2com_base.e2fsju) != (char *) NULL
           &&!strcmp(E2COM_JOURNAL_REC_TYPE,u.e2com_journal_rec.record_type));
                u.e2com_journal_rec.rec_offset = -1)
    {
        seq = atoi(u.e2com_journal_rec.seq);
        link_id = atoi(u.e2com_journal_rec.link_id);
        if ( skipmsg(e2com_base.e2fsju,&u1) == (char *) NULL)
            break;
        if (link_id == link->link_id
         && strcmp(u1.exchange_status.record_type, EXCHANGE_STATUS_TYPE)
         && strcmp(u1.shutdown.record_type, SHUTDOWN_TYPE))
        {
            if (u.e2com_journal_rec.status[0] == MCREATED)
            {
                if (u.e2com_journal_rec.trans_rec_ind[0] == TRANSMIT
                 && seq < min_t_c)
                    min_t_c = seq;    /* Earliest untransmitted message */
                else
                if (u.e2com_journal_rec.trans_rec_ind[0] == RECEIVE
                 && seq < min_r_c_n)
                    min_r_c_n = seq;  /* Earliest in unread */
            }
            if (u.e2com_journal_rec.trans_rec_ind[0] == RECEIVE
             && seq > max_r)
                max_r = seq;          /* Last received */
        }
    }
/*
 * Second pass: - pick up the term dependent on the first pass.
 */
    max_t_sad_min_t_c = 0;
    (void) e2rewind(e2com_base.e2fsju);
    for (u.e2com_journal_rec.rec_offset = -1;
             (siinrec((FILE *) NULL, &u, e2com_base.e2fsju) != (char *) NULL
           &&!strcmp(E2COM_JOURNAL_REC_TYPE,u.e2com_journal_rec.record_type));
                u.e2com_journal_rec.rec_offset = -1)
    {
        if ( skipmsg(e2com_base.e2fsju,&u1) == (char *) NULL)
            break;
        seq = atoi(u.e2com_journal_rec.seq);
        link_id = atoi(u.e2com_journal_rec.link_id);
        if (link_id == link->link_id
         && strcmp(u1.exchange_status.record_type, EXCHANGE_STATUS_TYPE)
         && strcmp(u1.shutdown.record_type, SHUTDOWN_TYPE))
        {
            if ((u.e2com_journal_rec.status[0] == MSENT
              || u.e2com_journal_rec.status[0] == MACKED
              || u.e2com_journal_rec.status[0] == MDONE)
             && u.e2com_journal_rec.trans_rec_ind[0] == TRANSMIT
             && seq < min_t_c
             && seq > max_t_sad_min_t_c)
                max_t_sad_min_t_c = seq;
        }
    }
    link->last_out_sent = max_t_sad_min_t_c;
    link->last_in_rec  = max_r;
    link->earliest_in_unread = min_r_c_n;
    scilinkupd(link);
    return;
}
/*
 * Update a link record
 */
void scilinkupd(link)
LINK * link;
{
union all_records u;
    if (link == (LINK *) NULL)
    {
        char * x = "Logic Error: scilinkupd() called with NULL link";
        scilog(1,&x);
        return;
    }
    strcpy(u.e2com_control_rec.record_type, E2COM_CONTROL_REC_TYPE);
    u.e2com_control_rec.rec_offset = link->rec_offset;
    strcpy(u.e2com_control_rec.dest_host_name, link->dest_host_name);
    (void) sprintf(u.e2com_control_rec.link_id, "%d", link->link_id);
    u.e2com_control_rec.allowed_to_call[0] = link->allowed_to_call;
    (void) strcpy(u.e2com_control_rec.dest_ftp_user_id,
            link->dest_ftp_user_id);
    (void) strcpy(u.e2com_control_rec.dest_ftp_pass,link->dest_ftp_pass);
    (void) sprintf(u.e2com_control_rec.max_simul, "%d",  link->max_simul);
    (void) sprintf(u.e2com_control_rec.last_out_alloc,
               "%d", link->last_out_alloc);
    (void) sprintf(u.e2com_control_rec.last_out_sent,
               "%d", link->last_out_sent);
    (void) sprintf(u.e2com_control_rec.last_in_rec,
               "%d", link->last_in_rec);
    (void) sprintf(u.e2com_control_rec.earliest_in_unread,
               "%d", link->earliest_in_unread);
    (void) sprintf(u.e2com_control_rec.earliest_out_unread,
               "%d", link->earliest_out_unread);
    u.e2com_control_rec.link_up_y_or_n[0] = link->link_up_y_or_n;
    if (e2com_base.debug_level > 1)
    {
        (void) fprintf(e2com_base.errout, "scilinkupd()\n");
(void) fprintf(e2com_base.errout,"Link: %d Allowed to Call: %c Max_Simul: %d Up? %c\n",
                   link->link_id, link->allowed_to_call, link->max_simul,
                               link->link_up_y_or_n);
(void) fprintf(e2com_base.errout, "last_out_alloc: %d last_out_sent: %d last_in_rec: %d\n",
                        link->last_out_alloc, link->last_out_sent,
                        link->last_in_rec);
(void) fprintf(e2com_base.errout, "earliest_in_unread: %d earliest_out_unread: %d\n",
                        link->earliest_in_unread, link->earliest_out_unread);
    }
    if (!sioutrec((FILE *) NULL, &u,e2com_base.e2fsc))
    {
        perror("scilinkupd()");
        (void) fprintf(e2com_base.errout,
               "Link Record Update (scilinkupd()) Failed\n");
    }
    return;
}
/*
 * Reset the link and control information, and ready for retransmission.
 */
void scilinkres(link)
LINK * link;
{
union all_records u;
    if (link == (LINK *) NULL)
    {
        char * x = "Logic Error: scilinkres() called with NULL link";
        scilog(1,&x);
        return;
    }
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout, "scilinkres(%d)\n",link->link_id);
    scilinkupd(link);        /* update the link with the new values */

    (void) e2rewind(e2com_base.e2fsjt);
/*
 * Loop through the journal records, stamping those for retransmission
 * as necessary.
 */
    for (u.e2com_journal_rec.rec_offset = -1;
             (siinrec((FILE *) NULL, &u, e2com_base.e2fsjt) != (char *) NULL
           &&!strcmp(E2COM_JOURNAL_REC_TYPE,u.e2com_journal_rec.record_type));
                u.e2com_journal_rec.rec_offset = -1)
    {
    long seq;
    long link_id;

        seq = atoi(u.e2com_journal_rec.seq);
        link_id = atoi(u.e2com_journal_rec.link_id);
/*
 * See if anything can be updated
 */
        if (link_id == link->link_id
         && u.e2com_journal_rec.trans_rec_ind[0] == TRANSMIT
         && seq > link->last_out_sent
         && u.e2com_journal_rec.status[0] != MDONE
         && (u.e2com_journal_rec.status[0] != MCREATED
          ||  (u.e2com_journal_rec.success[0] != ' '
             && u.e2com_journal_rec.success[0] != '\0')))
        {
            u.e2com_journal_rec.status[0] = MCREATED;
            u.e2com_journal_rec.success[0] = ' ';
            if (!sioutrec((FILE *) NULL, &u, e2com_base.e2fsjt))
            {
                perror("sclinkres()");
                (void) fprintf(e2com_base.errout,
                       "Retransmission Message Reset (scilinkres()) Failed\n");
            }
        }
        (void) skipmsg(e2com_base.e2fsjt,&u);
    }
    (void) e2rewind(e2com_base.e2fsjt);
    return;
}
static int fillinmsg(e2fp,up,msg)
E2_FILE * e2fp;
union all_records * up;
E2MSG * msg;
{
    msg->rec_offset = up->e2com_journal_rec.rec_offset;
    msg->seq = atoi(up->e2com_journal_rec.seq);
    msg->link_id = atoi(up->e2com_journal_rec.link_id);
    msg->their_seq = atoi(up->e2com_journal_rec.their_seq);
    msg->status = up->e2com_journal_rec.status[0];
    msg->success = up->e2com_journal_rec.success[0];
    msg->trans_rec_ind =  up->e2com_journal_rec.trans_rec_ind[0] ;
    msg->receive_file_name = strdup(up->e2com_journal_rec.receive_file_name);
    msg->error_message = strdup(up->e2com_journal_rec.error_message);
    msg->timestamp = atoi(up->e2com_journal_rec.timestamp);
    if (skipmsg(e2fp,up) == (char *) NULL)
        return 0;               /* Sequence error in journal */
    if ((msg->msg_data = (union all_records *)malloc(sizeof(union all_records)))
        == (union all_records *) NULL)
    {
        (void) fprintf(e2com_base.errout, "malloc() failed in fillinmsg()\n");
        return 0;
    }
    else
        *msg->msg_data = *up;
    if (!strcmp(up->send_file.record_type,SEND_FILE_TYPE))
    {
        msg->send_file_name = msg->msg_data->send_file.send_file_name;
        msg->msg_type = msg->msg_data->send_file.send_file_type;
    }
    else
    {
        msg->msg_type = msg->msg_data->send_file.record_type;
        msg->send_file_name = (char *) NULL;
    }
    return 1;
}
/*
 * Fetch another message from the journal for transmission
 * It is assumed that the E2MSG has been cleaned up prior to this call,
 * if it is not a fresh one
 */
int getnextmsg(msg)
E2MSG * msg;
{
int ret;
union all_records u;
    if (msg == (E2MSG *) NULL)
    {
        char * x = "Logic Error: getnextmsg() called with NULL msg";
        scilog(1,&x);
        return 0;
    }
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout, "getnextmsg()\n");
    for (;;)
    {
        u.e2com_journal_rec.rec_offset = msg->msg_link->t_offset;
        if (siinrec((FILE *) NULL, &u, e2com_base.e2fsjt) == (char *) NULL)
            return 0;
        else
        if (strcmp(E2COM_JOURNAL_REC_TYPE,u.e2com_journal_rec.record_type))
        {
            (void) fprintf(e2com_base.errout,
             "Sequence Error in getnextmsg (scilinkres()) Expected %s Saw %s\n",
                  E2COM_JOURNAL_REC_TYPE,u.e2com_journal_rec.record_type);
            return 0;
        }
/*
 * See if this record needs to be resent
 */
        if (atoi(u.e2com_journal_rec.link_id) == msg->link_id
         && u.e2com_journal_rec.trans_rec_ind[0] == TRANSMIT
         && u.e2com_journal_rec.status[0] == MCREATED)
            break;
        (void) skipmsg(e2com_base.e2fsjt,&u);
         msg->msg_link->t_offset = e2ftell(e2com_base.e2fsjt);
    }
    ret = fillinmsg(e2com_base.e2fsjt,&u,msg);
    msg->msg_link->t_offset = e2ftell(e2com_base.e2fsjt);
    return ret;
}
/*
 * Attempt to reprocess unsuccessful incoming transactions when the link
 * comes back up.
 */
void scilinksfo(link)
LINK * link;
{
    if (link == (LINK *) NULL)
    {
        char * x = "Logic Error: scilinksfo() called with NULL link";
        scilog(1,&x);
        return;
    }
    link->sf_offset = 0;
    return;
}
/*
 * Get the next Incoming Message to re-process
 */
int getnextsf(msg)
E2MSG * msg;
{
int ret;
union all_records u;
    if (msg == (E2MSG *) NULL)
    {
        char * x = "Logic Error: getnextsf() called with NULL msg";
        scilog(1,&x);
        return 0;
    }
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout, "getnextsf()\n");
    for (;;)
    {
        u.e2com_journal_rec.rec_offset = msg->msg_link->sf_offset;
        if (siinrec((FILE *) NULL, &u, e2com_base.e2fsjr) == (char *) NULL)
            return 0;
        else
        if (strcmp(E2COM_JOURNAL_REC_TYPE,u.e2com_journal_rec.record_type))
        {
            (void) fprintf(e2com_base.errout,
             "Sequence Error in getnextsf (scilinkres()) Expected %s Saw %s\n",
                  E2COM_JOURNAL_REC_TYPE,u.e2com_journal_rec.record_type);
            msg->msg_link->sf_offset = e2ftell(e2com_base.e2fsjr);
            return 0;
        }
/*
 * Return this one if more work needs to be done on this record.
 */
        if (atoi(u.e2com_journal_rec.link_id) == msg->link_id
         && u.e2com_journal_rec.trans_rec_ind[0] == RECEIVE
         && u.e2com_journal_rec.status[0] != MDONE)
            break;
        (void) skipmsg(e2com_base.e2fsjr,&u);
        msg->msg_link->sf_offset = e2ftell(e2com_base.e2fsjr);
    }
    ret =  fillinmsg(e2com_base.e2fsjr,&u,msg);
    msg->msg_link->sf_offset = e2ftell(e2com_base.e2fsjr);
    return ret;
}
/*
 * Read a message from the journal, if it exists.
 */
int readmsg(msg)
E2MSG * msg;
{
union all_records u;
    if (msg == (E2MSG *) NULL)
    {
        char * x = "Logic Error: readmsg() called with NULL msg";
        scilog(1,&x);
        return 0;
    }
/*
 * The search criteria are in the msg structure.
 */
    if (e2com_base.debug_level > 1)
    {
        (void) fprintf(e2com_base.errout, "readmsg()\n");
        (void) fprintf(e2com_base.errout, "Link: %d Sequence: %d T/R: %c\n",
                  msg->link_id,msg->seq,msg->trans_rec_ind);
    }
    (void) e2rewind(e2com_base.e2fsj);
    for (u.e2com_journal_rec.rec_offset = -1;
             (siinrec((FILE *) NULL, &u, e2com_base.e2fsj) != (char *) NULL
           &&!strcmp(E2COM_JOURNAL_REC_TYPE,u.e2com_journal_rec.record_type));
                u.e2com_journal_rec.rec_offset = -1)
    {
    long seq;
    long link_id;

        seq = atoi(u.e2com_journal_rec.seq);
        link_id = atoi(u.e2com_journal_rec.link_id);
/*
 * See if it matches
 */
        if (link_id == msg->link_id
          && u.e2com_journal_rec.trans_rec_ind[0] == msg->trans_rec_ind
          && seq  == msg->seq)
        {
            fillinmsg(e2com_base.e2fsj,&u, msg);
            return 1;
        }
        (void) skipmsg(e2com_base.e2fsj,&u);
    }
    if (!e2feof(e2com_base.e2fsj))
        (void) fprintf(e2com_base.errout,
             "Sequence Error in readmsg() Expected %s Saw %s\n",
                  E2COM_JOURNAL_REC_TYPE,u.e2com_journal_rec.record_type);
    return 0;
}
/*
 *  Return the next Link record
 */
LINK *
getlinkent()
{
    LINK    *lp;
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout, "getlinkent()\n");

    if (link_ind >= link_count)
        return (LINK *) NULL;
    NEW(LINK,lp);
    lp->link_id = link_id[link_ind];
    lp->rec_offset = link_rec_offset[link_ind];
    lp->dest_host_name = strdup(dest_host_name[link_ind]);
    lp->dest_ftp_user_id = strdup(dest_ftp_user_id[link_ind]);
    lp->dest_ftp_pass = strdup(dest_ftp_pass[link_ind]);
    lp->allowed_to_call = allowed_to_call[link_ind][0];
    lp->max_simul = max_simul[link_ind];
    lp->last_out_alloc = last_out_alloc[link_ind];
    lp->last_out_sent = last_out_sent[link_ind];
    lp->last_in_rec = last_in_rec[link_ind];
    lp->earliest_in_unread = earliest_in_unread[link_ind];
    lp->earliest_out_unread = earliest_out_unread[link_ind];
    lp->link_up_y_or_n = link_up_y_or_n[link_ind][0];
    link_ind++;
    return(lp);
}
/*
 *  Copyright (c) E2 Systems, UK, 1990. All rights reserved.
 *
 *  Job-related routines for nat system.
 *
 * msg_destroy - return malloc'ed space
 */
void
msg_destroy(msg)
E2MSG * msg;
{
    if (msg == (E2MSG *) NULL)
    {
        char * x = "Logic Error: msg_destroy() called with NULL msg";
        scilog(1,&x);
        return;
    }
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout, "msg_destroy()\n");
    if (msg->msg_type != (char *) NULL
      && (msg->msg_type < (char *) msg->msg_data || msg->msg_type >=
          (char *)( msg->msg_data + 1)))
        free(msg->msg_type);
    if (msg->send_file_name != (char *) NULL
      && (msg->msg_type < (char *) msg->msg_data || msg->msg_type >=
          (char *)( msg->msg_data + 1)))
        free(msg->send_file_name);
    if (msg->msg_data != (union all_records *) NULL)
        free((char *) msg->msg_data);
    if (msg->receive_file_name != (char *) NULL)
        free(msg->receive_file_name);
    if (msg->error_message != (char *) NULL)
        free(msg->error_message);
    free((char *) msg);
    return;
}
/*
 * getnextseq()
 * - Function gets the outgoing msg id for this link.
 */
int
getnextseq(link)
LINK * link;
{
    if (link == (LINK *) NULL)
    {
        char * x = "Logic Error: getnextseq() called with NULL link";
        scilog(1,&x);
        return -1;
    }
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout, "getnextseq() link: %d\n",link->link_id);
    link->last_out_alloc++;
    scilinkupd(link);        /* update the link with the new values */
    return link->last_out_alloc;
}
/*
 * Routine to transfer from the E2MSG to the Union
 */
static void fillinjournal(msg,up)
E2MSG * msg;
union all_records * up;
{
time_t t = time((time_t *) NULL);
    strcpy(up->e2com_journal_rec.record_type, E2COM_JOURNAL_REC_TYPE); 
    up->e2com_journal_rec.rec_offset = msg->rec_offset;
    (void) sprintf(up->e2com_journal_rec.seq, "%d", msg->seq);
    (void) sprintf(up->e2com_journal_rec.link_id, "%d", msg->link_id);
    (void) sprintf(up->e2com_journal_rec.their_seq, "%d", msg->their_seq);
    up->e2com_journal_rec.status[0] = msg->status;
    up->e2com_journal_rec.success[0] = msg->success;
    up->e2com_journal_rec.trans_rec_ind[0]  = msg->trans_rec_ind;
    if (msg->receive_file_name == (char *) NULL)
        up->e2com_journal_rec.receive_file_name[0] = '\0';
    else
        strcpy(up->e2com_journal_rec.receive_file_name, msg->receive_file_name);
    if (msg->error_message == (char *) NULL)
        up->e2com_journal_rec.error_message[0] = '\0';
    else
        strcpy(up->e2com_journal_rec.error_message, msg->error_message);
    (void) sprintf(up->e2com_journal_rec.timestamp, "%d", (long) t);
    return;
}
/*
 * Add a message to the Journal
 *
 */
int createmsg(msg)
E2MSG * msg;
{
union all_records u;
long t;
    int len;
    if (msg == (E2MSG *) NULL)
    {
        char * x = "Logic Error: createmsg() called with NULL msg";
        scilog(1,&x);
        return 0;
    }
    if (msg->msg_type == (char *) NULL || strlen(msg->msg_type) == 0)
        msg->msg_type = msg->msg_data->data.record_type;
    if (msg->trans_rec_ind == TRANSMIT)
    {                            /* Outgoing */
        msg->seq = getnextseq(msg->msg_link);
        if (!strcmp(msg->msg_type,SHUTDOWN_TYPE) ||
            !strcmp(msg->msg_type,EXCHANGE_STATUS_TYPE))
        (void) sprintf(msg->msg_data->exchange_status.sequence,"%-*d",
                       SEQUENCE_LEN,msg->seq);
    }
    else
    if (msg->seq == 0)
    {
        msg->seq = atoi(msg->msg_data->exchange_status.sequence);
    }
    msg->status = MCREATED;
    fillinjournal(msg,&u);
    if (e2com_base.debug_level > 1)
    {
   (void) fprintf(e2com_base.errout, "createmsg()\n");
   (void) fprintf(e2com_base.errout, "Link: %d Sequence: %d T/R: %c Status: %c\n",
                  msg->link_id,msg->seq,msg->trans_rec_ind,
                  msg->status);
   (void) fprintf(e2com_base.errout, "Record Type: %s Their Seq: %d Message Type: %s\n",
                  msg->msg_data->send_file.record_type,msg->their_seq,
                  msg->msg_type);
    }
    e2fseek(e2com_base.e2fsj,0,SEEK_END);
    u.e2com_journal_rec.rec_offset = -1;
    if (!sioutrec((FILE *) NULL, &u, e2com_base.e2fsj))
    {
        perror("Journal createmsg()");
        return 0;
    }
    msg->rec_offset = u.e2com_journal_rec.rec_offset;
    msg->msg_data->e2com_journal_rec.rec_offset = -1;
    if (!sioutrec((FILE *) NULL, msg->msg_data, e2com_base.e2fsj))
    {
        perror("Data createmsg()");
        return 0;
    }
    else
        return 1;
}
/*
 * Routine for changing msg attributes
 */
int changemsg(msg)
E2MSG * msg;
{
union all_records u;
    if (msg == (E2MSG *) NULL)
    {
        char * x = "Logic Error: changemsg() called with NULL msg";
        scilog(1,&x);
        return 0;
    }
    fillinjournal(msg,&u);
    if (e2com_base.debug_level > 1)
    {
        (void) fprintf(e2com_base.errout, "changemsg()\n");
        (void) fprintf(e2com_base.errout, "Link: %d Sequence: %d T/R: %c Status: %c\n",
                               msg->link_id,msg->seq,msg->trans_rec_ind,
                              msg->status);
       (void) fprintf(e2com_base.errout, "Success: %c Send File: %s Receive File: %s\n",
                  msg->success,(msg->send_file_name == (char *) NULL)?"":
                  msg->send_file_name,(msg->receive_file_name == (char *) NULL)?
                  "":msg->receive_file_name);
    }
    if (!sioutrec((FILE*) NULL, &u, e2com_base.e2fsju))
    {
        (void) fprintf(e2com_base.errout,
                       "Message Update (changemsg()) Failed\n");
        return 0;
    }
    else
        return 1;
}
/*
 * Routine to allocate and fill in a skeleton E2MSG
 */
E2MSG * msg_struct_ini (msg_type,trans_rec_ind)
char * msg_type;
char trans_rec_ind;
{
    E2MSG * input_request;
    NEW(E2MSG,input_request);
    input_request->msg_data = (union all_records *)
            malloc(sizeof( union all_records));
    strcpy(input_request->msg_data->send_file.record_type,msg_type);
    input_request->trans_rec_ind = trans_rec_ind;
    input_request->status = MCREATED;
    input_request->rec_offset = -1;
    return input_request;
}
/***********************************************
 * General Utility Routines
 *
 *  Open the file for dirname, filename, and return the fp.
 *      Lock the file to prevent modification by others.
 *      ONLY ONE FILE CAN BE OPEN AT A TIME WITH THIS SCHEME
 *      WE MANAGE THE BUFFERS OURSELVES, BECAUSE fclose() doesn't free
 *      them!?
 */
static char file_buf[BUFSIZ]; 
FILE *
openfile(d, f, mode)
char    *d;
char    *f;
char    *mode;
{
#ifndef ULTRIX
        static struct f_mode_flags {
          char * modes;
          int flags;
        } f_mode_flags[] =
{{ "r", O_RDONLY },
{"w", O_CREAT | O_APPEND | O_TRUNC | O_WRONLY  },
{"a", O_CREAT | O_WRONLY | O_APPEND  },
{"r+",  O_RDWR  },
{"w+", O_CREAT | O_TRUNC | O_RDWR  },
{"a+", O_CREAT | O_APPEND | O_RDWR  },
{0,0}};
       static struct flock fl = {F_WRLCK,0,0,0,0};
#else
        static struct f_mode_flags {
          char * modes;
          int flags;
        } f_mode_flags[] =
{{ "r", O_RDONLY | O_BLKANDSET },
{"w", O_CREAT | O_APPEND | O_TRUNC | O_WRONLY | O_BLKANDSET },
{"a", O_CREAT | O_WRONLY | O_APPEND | O_BLKANDSET },
{"A", O_CREAT | O_APPEND | O_WRONLY | O_BLKANDSET },
{"r+",  O_RDWR | O_BLKANDSET },
{"w+", O_CREAT | O_TRUNC | O_RDWR | O_BLKANDSET },
{"a+", O_CREAT | O_APPEND | O_RDWR | O_BLKANDSET },
{"A+", O_CREAT | O_APPEND | O_RDWR | O_BLKANDSET },
{0,0}};
#endif
        register struct f_mode_flags * fm;
        static int cmode=0600;
    FILE    *fp;
    char    *name;
        int fd;
        for (fm = f_mode_flags;
                 fm->modes != (char *) NULL &&
                 strcmp(fm->modes,mode);
                     fm++);

        if (fm->modes == (char *) NULL)
        {
            ERROR(__FILE__, __LINE__, 0, "invalid file mode %s", mode, NULL);
            return (FILE *) NULL;
        }
    name = filename("%s/%s", d, f);
        if ((fd = open(name,fm->flags,cmode)) < 0)
        {
            (void) free(name);
            return (FILE *) NULL;
        }
    if ((fp = fdopen(fd, mode)) == (FILE *) NULL) {
        ERROR(__FILE__, __LINE__, 0, "can't fdopen %s", name, NULL);
    } else
        setbuf(fp, file_buf);
        (void) free(name);
    return(fp);
}

/****************************************************************
 * ORACLE Service routines (so that e2com.c does not need
 * ORACLE elements)
 * Manage a list of updated files? May need some indexing.
 */
void scicommit()
{
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout, "scicommit()\n");
    e2commit();
    return;
}
void scirollback()
{
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout, "scirollback()\n");
    e2commit();
    return;
}
void sciabort()
{
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout, "sciabort()\n");
    e2commit();
    exit(1);
}
void scifinish()
{
    if (e2com_base.debug_level > 1)
        (void) fprintf(e2com_base.errout, "scifinish()\n");
    e2commit();
    exit(0);
}
