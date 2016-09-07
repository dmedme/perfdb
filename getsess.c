/*
 * getsess.c - Create session records from utmp.
 */
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#ifdef MINGW32
static FILE * utfp;
#define UTMP_FILE "utmp"
static char * ufname = UTMP_FILE;
int utmpname(p)
char *p;
{
    ufname = p;
    return 1;
}
int endutent()
{
    if (utfp != (FILE *) NULL)
    {
        fclose(utfp);
        utfp = (FILE *) NULL;
    }
}
typedef unsigned int pid_t;
struct exit_status
{
  short int e_termination;      /* Process termination status.  */
  short int e_exit;             /* Process exit status.  */
};
/*
 * Use a text rendition of the file from wtmpfix and fwtmp.
 */
#define UT_UNKNOWN      0       /* for ut_type field */
#define RUN_LVL         1
#define BOOT_TIME       2
#define NEW_TIME        3
#define OLD_TIME        4

#define INIT_PROCESS    5
#define LOGIN_PROCESS   6
#define USER_PROCESS    7
#define DEAD_PROCESS    8
#define ACCOUNTING      9
#define UT_LINESIZE     32
#define UT_NAMESIZE     32
#define UT_HOSTSIZE     256
struct utmp
{
  short int ut_type;            /* Type of login.  */
  pid_t ut_pid;                 /* Pid of login process.  */
  char ut_line[UT_LINESIZE];    /* NUL-terminated devicename of tty.  */
  char ut_id[14];               /* Inittab id. - 4 rather than 14? - NUL?  */
  char ut_user[UT_NAMESIZE];    /* Username (not NUL terminated).  */
#define ut_name ut_user         /* Compatible field name for same.  */
  char ut_host[UT_HOSTSIZE];    /* Hostname for remote login.  */
  struct exit_status ut_exit;   /* The exit status of a process marked
                                   as DEAD_PROCESS.  */
  long ut_time;         /* Time entry was made.  */
};
#ifdef DO_OSF1
/*
 * Sample record
 *
atsuser                          ttys6          ttys6                            15967 07 0000 0000 1041503535 wdlf936.channel4.local                                           Thu Jan  2 10:32:15 2003 GMT 
 */
struct utmp * getutent()
{
static struct utmp utmp;
char buf[256];
int i;

    if (utfp == (FILE *) NULL
     && (utfp = fopen(ufname,"rb")) == (FILE *) NULL)
        return (struct utmp *) NULL;
    for (;;)
    {
        if (fgets(buf,sizeof(buf), utfp) == (char *) NULL)
            return (struct utmp *) NULL;
        if ((i = sscanf(buf, "%32c %13c %32c %d %hd %hd %hd %d %64c",
                utmp.ut_user,
                utmp.ut_id,
                utmp.ut_line,
                &(utmp.ut_pid),
                &(utmp.ut_type),
                &(utmp.ut_exit.e_termination),
                &(utmp.ut_exit.e_exit),
                &(utmp.ut_time),
                utmp.ut_host)) == 9)
        {
#ifdef DEBUG
            fprintf(stderr,"Parsed: %d => %.32s %.14s %.32s %d %d %d %d %d %.64s\n%s",
                     i, 
                utmp.ut_user,
                utmp.ut_id,
                utmp.ut_line,
                utmp.ut_pid,
                utmp.ut_type,
                utmp.ut_exit.e_termination,
                utmp.ut_exit.e_exit,
                utmp.ut_time, utmp.ut_host, buf);
            fflush(stderr);
#endif
            return &utmp;
        }
    }
}
#else
struct utmp * getutent()
{
static struct utmp utmp;
char buf[132];
int i;
/*
 * Sample record
 *
mal      tn20 pts/1            24152  8 0000 0000 1012551209 Fri Feb  1 08:13:29 2002
 */

