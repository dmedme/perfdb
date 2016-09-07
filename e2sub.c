/*************************************************************
 * e2sub.c - program to submit a command to one of the daemons.
 * As a special case, it knows about submitting things to e2cap.
 */
static char * sccs_id="@(#) $Name$ $Id$\nCopyright (c) E2 Systems 1991";
#include <sys/types.h>
#ifdef VCC2003
#include <fcntl.h>
#else
#ifdef ULTRIX
#include <fcntl.h>
#else
#ifdef AIX
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#endif
#endif
#endif
#include <sys/stat.h>
#ifdef MINGW32
typedef char * caddr_t;
#endif
#ifndef VCC2003
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#else
#include <WinSock2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashlib.h"
#include "e2file.h"
#include "siinrec.h"

extern char * to_char();
extern double strtod();
/*
 *    Usage: e2sub queue ...
 *    Options:
 *    -w (Create the output file)
 *    -n Submission output name (lock file is same plus .lck)
 *    { In pairs
 *    -x Batch Run
 *    -b Filename (for create/last modified details)
 *    }
 *    -v Variable Format file name (default is stdin)
 *    -o OK String
 *    Other options as per e2com.
 */
static char cmd_buf[BUFSIZ];
int main(argc, argv, envp)
int    argc;
char    **argv;
char    **envp;
{
union all_records rec_buf;
int c;
int i;
int ret;
FILE * fp = stdin;
FILE * ofp = (FILE *) NULL;
char * in_name = (char *) NULL;
char * out_name = (char *) NULL;
char * b_name = (char *) NULL;
char * x_name = (char *) NULL;
char * fifo_name = (char *) NULL;
char * m_name = (char *) NULL;   /* Which Machine */
char * check_string = "\nOK\n";   /* Default OK String */

    e2com_base.errout = stderr;
    e2com_base.debug_level = 0;
    ret = 0;
    while ((c=getopt(argc, argv, "j:as:hd:ik:f:t:l:r:c:wn:o:v:b:x:1e:Q"))
                    != EOF )
    {
        switch ( c )
        {
/*
 *    -w (Create the output file)
 */
        case 'w':
            if (out_name != (char *) NULL)
                ofp = fopen(out_name,"w");
            else
                fprintf(e2com_base.errout,
                       "The -f filename option must appear before the -w\n");
            break;
/*
 *   -n Submission output name (lock file is same plus .lck)
 */
        case 'n':
            fifo_name = optarg;
            break;
/*
 *    In pairs
 *    -x Batch Run
 *    -b Filename (for create/last modified details)
 */
        case 'm':
        case 'x':
        case 'b':
            if (b_name == (char *) NULL && x_name == (char *) NULL)
                memset((char *) &rec_buf, (int) ' ', sizeof(rec_buf));
            if (c == 'x')
            {
                STRCPY(rec_buf.e2com_perf.rec_instance,optarg);
                x_name = optarg; 
            }
            else
            if (c == 'm')
                m_name = optarg;
            else /* b */
                b_name = optarg; 
            if (x_name != (char *) NULL
             && b_name != (char *) NULL
             && m_name != (char *) NULL
             && ofp != (FILE *) NULL)
            {
            struct stat stat_buf;
                if (stat(b_name, &stat_buf) > -1)
                {
                    double secs_since = (double) stat_buf.st_ctime;
                    STRCPY(rec_buf.e2com_perf.start_time,
                        to_char("DD-Mon-YYYY HH24:MI:SS", secs_since));
                    sprintf(rec_buf.e2com_perf.el,"%d",
                        stat_buf.st_mtime - stat_buf.st_ctime);
                    STRCPY(rec_buf.e2com_perf.rec_type, "BATCH");
                    STRCPY(rec_buf.e2com_perf.host_name, m_name);
                    sioutrec(ofp, &rec_buf, (E2_FILE *) NULL);
                    b_name = (char *) NULL;
                    x_name = (char *) NULL;
                }
            }
            break;
/*
 *    -v Variable Format file name (default is stdin)
 */
        case 'v':
            fp = fopen(optarg,"r");
            break;
/*
 *    -o OK String
 */
        case 'o':
            check_string = optarg;
            break;
        case 'a' :
            strcat(cmd_buf, " -a ");
            break;
        case '1' :
            strcat(cmd_buf, "-1");
            break;
        case 'h' :
            (void) fprintf(e2com_base.errout,
"e2sub: Performance Database Control\n\
Options:\n\
 -h prints this message on e2com_base.errout\n\
Directions to e2sub are\n\
 -x Batch Execution Name\n\
 -e Submit an index request (-x to badsort)\n\
 -l (with a NULL string argument) submit a search request (to badsort)\n\
 -b Batch Log File Name\n\
 -w (Create the file to submit)\n\
 -v Variable format file to process (default is to read stdin)\n\
 -o Format of OK string (default should be correct)\n\
 -n Name of submission fifo (lock file is same .lck)\n\
 -Q Tell the process to quit\n\
Directions to e2com are\n\
 -a Activate\n\
 -c link_id; attempt to connect on a link\n\
 -s link_id; shut a link; 0 shuts all links, and terminates\n\
 -l Submit a SEND_FILE message for this link (default 2) (or search for all)\n\
 -f Submit this file to be sent by a SEND_FILE\n\
 -t Submit a SEND_FILE message of this type\n\
 -r time; set new link retry delay\n\
 -d set the debug level (between 0 and 4)\n\
 -i dump out the link status information on e2com_base.\n\
 -j search for the contents of a given file.\n\
 -k abort the specified SEND_FILE message (or search incrementally)\n");
            break;
        case 'r':
            strcat(cmd_buf, " -r ");
            strcat(cmd_buf, optarg);
            break;
        case 'c':
            strcat(cmd_buf, " -c ");
            strcat(cmd_buf, optarg);
            break;
        case 's':
            strcat(cmd_buf, " -s ");
            strcat(cmd_buf, optarg);
            break;
        case 'j':
            strcat(cmd_buf, " -j ");
            strcat(cmd_buf, optarg);
            break;
        case 'k':
            strcat(cmd_buf, " -k ");
            strcat(cmd_buf, optarg);
            break;
        case 'i':
            strcat(cmd_buf, " -i ");
            break;
        case 'd':
            strcat(cmd_buf, " -d ");
            strcat(cmd_buf, optarg);
            break;
        case 'Q':
            strcat(cmd_buf, " -Q ");
            break;
        case 'e':
            strcat(cmd_buf, " -x ");
            strcat(cmd_buf, optarg);
            break;
        case 'l':
            strcat(cmd_buf, " -l ");
            strcat(cmd_buf, optarg);
            break;
        case 'f':
            strcat(cmd_buf, " -f ");
            strcat(cmd_buf, optarg);
            out_name = optarg;
            break;
        case 't':
            strcat(cmd_buf, " -t ");
            strcat(cmd_buf, optarg);
            break;
        default:
        case '?' : /* Default - invalid opt.*/
               (void) fprintf(e2com_base.errout, "Invalid argument; try -h\n");
        break;
        } 
    }
/*
 * Special case; handle the feed to e2cap. Take the details out of
 * the environment
 */
    if (argc == 2 && cmd_buf[0] == '\0')
    {
        if ((fifo_name = getenv("E2CAP_FIFO")) == (char *) NULL)
            exit(1);
        sprintf(cmd_buf, "-f %s", argv[1]);
    }
/*
 * If there is a possibility of doing some translation for an output file,
 * do it.
 */
    if (ofp != (FILE *) NULL && fp != (FILE *) NULL)
        (void) nvtoff_tran(fp,ofp);
/*
 * If the user asked for a submission
 */
    if (fifo_name != (char *) NULL && cmd_buf[0] != '\0')
    {
        if (!wakeup_e2com(fifo_name, cmd_buf, check_string ))
             exit(1);
    }
    exit(0);
}
finish()
{
    exit(0);
}
