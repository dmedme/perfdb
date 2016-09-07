/*
 * Program   : e2cap.pc - Performance Base Records
 *
 * Arguments : 1 - ORACLE User ID/Password
 *             2 - Submission FIFO
 *
 * Processing:
 *
 * The program runs as a daemon.
 * 
 * To begin with, remove the communication FIFO.
 *
 * Loop forever:
 * - Attempt to log on to ORACLE
 * - If failed, sleep 1 minute and retry.
 * - Otherwise
 *   - Read in the hierarchy data
 *   - Create the communication FIFO.
 *   - Loop, until an ORACLE error
 *     - Accept a perf database file name on the FIFO.
 *     - Open the file
 *     - Read through it processing the records. For each record:
 *       - Multiply it up based on the number of different possible
 *         hierarchy combinations
 *       - For each combination
 *         - Check to see if it exists
 *         - If so, add it to the update array
 *         - If not, add it to the insert array.
 *
 * @(#) $Name$ $Id$
 * Copyright (c) E2 Systems 1995
 *
*/
static char * sccs_id="@(#) $Name$ $Id$\nCopyright(c) E2 Systems, 1995\n";
#include <sys/types.h>
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
#include <sys/stat.h>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "hashlib.h"
#include "e2file.h"
#include "siinrec.h"
#include "tabdiff.h"
#include "perf.h"
extern double strtod();
extern double floor();
extern char * to_char();

static void status_dump();   /* Function to dump out internal status info */
/*
 * oracle statements (no executable code generated)
 */
struct sess_con * con;
/*
 * Macro defined to enable old SQL*FORMS code to be brought across easily.
 */
/*
 * Program global data
 */
struct prog_glob perf_global;
/*
 * Check the FIFO for something to do
 */
static int do_things()
{
char * so_far;
char * fifo_args[14];                           /* Dummy arguments to process */
char fifo_line[BUFSIZ];                         /* Dummy arguments to process */
int read_cnt;                                   /* Number of bytes read */
register char * x;
short int i;
struct stat stat_buf;
    (void) getcwd(fifo_line,sizeof(fifo_line));
#ifdef SOLAR
    if (fifo_accept(fifo_line,perf_global.listen_fd) < 0)
        return 0;
#else
    if (( perf_global.fifo_fd = fifo_open()) < 0)
        return 0;
                         /*  get back to a state of readiness */
#endif
/*
 * There is
 */
    sighold(SIGCHLD);
    sighold(SIGALRM);
    for (read_cnt = 0,
         x=fifo_line,
         so_far = x;
            (x < (fifo_line + sizeof(fifo_line) - 1)) &&
            ((read_cnt = read (perf_global.fifo_fd,x,
               sizeof(fifo_line) - (x - fifo_line) -1))
                  > 0);
                    x += read_cnt)
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
#endif
    sigrelse(SIGCHLD);
    sigrelse(SIGALRM);
#ifndef SOLAR
    (void) fclose(perf_global.fifo);
#endif
    *x = '\0';                             /* terminate the string */
    (void) unlink(perf_global.lock_name);
/*
 * Process the arguments in the string that has been read
 */
    if ((fifo_args[1]=strtok(fifo_line,"  \n"))==NULL)
         return 1;
/*
 * Generate an argument vector
 */
    for (i=2; (fifo_args[i]=strtok(NULL," \n")) != (char *) NULL; i++);
 
    fifo_args[0] = "";
    opterr=1;                /* turn off getopt() messages */
    optind=1;                /* reset to start of list */
    return proc_args(i,fifo_args);
}
/*
 * Set up submission fifo
 */
static int setup_fifo()
{
    (void) umask(07);         /* allow owner only to submit */
#ifdef SOLAR
    if ((perf_global.listen_fd = fifo_listen(perf_global.fifo_name)) < 0)
    {
        char * x=
    "Failed to open the FIFO listen socket; aborting";
        (void) fprintf(perf_global.errout,"%s: %s\nError: %d\n",perf_global.fifo_name,x,errno);
        perror("Cannot Open FIFO");
        (void) unlink(perf_global.fifo_name);
        return 0;
    }
#else
    if (mknod(perf_global.fifo_name,0010660,0) < 0)
    { /* create the input FIFO */
        char * x ="Failed to create the FIFO; aborting";
        (void) fprintf(perf_global.errout,"%s: %s\nError: %d\n",perf_global.fifo_name,x,errno);
        perror("Cannot Create FIFO");
        return 0;
    }
#endif
    return 1;
}
/*
 * Process arguments on the FIFO
 */