    if (utfp == (FILE *) NULL
     && (utfp = fopen(ufname,"rb")) == (FILE *) NULL)
        return (struct utmp *) NULL;
    for (;;)
    {
        if (fgets(buf,sizeof(buf), utfp) == (char *) NULL)
            return (struct utmp *) NULL;
        if ((i = sscanf(buf, "%8c %4c %s %d %hd %hd %hd %d",
                utmp.ut_user,
                utmp.ut_id,
                utmp.ut_line,
                &(utmp.ut_pid),
                &(utmp.ut_type),
                &(utmp.ut_exit.e_termination),
                &(utmp.ut_exit.e_exit),
                &(utmp.ut_time))) == 8)
            return &utmp;
#ifdef DEBUG
        else
            fprintf(stderr,"Parsed: %d => %.8s %.4s %s %d %d %d %d %d\n",
                     i, 
                utmp.ut_user,
                utmp.ut_id,
                utmp.ut_line,
                utmp.ut_pid,
                utmp.ut_type,
                utmp.ut_exit.e_termination,
                utmp.ut_exit.e_exit,
                utmp.ut_time);
#endif
    }
}
#endif
#else
#include <utmp.h>
#endif
#include "hashlib.h"
#ifdef ULTRIX
#define ut_type ut_line[0]
#define	BOOT_TIME	'~'
#define	OLD_TIME	'|'
#define	NEW_TIME	'}'
#endif
/***********************************************************************
 *
 * Getopt support
 */
extern int optind;           /* Current Argument counter.      */
extern char *optarg;         /* Current Argument pointer.      */
extern int opterr;           /* getopt() err print flag.       */
extern int errno;

#define MAX_SESS 4096
struct utel {
 struct utmp utmp;
 struct utel * next_utel;
 struct utel * prev_utel;
};
struct utmp * getutent();
/*
 * Hash function for counted strings
 */
static unsigned cnthash(w, len, modulo)
char *w;
int modulo;
{
char *p;
unsigned int x,y;
    x = strlen(w);
    if (x > len)
        x = len;
    for (p = w + x - 1, y=0;  p >= w;  x--, p--)
    {
        y += x;
        y *= (*p);
    }
    x = y & (modulo-1);
    return(x);
}
/*
 * Hash the utmp key fields
 */
int utmp_hh (utp,modulo)
struct utmp * utp;
int modulo;
{
    return (
#ifndef ULTRIX
#ifndef DO_OSF1
           cnthash(utp->ut_user, sizeof(utp->ut_user), modulo) ^
           cnthash(utp->ut_id, sizeof(utp->ut_id), modulo) ^
#endif
#endif
           cnthash(utp->ut_line, sizeof(utp->ut_line), modulo)) &(modulo - 1);
}
/*
 * Compare a pair of utmp key fields
 */
