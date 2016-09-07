/*
 * getsess.c - Create session records from wtmpx/utmpx.
 */
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <utmpx.h>
#include "hashlib.h"
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
 struct utmpx utmpx;
 struct utel * next_utel;
 struct utel * prev_utel;
};
struct utmpx * getutxent();
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
 * Hash the utmpx key fields
 */
int utmp_hh (utp,modulo)
struct utmpx * utp;
int modulo;
{
    return (
           cnthash(utp->ut_user, sizeof(utp->ut_user), modulo) ^
           cnthash(utp->ut_id, sizeof(utp->ut_id), modulo) ^
           cnthash(utp->ut_line, sizeof(utp->ut_line), modulo)) &(modulo - 1);
}
/*
 * Compare a pair of utmp key fields
 */
int ucomp(utp1,  utp2)
struct utmpx * utp1;
struct utmpx * utp2;
{
    int i;
    return
           ((i = strncmp(utp1->ut_user, utp2->ut_user,
               sizeof(utp1->ut_user)))) ? i :
           ((i = strncmp(utp1->ut_id, utp2->ut_id,
               sizeof(utp1->ut_id))) ? i :
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
struct utmpx * ut1;
struct utmpx * ut2;
{
    if (ut1->ut_user[0] != '.'
     && (ut1->ut_type == USER_PROCESS || ut2->ut_type == DEAD_PROCESS)
       )
        printf("%-8.8s %8.1d %9.1u %24.24s %12.12s %4.4s %d %d\n",
           ut1->ut_user,(ut2->ut_tv.tv_sec - ut1->ut_tv.tv_sec),
           ut1->ut_tv.tv_sec,
           ctime( &ut1->ut_tv.tv_sec),
           ut1->ut_line,
           ut1->ut_id,
           ut1->ut_type,ut2->ut_type);
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
 * Process a utmpx element and get rid of it afterwards
 * up AND un MUST MATCH
 */
static void utel_dispose(htp,ap,up,un,ft, close_flag)
HASH_CON * htp;
struct utel ** ap;
struct utmpx * up;
struct utel * un;
time_t ft;
int close_flag;
{
   if (ap == (struct utel **) NULL ||
       *ap == (struct utel *) NULL ||
       htp == (HASH_CON *) NULL ||
       up == (struct utmpx *) NULL ||
       un == (struct utel *) NULL)
        return;
#ifdef DEBUG
    fflush(stdout);
    fprintf(stderr,"name %s type %d\n",un->utmpx.ut_user,un->utmpx.ut_type);
    fflush(stderr);
#endif
    if (up->ut_tv.tv_sec > ft)
    {
        if (un->utmpx.ut_tv.tv_sec < ft && !close_flag)
            un->utmpx.ut_tv.tv_sec = ft;
        do_session(&un->utmpx, up);
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
struct utmpx * up;
struct tm *cur_tm;
char * date_format=(char *) NULL;
                         /* date format expected of data arguments */
struct utmpx tchange;
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
        utmpxname(argv[optind]);
/*
 * Main Processing - For each utmpx record
 */
    while ((up = getutxent()) != (struct utmpx *) NULL)
    {
/*
 * - Look at its type
 */
        if ( up->ut_tv.tv_sec > last_time && !close_flag)
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
 *     saved utmpx records with the time difference, and discard the pair.
 *   - Otherwise, remember it.
 */
            if (tchange.ut_type == 0 || tchange.ut_type == up->ut_type)
                tchange = *up;
            else
            {
                int adj = (tchange.ut_type == NEW_TIME) ?
                           (tchange.ut_tv.tv_sec - up->ut_tv.tv_sec) :
                           (up->ut_tv.tv_sec - tchange.ut_tv.tv_sec) ;
                for (un = anchor;
                         un != (struct utel *) NULL;
                             un=un->next_utel)
                    un->utmpx.ut_tv.tv_sec += adj;
                tchange.ut_type = 0;
            }
            break;
/*
 *  - If it is a LOGIN_PROCESS, USER_PROCESS or DEAD_PROCESS
 */
        case LOGIN_PROCESS:
        case USER_PROCESS:
        case DEAD_PROCESS:
/*
 *   - Look for a match in the hash table.
 *   - If found:
 */
            if ((h = lookup(open_sess, (char *) up)) != (HIPT *) NULL)
            {
                un = (struct utel *) h->body ;
                if (un->utmpx.ut_type != up->ut_type)
                {
/*
 *     - If we have a LOGIN and a USER
 *       - Overwrite the record
 */
                    if ( up->ut_type == USER_PROCESS
                      || up->ut_type == LOGIN_PROCESS)
                        un->utmpx = *up;
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
            }
            else
/*
 *     - If it is a LOGIN or USER
 *       - Add it to the linked list
 *       - Add it to the hash table
 */
            if ( up->ut_type != DEAD_PROCESS)
            {
                un = (struct utel *) malloc(sizeof(struct utel));
                un->utmpx = *up;
                if (anchor != (struct utel *) NULL)
                    anchor->prev_utel = un;
                un->next_utel = anchor;
                un->prev_utel = (struct utel *) NULL;
                anchor = un;
                insert(open_sess,&un->utmpx,un);
            }
            break;
/*
 * If any of these occur, everything must be terminated.
 */
        case RUN_LVL:
            if (up->ut_line[10] != '0' &&
                up->ut_line[10] != '1' &&
                up->ut_line[10] != '6')
                break;
        case BOOT_TIME:
            while ( anchor != (struct utel *) NULL)
            {
#ifdef DEBUG
    fflush(stdout);
    fputs("Boot cleardown\n",stderr);
    fflush(stderr);
#endif
                strncpy(up->ut_user, anchor->utmpx.ut_user,
                                  sizeof(up->ut_user));
                strncpy(up->ut_id, anchor->utmpx.ut_id, sizeof(up->ut_id));
                strncpy(up->ut_line, anchor->utmpx.ut_line,
                                     sizeof(up->ut_line));
                utel_dispose(open_sess,&anchor,up,anchor,first_time,
                                     close_flag);
            }
            break;
        default:
            break;
        }
#ifdef DEBUG
        if ( do_check(anchor))
        {
            fprintf(stderr,"up->ut_name=%s\nup->ut_tv.tv_sec=%u\nup->ut_type=%d\n",
                          up->ut_name, up->ut_tv.tv_sec, up->ut_type);
            abort();
        }
#endif
    }
/*
 * When all done:
 * - Write out session records for all the saved items, using the
 *   last time as the termination time. 
 * - If there is no end time, records with no utmpx entry should
 *   be discarded.
 */
    endutxent();
    tchange.ut_tv.tv_sec = last_time;

    if (utmp_flag)
    {
/*      Loop through the utmp records.
 *      Process them as if they were wtmp log out records with the last time
 *      as their timestamps.
 */
        utmpxname(UTMPX_FILE);
        while ((up = getutxent()) != (struct utmpx *) NULL)
        {
            up->ut_tv.tv_sec = last_time;
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
        strncpy(tchange.ut_user, anchor->utmpx.ut_user, sizeof(up->ut_user));
        strncpy(tchange.ut_id, anchor->utmpx.ut_id, sizeof(up->ut_id));
        strncpy(tchange.ut_line, anchor->utmpx.ut_line, sizeof(up->ut_line));
        utel_dispose(open_sess,&anchor,&tchange,anchor,first_time,
                                     close_flag);
    }
    exit(0);
}