int proc_args(argc,argv)
int argc;
char ** argv;
{
    int c;
    int ret = 1;
    char * input_request;
    union all_records data_envelope;
    char err_buf[BUFSIZ];
    FILE * errout_save;
    if ( perf_global.debug_level > 0)
        dump_args(argc,argv);
    errout_save =  perf_global.errout;
/*
 * Process the arguments
 */
    while ( ( c = getopt ( argc, argv, "hd:f:r:e:" ) ) != EOF )
    {
        switch ( c )
        {
        case 'h' :
            (void) fprintf(stderr,
"e2cap: Performace Guarantee Data Capture Process\n\
You ought to put this program in /etc/inittab\n\
Options:\n\
 -h prints this message on stderr\n\
 -f Submit this file to be sent by a SEND_FILE\n\
 -r time; set new ORACLE retry delay\n\
 -d set the debug level (between 0 and 4)\n");
            break;
        case 'r':
            {
                int r;
                if ((r = atoi(optarg)) != 0)
	            perf_global.retry_time = r;
            }
            break;
        case 'e':
/*
 * Change the disposition of error output
 */
               sighold(SIGALRM);
               sighold(SIGCHLD);
               sigset(SIGPIPE,SIG_IGN);
#ifdef SOLAR
               if ((perf_global.errout = fdopen(perf_global.fifo_fd,"w")) == (FILE *) NULL)
               {
                   perror("Response Connect() failed");
                   perf_global.errout = stderr;
               }
               else
                   setbuf(perf_global.errout,err_buf);
#else
               if ((perf_global.errout = fopen(optarg,"w")) == (FILE *) NULL)
               {
                   perror("Response FIFO open failed");
                   perf_global.errout = stderr;
               }
               else
                   setbuf(perf_global.errout,err_buf);
#endif
               sigrelse(SIGALRM);
               sigrelse(SIGCHLD);
            break;
        case 'd':
            {
                static char * x[2]={"Debug Level Set to %s"};
                perf_global.debug_level = atoi(optarg);
                e2com_base.debug_level = perf_global.debug_level;
                x[1] = optarg;
                fprintf(perf_global.errout,x[0],x[1]);
            }
            break;
        case 'f':
/*
 * The name of the file to process
 */
            {
                struct stat stat_buf;
                input_request = strdup(optarg);
                if (stat(input_request, &stat_buf) < 0)
                {
                    static char * x[2] ={
                              "Unable to establish the length of the file %s\n",
                                 ""};
                    x[1] = input_request;
                    fprintf(perf_global.errout,x[0],x[1]);
                    (void) free( input_request);
                    input_request = (char *) NULL;
                }
                else if (stat_buf.st_size == 0L)
                {
                    static char * x[2] ={
                                 "Cannot process zero length file %s\n",
                                 ""};
                    x[1] = input_request;
                    fprintf(perf_global.errout,x[0],x[1]);
                    (void) free( input_request);
                    input_request = (char *) NULL;
                }
                else
                {
/*
 * Process the file
 */
                    ret = do_one_file( input_request);
                    if (ret)
                    {
                        fprintf(perf_global.errout,"\nOK\n");
                        fflush(perf_global.errout);
                        unlink(input_request);
                    }
                    (void) free( input_request);
                    input_request = (char *) NULL;
                }
            }
            break;
        default:
        case '?' : /* Default - invalid opt.*/
               (void) fprintf(stderr,"Invalid argument; try -h\n");
        break;
        } 
    }
/*
 * Restore the error file pointer
 */
    if (perf_global.errout != errout_save)
        (void) fclose(perf_global.errout);
    perf_global.errout = errout_save;
    return 1;
}
/*
 * Process a data capture file
 * Notice the return status is 1, unless ORACLE has failed.
 */