int ucomp(utp1,  utp2)
struct utmp * utp1;
struct utmp * utp2;
{
    int i;
    return
#ifdef ULTRIX
           (
#else
#ifdef DO_OSF1
           (
#else
           ((i = strncmp(utp1->ut_user, utp2->ut_user,
               sizeof(utp1->ut_user)))) ? i :
           ((i = strncmp(utp1->ut_id, utp2->ut_id,
               sizeof(utp1->ut_id))) ? i :
#endif
#endif
           (strncmp(utp1->ut_line, utp2->ut_line,
               sizeof(utp1->ut_line))));
}
/*
 * Write out an acctcon1-style session record.
 *  - User name
 *  - Connect time
 *  - Start time in seconds since 1970
 *  - Formatted data, as per ctime
 */
void do_session(ut1,ut2)
struct utmp* ut1;
struct utmp* ut2;
{
#ifndef DEBUG
    if (ut1->ut_user[0] != '.'
#ifndef ULTRIX
     && (ut1->ut_type == USER_PROCESS || ut2->ut_type == DEAD_PROCESS)
#endif
       )
#endif
#ifdef ULTRIX
        printf("%-8.8s %8.1d %9.1u %24.24s %12.12s\n",
           ut1->ut_user,(ut2->ut_time - ut1->ut_time),
           ut1->ut_time,
           ctime( &ut1->ut_time),
           ut1->ut_line);
#else
#ifdef DO_OSF1
        printf("%-8.8s %8.1d %9.1u %24.24s %12.12s %4.4s %d %d %s\n",
           ut1->ut_user,(ut2->ut_time - ut1->ut_time),
           ut1->ut_time,
           ctime( &ut1->ut_time),
           ut1->ut_line,
           ut1->ut_id,
           ut1->ut_type,ut2->ut_type, ut1->ut_host);
#else
        printf("%-8.8s %8.1d %9.1u %24.24s %12.12s %4.4s %d %d\n",
           ut1->ut_user,(ut2->ut_time - ut1->ut_time),
           ut1->ut_time,
           ctime( &ut1->ut_time),
           ut1->ut_line,
           ut1->ut_id,
           ut1->ut_type,ut2->ut_type);
#endif
#endif
    return;
}
#ifdef DEBUG
static int do_check(anchor)
struct utel * anchor;
{
struct utel * un;

    if (anchor == (struct utel *) NULL)
        return 0;
    if (anchor->prev_utel != (struct utel *) NULL)
    {
        (void) fprintf(stderr,"anchor has a back pointer:%x, %x\n",
                      (unsigned long) anchor,
                      (unsigned long)(anchor->prev_utel));
        return 1;
    }
    for (un = anchor; un != (struct utel *) NULL; un = un->next_utel)
        if (un->next_utel != (struct utel *) NULL 
          && un->next_utel->prev_utel != un)
    {
        (void) fprintf(stderr,"back pointer mis-match:%x, %x\n",
                      (unsigned long) un->next_utel,
                      (unsigned long)(un->next_utel->prev_utel));
        return 1;
    }
    return 0;
}
#endif
/*
 * Process a utmp element and get rid of it afterwards
 * up AND un MUST MATCH
 */
static void utel_dispose(htp,ap,up,un,ft, close_flag)
HASH_CON * htp;
struct utel ** ap;
struct utmp * up;
struct utel * un;
time_t ft;
int close_flag;
{
   if (ap == (struct utel **) NULL ||
       *ap == (struct utel *) NULL ||
       htp == (HASH_CON *) NULL ||
       up == (struct utmp *) NULL ||
       un == (struct utel *) NULL)
        return;
#ifdef DEBUG
    fflush(stdout);
    fprintf(stderr,"name %s type %d\n",un->utmp.ut_user,un->utmp.ut_type);
    fflush(stderr);
#endif
    if (up->ut_time > ft)
    {
        if (un->utmp.ut_time < ft && !close_flag)
            un->utmp.ut_time = ft;
        do_session(&un->utmp, up);
    }
    if (un->prev_utel ==
            (struct utel *) NULL)
    {
#ifdef DEBUG
         if (*ap != un)
         {
             (void) fprintf(stderr,
                      "%x has null back pointer but is not anchored %x!\n",
                       (unsigned long) un, (unsigned long) (*ap));
             abort();
         }
#endif
        (*ap) = un->next_utel;
    }
    else
        un->prev_utel->next_utel =
           un->next_utel;
    if (un->next_utel !=
            (struct utel *) NULL)
        un->next_utel->prev_utel =
           un->prev_utel;
    hremove(htp,(char *) up);
    free( (char *) un );
    return;
}
/*************************************************************************
 * Main program starts here.
 */
int main (argc,argv)
int argc;
char ** argv;
{
int close_flag = 0;
int utmp_flag = 0;
int c;
char * x;
struct utel * anchor;
struct utel * un;
HASH_CON * open_sess;
HIPT *h;
struct utmp * up;
struct tm *cur_tm;
char * date_format=(char *) NULL;
                         /* date format expected of data arguments */
struct utmp tchange;
time_t first_time;
time_t last_time;
double valid_time;       /* needed for the date check */
    first_time = (time_t) 0;
    last_time = time(0);
    cur_tm = gmtime(&last_time);
    if (cur_tm->tm_isdst)
        last_time += 3600;
    while ( ( c = getopt ( argc, argv, "hd:s:e:uc" ) ) != EOF )
    {
        switch ( c )
        {
        case 'd':
             date_format = optarg;
             break;
        case 'u' :
            utmp_flag = 1;
            break;
        case 'c' :
            close_flag = 1;
            break;
        case 'e' :
        case 's' :
            if ( date_format != (char *) NULL)
            {
                if ( !date_val(optarg,date_format,&x,&valid_time))
    /*
     * Time argument is not a valid date
     */
                {
                   (void) fprintf(stderr,
                             "%s is not a valid date of format %s\n",
                              optarg, date_format);
                   exit(0);
                }
                if (c == 's')
                    first_time = (time_t)
                        (((long) valid_time) - ((cur_tm->tm_isdst)?3600:0));
                else
                    last_time = (time_t)
                        (((long) valid_time) - ((cur_tm->tm_isdst)?3600:0));
            }
            else
            {
                if (c == 's')
                    first_time = (time_t) atoi(optarg);
                else
                    last_time = (time_t) atoi(optarg);
            }
            break;
        case 'h' :
        default:
        case '?' : /* Default - invalid opt.*/
               (void) fprintf(stderr,
               "Arguments: -d format -s start -e end -u -c wtmpfile\n");
               exit(0);
            break;
       }
    }
    anchor = (struct utel *) NULL;
    open_sess = hash(MAX_SESS,utmp_hh,ucomp);
    tchange.ut_type = 0;   /* Flag it as unused */
    if (argc > optind)
        utmpname(argv[optind]);
/*
 * Main Processing - For each utmp record
 */
    while ((up = getutent()) != (struct utmp *) NULL)
    {
/*
 * - Look at its type
 */
        if ( up->ut_time > last_time && !close_flag)
            break; 
        switch(up->ut_type)
        {
/*
 * - If it is a NEW_TIME or OLD_TIME
 */
        case NEW_TIME:
        case OLD_TIME:
/*
 *   - If we have its mate, compute the difference, update all the
 *     saved utmp records with the time difference, and discard the pair.
 *   - Otherwise, remember it.
 */
            if (tchange.ut_type == 0 || tchange.ut_type == up->ut_type)
                tchange = *up;
            else
            {
                int adj = (tchange.ut_type == NEW_TIME) ?
                           (tchange.ut_time - up->ut_time) :
                           (up->ut_time - tchange.ut_time) ;
                for (un = anchor;
                         un != (struct utel *) NULL;
                             un=un->next_utel)
                    un->utmp.ut_time += adj;
                tchange.ut_type = 0;
            }
            break;
/*
 *  - If it is a LOGIN_PROCESS, USER_PROCESS or DEAD_PROCESS
 */
#ifdef ULTRIX
        default:
#else
        case LOGIN_PROCESS:
        case USER_PROCESS:
        case DEAD_PROCESS:
#endif
/*
 *   - Look for a match in the hash table.
 *   - If found:
 */
            if ((h = lookup(open_sess, (char *) up)) != (HIPT *) NULL)
            {
                un = (struct utel *) h->body ;
#ifdef ULTRIX
                utel_dispose(open_sess,&anchor,up,un,first_time,
                                     close_flag);
#else
                if (un->utmp.ut_type != up->ut_type)
                {
/*
 *     - If we have a LOGIN and a USER
 *       - Overwrite the record
 */
                    if ( up->ut_type == USER_PROCESS
                      || up->ut_type == LOGIN_PROCESS)
                        un->utmp = *up;
                                   /* Replace login with user (don't want
                                      to count getty's) */
                    else
/*
 * Output a session record, and get rid of the entry.
 */
                    if ( up->ut_type == DEAD_PROCESS)
/*
 *     - If we have a LOGIN/USER DEAD pair
 *       - Write out a session record
 *       - Delete the item from the hash table 
 *       - Unlink the item from the list
 *       - free() it
 */
                        utel_dispose(open_sess,&anchor,up,un,first_time,
                                     close_flag);
                }
#endif
            }
            else
/*
 *     - If it is a LOGIN or USER
 *       - Add it to the linked list
 *       - Add it to the hash table
 */
#ifdef ULTRIX
            if ( up->ut_user[0] != '\0')
#else
            if ( up->ut_type != DEAD_PROCESS)
#endif
            {
                un = (struct utel *) malloc(sizeof(struct utel));
                un->utmp = *up;
                if (anchor != (struct utel *) NULL)
                    anchor->prev_utel = un;
                un->next_utel = anchor;
                un->prev_utel = (struct utel *) NULL;
                anchor = un;
                insert(open_sess,&un->utmp,un);
            }
            break;
/*
 * If any of these occur, everything must be terminated.
 */
#ifndef ULTRIX
        case RUN_LVL:
            if (up->ut_line[10] != '0' &&
                up->ut_line[10] != '1' &&
                up->ut_line[10] != '6')
                break;
#endif
        case BOOT_TIME:
            while ( anchor != (struct utel *) NULL)
            {
#ifdef DEBUG
    fflush(stdout);
    fputs("Boot cleardown\n",stderr);
    fflush(stderr);
#endif
#ifndef ULTRIX
                strncpy(up->ut_user, anchor->utmp.ut_user, sizeof(up->ut_user));
                strncpy(up->ut_id, anchor->utmp.ut_id, sizeof(up->ut_id));
#endif
                strncpy(up->ut_line, anchor->utmp.ut_line, sizeof(up->ut_line));
                utel_dispose(open_sess,&anchor,up,anchor,first_time,
                                     close_flag);
            }
            break;
#ifndef ULTRIX
        default:
            break;
#endif
        }
#ifdef DEBUG
        if ( do_check(anchor))
        {
            fprintf(stderr,"up->ut_name=%s\nup->ut_time=%u\nup->ut_type=%d\n",
                          up->ut_name, up->ut_time, up->ut_type);
            abort();
        }
#endif
    }
/*
 * When all done:
 * - Write out session records for all the saved items, using the
 *   last time as the termination time. 
 * - If there is no end time, records with no utmp entry should
 *   be discarded.
 */
    endutent();
    tchange.ut_time = last_time;

    if (utmp_flag)
    {
/*      Loop through the utmp records.
 *      Process them as if they were wtmp log out records with the last time
 *      as their timestamps.
 */
        utmpname(UTMP_FILE);
        while ((up = getutent()) != (struct utmp *) NULL)
        {
            up->ut_time = last_time;
            if ((h = lookup(open_sess, (char *) up)) != (HIPT *) NULL)
            {
                un = (struct utel *) h->body ;
/*
 * Output a session record, and get rid of the entry.
 */
                utel_dispose(open_sess,&anchor,up,un,first_time,
                                     close_flag);
           }
       }
    }
    if (!close_flag)
    while ( anchor != (struct utel *) NULL)
    {
#ifndef ULTRIX
        strncpy(tchange.ut_user, anchor->utmp.ut_user, sizeof(up->ut_user));
        strncpy(tchange.ut_id, anchor->utmp.ut_id, sizeof(up->ut_id));
#endif
        strncpy(tchange.ut_line, anchor->utmp.ut_line, sizeof(up->ut_line));
        utel_dispose(open_sess,&anchor,&tchange,anchor,first_time,
                                     close_flag);
    }
    exit(0);
}