int do_one_file(in_file)
char * in_file;
{
int file_format;
FILE * fp;
union all_records rec_buffer;
struct aud_control aud;
char buf[BUFSIZ];
/*
 * - Open the file
 */
    if ((fp = fopen(in_file,"r")) == (FILE *) NULL)
    {
        (void) fprintf(stderr,"Failed to open incoming file %s\n",in_file);
        perror("Open Failed");
        return(1);
    }
    setbuf(fp,buf);
/*
 * - Read through it once, to check that the file format is valid
 * - If not, emit an error message
 */
    if ((file_format = scan_file(fp)) == 0)
    {
        (void) fprintf(stderr,"File format error, %s byte position %u\n",
                       in_file,ftell(fp));
        dbms_roll(con);
        fclose(fp);
        return(1);
    }
/*
 * Ready the audit details
 */
    memset((char *) &aud, 0, sizeof(aud));
    aud.audit_id = get_audit_id();
/*
 * - Reset to the beginning
 */
    (void) fseek(fp,0,0);
    if ((perf_global.dt_combs = hash(2000,uhash_key, ucomp_key)) == (HASH_CON *) NULL)
    {
        fprintf(perf_global.errout, "Failed to create the combination hash table\n");
        fclose(fp);
        return 0;
    }
/*
 * - Read through it again, this time filling in the fields in an
 *   array of structures, one order at a time
 */
    memset((char *) &rec_buffer, 0, sizeof(rec_buffer));
    while ((file_format == '{' && do_define(fp,&rec_buffer) == END)
      || (file_format != '{' &&
         !strcmp(siinrec(fp,&rec_buffer, (E2_FILE *) NULL),E2COM_PERF_TYPE)))
    {
        if (!cap_one_record(&rec_buffer.e2com_perf, &aud))
        {
            dbms_roll(con);
            iterate(perf_global.dt_combs,0,free);  /* Get rid of the hash data  */
            cleanup(perf_global.dt_combs);         /* Get rid of the hash table */
            fclose(fp);
            return 0;
        }
        memset((char *) &rec_buffer, 0, sizeof(rec_buffer));
    }
    fclose(fp);
/*
 * Initiate the processing of the accumulator records
 */
    if (!cap_rest(&aud))
    {
        dbms_roll(con);
        iterate(perf_global.dt_combs,0,free);  /* Get rid of the hash data  */
        cleanup(perf_global.dt_combs);         /* Get rid of the hash table */
        return 0;
    }
    dbms_commit(con);
    iterate(perf_global.dt_combs,0,free);  /* Get rid of the hash data  */
    cleanup(perf_global.dt_combs);         /* Get rid of the hash table */
    return 1;
}
/*
 * Receipt of a termination signal
 */
void finish()
{
    (void) unlink(perf_global.fifo_name);
    (void) unlink(perf_global.lock_name);
    if (con != (struct sess_con *) NULL)
    {
        dbms_roll(con);
        dbms_disconnect(con);
    }
    exit(0);
}
/**************************************************************************
 * Main Program Start Here
 * VVVVVVVVVVVVVVVVVVVVVVV
 */
int main(argc,argv)
int     argc;
char    *argv[];
{
char * x;
char buf[128];
char * uid;
char * uid_save;
    perf_global.errout = stderr;
    e2com_base.errout = stderr;     /* For siinrec.c */
    perf_global.retry_time = 30;
    (void) fprintf(stderr,"e2cap.c - Performance Base Capture Daemon\n");
    proc_args(argc,argv);
    if ((argc - optind) < 2)
    {
        fprintf(stderr, "Must provide an ORACLE User and a Fifo name\n");
        exit(1);
    }
    uid_save = strdup(argv[optind]);
    for (x = argv[optind]; *x != '\0'; *x++ = '\0');
                      /* Rub out the userid/password on the command line */
    perf_global.fifo_name = argv[optind + 1];
    (void) unlink(perf_global.fifo_name);
    sprintf(buf,"%s.lck",perf_global.fifo_name);
    perf_global.lock_name = strdup(buf );
    (void) unlink(perf_global.lock_name);
/*
 * Forever; establish an ORACLE connexion, and process files
 */
    sigset(SIGTERM, finish);
    sigset(SIGUSR1, finish);
    load_dict();            /* Load up the bind variable dictionary */
    for (;;)
    {
/*
 * Login to the database.
 */
        uid = strdup(uid_save);
        if ((con = dyn_connect(uid, "e2cap")) == (struct sess_con *) NULL)
        {
            (void) fprintf(stderr,"Database Connect Failed\n");
            free(uid);
            sleep(perf_global.retry_time);
/*
 * We do not appear to be able to re-login to ORACLE successfully
 * We exec another process.
 */
            argv[argc - 2] = uid_save;
            execvp("e2cap",argv);  /* Should not return!             */
            continue;
        }
        free(uid);
/*
 * Process things whilst ORACLE is alive
 */
        if (!setup_fifo())
            exit(1);               /* Exit if cannot create the fifo */
        open_all_sql(con);         /* Parse the SQL                  */
        glr_fetch();               /* Read in the data type details  */
        while(do_things());        /* Until ORACLE finishes          */
        glr_free();                /* Free up the data type details  */
        if (con != (struct sess_con *) NULL)
        {
            dbms_roll(con);
            dyn_disconnect(con);
            con = (struct sess_con *) NULL;
        }
        (void) unlink(perf_global.fifo_name);
        (void) unlink(perf_global.lock_name);
        sleep(perf_global.retry_time);
/*
 * We do not appear to be able to re-login to ORACLE successfully
 * We exec another process.
 */
        argv[argc - 2] = uid_save;
        execvp("e2cap",argv);
    }
    exit(0);
}
