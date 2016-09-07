/*
 * SQL capture logic for badsort for a Sybase database.
 *
 * Initialise:
 * - Log on to sybase using given id
 *
 * Thereafter, accept calls from the SQL capture routine.
 *     - Receive the SQL statements
 *     - See if we already have them.
 *     - If not
 *       - We need to get a plan for them.
 *       - We store the SQL
 *     - We store the usage
 *       - Add it to the current bucket and the total usage for this statement
 *     - See if 20 minutes, or whatever, have elapsed
 *     - If so
 *       - Write a checkpoint
 *       - Reset the current usage buckets.
 *
 * @(#) $Name$ $Id$
 * Copyright (c) E2 Systems 1995
 ***********************************************************************
 *This file is a Trade Secret of E2 Systems. It may not be executed,
 *copied or distributed, except under the terms of an E2 Systems
 *UNIX Instrumentation for CFACS licence. Presence of this file on your
 *system cannot be construed as evidence that you have such a licence.
 ***********************************************************************
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright(c) E2 Systems,1995,1998\n";
#include <sys/types.h>
#include <time.h>
#ifndef LCC
#ifndef VCC2003
#include <sys/time.h>
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
#endif
#include <sys/stat.h>
#ifdef MINGW32
typedef char * caddr_t;
#endif
#ifndef MINGW32
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#else
#include <WinSock2.h>
#endif
#endif
#else
#include <windows.h>
#include <winsock2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include "IRfiles.h"
#include "hashlib.h"
#include "tabdiff.h"
#include "e2file.h"
#include "siinrec.h"
#include "badsort.h"
extern double strtod();
extern double floor();
extern char * to_char();
#ifndef LINUX
extern char * strdup();
#endif
extern char * strnsave();
jmp_buf ora_fail;            /* Used to recover from Sybase errors        */
static void do_oracle_plan();
static void do_oracle_usage();
static char * get_collector();
static void find_the_plan();
/*
 * Program global data, shared with natsock.
 */
struct prog_glob perf_global;
/************************************************************************
 * Dummies; The performance database doesn't use this functionality
 */
int scram_cnt = 0;                /* Count of to-be-scrambled strings */
char * scram_cand[1];             /* Candidate scramble patterns */
char * tbuf;
char * tlook;
int tlen;
enum tok_id look_tok;
enum look_status look_status;
/**************************************************************************
 * Sybase Elements
 */
static struct sess_con * con;
static struct sess_con * con_explain;
static struct sess_con * con_stats;
/*
 * SQL statement (fragments)
 */
#define CURS_COST 1
static struct dyn_con * dcost;
#define CURS_XPLREAD 4
static struct dyn_con * dxplread;
/*
 * Update the history in the Sybase database
 */
#define CURS_BADSORT_PLAN 5
static struct dyn_con * dbadsort_plan;
static char * sbadsort_plan="insert into BADSORT_PLAN(HASH_VAL,\n\
PLAN_TEXT) values (@hash_val,@plan_text)";
#define CURS_BADSORT_SQL 6
static struct dyn_con * dbadsort_sql;
static char * sbadsort_sql="insert into BADSORT_SQL(HASH_VAL,\n\
CUR_PLAN_ID,SQL_TEXT) values (@hash_val,@plan_id,@sql_text)";
#define CURS_BADSORT_INDEX 7
static struct dyn_con * dbadsort_index;
static char * sbadsort_index="insert into BADSORT_INDEX(PLAN_ID,INDEX_NAME)\n\
values (@plan_id,@index_name)";
#define CURS_BADSORT_TABLE 8
static struct dyn_con * dbadsort_table;
static char * sbadsort_table="insert into BADSORT_TABLE(PLAN_ID,TABLE_NAME)\n\
values (@plan_id,@table_name)";
#define CURS_BADSORT_USAGE 9
static struct dyn_con * dbadsort_usage;
static char * sbadsort_usage="insert into BADSORT_USAGE(SQL_ID,PLAN_ID,\n\
END_DTTM,EXECS,DISK_READS,BUFFER_GETS,KNOWN_TIME) values (@sql_id,@plan_id,\n\
to_date(@end_dttm,'DD Mon YYYY HH24:MI:SS'),@execs,@disk_reads,@buffer_gets,\n\
@known_time)";
#define CURS_BADSORT_CHKP 10
static struct dyn_con * dbadsort_chkp;
static char * sbadsort_chkp="select plan_id,plan_text from BADSORT_PLAN\n\
where hash_val=@hash_val";
#define CURS_BADSORT_CHKS 11
static struct dyn_con * dbadsort_chks;
static char * sbadsort_chks="select sql_id,cur_plan_id,sql_text from\n\
BADSORT_SQL where hash_val=@hash_val";
#define CURS_BADSORT_UPDSP 12
static struct dyn_con * dbadsort_updsp;
static char * sbadsort_updsp="update BADSORT_SQL set cur_plan_id=@plan_id\n\
where sql_id=@sql_id";
#define CURS_NEW_ID 13
static struct dyn_con * dnew_id;
static char * snew_id = "select @@identity";
#define CURS_NEW_MODULE 14
static struct dyn_con * dnew_module;
static char * snew_module = "insert into BADSORT_MODULE(module_name)\n\
values (@module_name)";
#define CURS_NEW_MAPPING 15
static struct dyn_con * dnew_mapping;
static char * snew_mapping = "insert into BADSORT_MAPPING(module_id, sql_id)\n\
values (@module_id, @sql_id)";
#define CURS_CLEAR_MAPPING 16
static struct dyn_con * dclear_mapping;
static char * sclear_mapping = "delete BADSORT_MAPPING where sql_id = @sql_id";
#define CURS_CHK_MODULE 17
static struct dyn_con * dchk_module;
static char * schk_module = "select module_id from BADSORT_MODULE\n\
where module_name = @module_name";
#define CURS_GET_DEFAULT 18
static struct dyn_con * dget_default;
static char * sget_default="select param_id,param_value from BADSORT_PARAMETER";
/****************************************************************************
 * Session manipulation statements
 */
#define CURS_TO_SYS 24
static struct dyn_con * dto_sys;
static char * sto_sys = "setuser";
#define CURS_TO_USER 25
static struct dyn_con * dto_user;
static char * sto_user = "setuser \"%s\"";
#define CURS_GET_USER 26
static struct dyn_con * dget_user;
static char * sget_user = "select suser_name(@user_id)";
/****************************************************************************
 * Set up the SQL statements
 */
static void open_all_sql()
{
    if (badsort_base.debug_level > 3)
        fputs("open_all_sql() called\n",perf_global.errout);
    set_def_binds(1,20);
    if (con != (struct sess_con *) NULL)
    {
        curse_parse(con, &dget_user, CURS_GET_USER, sget_user) ;
        curse_parse(con, &dto_sys, CURS_TO_SYS, sto_sys) ;
        dto_user = dyn_init(con, CURS_TO_USER) ;
    }
    if (con_explain != (struct sess_con *) NULL)
        dxplread = dyn_init(con_explain, CURS_XPLREAD) ;
    if (con_stats != (struct sess_con *) NULL)
    {
        curse_parse(con_stats, &dget_default, CURS_GET_DEFAULT,
            sget_default);
        curse_parse(con_stats, &dbadsort_plan, CURS_BADSORT_PLAN,
            sbadsort_plan);
        curse_parse(con_stats, &dbadsort_sql, CURS_BADSORT_SQL,
            sbadsort_sql);
        curse_parse(con_stats, &dbadsort_index, CURS_BADSORT_INDEX,
            sbadsort_index);
        curse_parse(con_stats, &dbadsort_table, CURS_BADSORT_TABLE,
            sbadsort_table);
        curse_parse(con_stats, &dbadsort_usage, CURS_BADSORT_USAGE,
            sbadsort_usage);
        curse_parse(con_stats, &dbadsort_chkp, CURS_BADSORT_CHKP,
            sbadsort_chkp);
        curse_parse(con_stats, &dbadsort_chks, CURS_BADSORT_CHKS,
            sbadsort_chks);
        curse_parse(con_stats, &dbadsort_updsp, CURS_BADSORT_UPDSP,
            sbadsort_updsp);
        curse_parse(con_stats, &dnew_id, CURS_NEW_ID, snew_id);
        curse_parse(con_stats, &dnew_module, CURS_NEW_MODULE, snew_module);
        curse_parse(con_stats, &dnew_mapping, CURS_NEW_MAPPING, snew_mapping);
        curse_parse(con_stats, &dclear_mapping, CURS_CLEAR_MAPPING,
                    sclear_mapping);
        curse_parse(con_stats, &dchk_module, CURS_CHK_MODULE, schk_module);
    }
    return;
}
/****************************************************************************
 * Load the bind variables into a dictionary
 */
static void load_dict()
{
struct dict_con * dict;
static int called = 0;
    if (badsort_base.debug_level > 3)
        fputs("load_dict() called\n",perf_global.errout);
    if (called)
        return;
    else
        called = 1;
    set_long(255);
    dict = new_dict( 20);
    dict_add(dict,"@disk_buf_ratio",ORA_FLOAT,  sizeof(double));
    dict_add(dict,"@buf_cost",ORA_FLOAT,  sizeof(double));
    dict_add(dict, "@plan_id", ORA_INTEGER, sizeof(long));
    dict_add(dict, "@hash_val", ORA_INTEGER, sizeof(long));
    dict_add(dict, "@plan_text", ORA_LONG, 16384);
    dict_add(dict, "@sql_id", ORA_INTEGER, sizeof(long));
    dict_add(dict, "@user_id", ORA_INTEGER, sizeof(long));
    dict_add(dict, "@execs", ORA_INTEGER, sizeof(long));
    dict_add(dict, "@disk_reads", ORA_INTEGER, sizeof(long));
    dict_add(dict, "@buffer_gets", ORA_INTEGER, sizeof(long));
    dict_add(dict, "@sql_text", ORA_LONG, 16384);
    dict_add(dict, "@index_name", ORA_CHAR, 65);
    dict_add(dict, "@table_name", ORA_CHAR, 65);
    dict_add(dict, "@end_dttm", ORA_CHAR, 21);
    dict_add(dict, "@known_time", ORA_INTEGER, sizeof(long));
    dict_add(dict, "@module_id", ORA_INTEGER, sizeof(long));
    dict_add(dict, "@module_name", ORA_CHAR, 250);
    (void) set_cur_dict(dict);
    return;
}
/*
 * Process options in the parameter table
 */
static int do_oracle_parameter()
{
static int rec_flag;                   /* Do not allow recursive entry */
char * ora_args[30];                   /* Dummy arguments to process */
char *p[2];
long l[2];
int argc;

    if (rec_flag)
        return 0;
    else
        rec_flag = 1;
    if (badsort_base.debug_level > 3)
        fputs("do_oracle_parameter() called\n", perf_global.errout);
    exec_dml(dget_default);
    if (con_stats->ret_status != 0)
    {
        scarper(__FILE__,__LINE__,"Unexpected Error fetching parameters");
        longjmp(ora_fail,1);         /* Give up if unexpected Sybase error */
    }
    optind = 1;
    dget_default->so_far = 0;
    while (dyn_locate(dget_default,&(l[0]),&(p[0])))
    {
        ora_args[optind] = strnsave(p[0],l[0]);
        optind++;
        if (l[1] > 0)
        {
            ora_args[optind] = strnsave(p[1],l[1]);
            optind++;
        }
        if (badsort_base.debug_level > 3)
        {
            fflush(stdout);
            fprintf(perf_global.errout,"%*.*s %*.*s\n",l[0],l[0],p[0],
                          l[1],l[1],p[1]);
            fflush(perf_global.errout);
        }
        if (optind > 29)
            break;
    }
    dyn_reset(dget_default); /* Releases the parameter block */
    ora_args[0] = "";
    argc = optind;
    opterr=1;                /* turn off getopt() messages */
    optind=1;                /* reset to start of list */
    proc_args(argc,ora_args);
    for (optind = 1; optind < argc; optind++)
        free(ora_args[optind]);
    rec_flag = 0;
    return 1;
}
/*
 * Switch schema to a named user
 */
void switch_schema(user_name)
char * user_name;
{
    if (badsort_base.debug_level > 3)
    {
        fputs("switch_schema(", perf_global.errout);
        fputs(user_name, perf_global.errout);
        fputs(") called\n", perf_global.errout);
        fflush(perf_global.errout);
    }
    if (!strcmp(user_name, "SYS"))
        exec_dml(dto_sys);
    else
    {
        sprintf(badsort_base.tbuf, sto_user, user_name);
        dto_user->statement = badsort_base.tbuf;
        prep_dml(dto_user);
        exec_dml(dto_user);
    }
    if (con_explain->ret_status != 0)
        scarper(__FILE__,__LINE__,"Unexpected Error switching schema");
    return;
}
#ifdef DEBUG
/*
 * Dump out a region of memory that is getting corrupted
 */
static void statp_dump(statp, label)
struct stat_con * statp;
char * label;
{
    fprintf(perf_global.errout, "%s: statp = %lx\n",
                  label, (long) statp );
    gen_handle(perf_global.errout, (char *) statp, 
              (char *) (statp + 1),1);
    fflush(perf_global.errout);
    return;
}
#endif
static void clearup()
{
    do_export(badsort_base.out_name);
    oracle_cleanup();
    return;
}
/*
 * Receipt of a termination signal
 */
void finish()
{
    clearup();
    match_dismantle();
    exit(0);
}
void catch_segv()
{
    sigset(SIGSEGV, SIG_DFL); 
    sigset(SIGBUS, SIG_DFL); 
    finish();
}
/*
 * Fatal Error - have another go
 */
extern char ** environ;
void restart()
{
static char buf[40];
int i;
static char *argv[]= {"badsort",
    "-m","",           /* Read in the checkpoint                  */
    "-o","",           /* New checkpoints will have the same name */
    "-d","",           /* Debug Level                             */
    "-c","",           /* Buffer Get Cost                         */
    "-r","",           /* Disk/buf ratio (for sorting)            */
    "-t","",           /* Periodicity                             */
    "-u","",           /* Statistics tables owner                 */
    "-n","",           /* Explain ID                              */
    "-q","","",0};        /* SQL Collection ID                       */
    fputs("Segmentation Fault?\n", stderr);
    read(0,(char *) &i,1);
    if (badsort_base.dont_save)
    {          /* The last export is corrupted */
        strcpy(&buf[0], badsort_base.out_name);
        strcat(&buf[0], "_bk");
#ifdef MINGW32
        unlink(badsort_base.out_name);
#endif
        rename(buf, badsort_base.out_name);    /* Restore last backup */
    }
    clearup();
    argv[2] = badsort_base.out_name;
    argv[4] = badsort_base.out_name;
    sprintf(buf,"%d",badsort_base.debug_level);
    argv[6] = strdup(buf);
    sprintf(buf,"%.8f",badsort_base.buf_cost);
    argv[8] = strdup(buf);
    sprintf(buf,"%.8f",badsort_base.disk_buf_ratio);
    argv[10] = strdup(buf);
    sprintf(buf,"%d",badsort_base.retry_time);
    argv[12] = strdup(buf);
    i = 13;
    if (badsort_base.uid_stats != (char *) NULL)
    {
        argv[i] = "-u";
        i++;
        argv[i] = badsort_base.uid_stats;
        i++;
    }
    if (badsort_base.uid_explain != (char *) NULL)
    {
        argv[i] = "-n";
        i++;
        argv[i] = badsort_base.uid_explain;
        i++;
    }
    if (badsort_base.uid != (char *) NULL)
    {
        argv[i] = "-q";
        i++;
        argv[i] = badsort_base.uid;
        i++;
    }
    argv[i] = "";     /* For the network file */
    i++;
    argv[i] = (char *) NULL;
    execvp(argv[0],argv);
    perror("execvp() failed");
    exit(0);
}
/*****************************************************************************
 *  Things that only need be done once per invocation
 */
void daemon_init()
{
char buf[128];
    (void) fputs("badsql.c - Performance Base SQL Capture Daemon\n",
                          perf_global.errout);
    perf_global.retry_time = 300;        /* Sybase login retry time   */
    perf_global.errout = stderr;
    perf_global.fifo_name = badsort_base.fifo_name;
    (void) unlink(perf_global.fifo_name);
    sprintf(buf,"%s.lck",perf_global.fifo_name);
    perf_global.lock_name = strdup(buf );
    (void) unlink(perf_global.lock_name);
/*
 * Forever; establish an Sybase connexion, and process files
 */
    sigset(SIGBUS, catch_segv);
    sigset(SIGPIPE, finish);
    sigset(SIGSEGV, catch_segv); 
    sigset(SIGTERM, finish);
    load_dict();                /* Identify the bind variable types    */
    return;
}
/*
 * Things that need to be done at the start of each Sybase session
 * - Return 0 if failed
 * - Return 1 if succeeded
 *
 * Set up as few connections as possible. With NT, the user is allowed to 
 * enter zero length place-holders, and the program will prompt for them.
 */
int oracle_init()
{
char *uid;

    if (badsort_base.debug_level)
        (void) fputs("oracle_init()\n", perf_global.errout);
    if ((badsort_base.uid != (char *) NULL) && *(badsort_base.uid) != '\0')
    {
        uid = strdup(badsort_base.uid);
        if ((con = dyn_connect(uid, "badsort")) == (struct sess_con *) NULL)
        {
            (void) fputs( "Collector Database Connect Failed\n",
                           perf_global.errout);
            free(uid);
            return 0;
        }
        badsort_base.in_con = con;
        free(uid);
    }
    if (badsort_base.uid_explain != (char *) NULL
     && (badsort_base.uid == (char *) NULL
       || strcmp(badsort_base.uid, badsort_base.uid_explain)))
    {
        {
            uid = strdup(badsort_base.uid_explain);
            if ((con_explain = dyn_connect(uid, "badsort"))
                                 == (struct sess_con *) NULL)
            {
                (void) fputs( "Explain Database Connect Failed\n",
                               perf_global.errout);
                free(uid);
                if (con != (struct sess_con *) NULL)
                {
                    dyn_disconnect(con);
                    con = (struct sess_con *) NULL;
                }
                return 0;
            }
            free(uid);
        }
    }
    else
        con_explain = con;
    if (badsort_base.uid_stats != (char *) NULL)
    {
        {
            if ((con == (struct sess_con *) NULL
                && con_explain == (struct sess_con *) NULL)
             || ((badsort_base.uid == (char *) NULL 
                 || strcmp(badsort_base.uid_stats, badsort_base.uid))
              && (badsort_base.uid_explain == (char *) NULL
               || strcmp(badsort_base.uid_stats, badsort_base.uid_explain))))
            {
                uid = strdup(badsort_base.uid_stats);
                if ((con_stats = dyn_connect(uid, "badsort"))
                                      == (struct sess_con *) NULL)
                    (void) fprintf(perf_global.errout,
                                 "Statistics Database Connect Failed\n");
                free(uid);
            }
            else
            if (badsort_base.uid_explain != (char *) NULL
              && !strcmp(badsort_base.uid_stats, badsort_base.uid_explain))
                con_stats = con_explain;
            else
                con_stats = con;
        }
    }
/*
 * Fail if no connexions to Sybase at all
 */
    if (con == (struct sess_con *) NULL
     && con_explain == (struct sess_con *) NULL
     && con_stats  == (struct sess_con *) NULL)
        return 0;
    open_all_sql();            /* Parse the SQL                   */
    return 1;
}
void oracle_cleanup()
{
static int called_again;

    if (called_again)
        return;
    called_again = 1;
    if (con != (struct sess_con *) NULL)
    {
        dbms_roll(con);
        dyn_disconnect(con);
    }
    if (con_explain != (struct sess_con *) NULL
     &&  con != con_explain)
    {
        dbms_roll(con_explain);
        dyn_disconnect(con_explain);
    }
    if (con_stats != (struct sess_con *) NULL
     &&  con_stats != con_explain
     &&  con != con_stats)
    {
        dbms_roll(con_stats);
        dyn_disconnect(con_stats);
    }
    con_stats = (struct sess_con *) NULL;
    con_explain = (struct sess_con *) NULL;
    con_stats = (struct sess_con *) NULL;
    (void) unlink(perf_global.fifo_name);
    (void) unlink(perf_global.lock_name);
    return;
}
/**************************************************************************
 * Main Program Start Here
 * VVVVVVVVVVVVVVVVVVVVVVV
 * Originally, I had thought that the process would be able to reconnect after
 * an Sybase session terminated, but that does not work reliably. We therefore
 * restart always.
 */
void run_daemon()
{
static int rec_flag;

    if (rec_flag)
    {
        fputs("run_daemon() called recursively - ignored\n", stderr); 
        return;
    }
    else
        rec_flag = 1;
    daemon_init();
/*
 * Login to the database.
 */
    if (oracle_init())
    {
/*
 * Process any back-log of query plans
 */
        if (!setjmp(ora_fail))
        {
        struct stat_con *statp;

            for (statp = badsort_base.anchor;
                    statp != (struct stat_con *) NULL ;
                        statp = statp->next)
            {
                if (statp->plan == (struct plan_con *) NULL)
                {
                    if (badsort_base.debug_level > 3)
                        (void) fputs(statp->sql, perf_global.errout);
                    find_the_plan(statp);
                }
            }
            return;
        }
        clearup();          /* Save the data         */
    }
    finish();
    return;
}
/**************************************************************************
 * Routine to inject line feeds into SQL statements so that they are more
 * readable.
 *
 * Parameters :
 * - Length
 * - Statement
 */
static void printifysql(len,buf,dest)
int len;
char * buf;
char * dest;
{
char * bound;
char * top = buf + len;
char *x1, *x2;
int i;
register char *x = top -1;
/*
 * Trim leading and trailing white space
 */
    while ( x > buf && (isspace(*x) || *x == '\0'))
        x--;
    bound = x;
    top = x + 1;
    *top = '\0';
    x = buf; 
    while (isspace(*x))
        x++;
    buf = x;
#ifdef PRINTIFY
/*
 * Break up the statement into separate lines:
 * - Do not split strings over lines.
 * - Inject line feeds when multiple spaces are not in a string and
 *   the multiples do not appear at the start of the line.
 *
 * This code will not do much useful if variable names enclosed in
 * " characters include single quotes. 
 *
 * x1 -  Is the start of the current line.
 * x2 -  Is how far back we should look to find a break character.
 *       It is past the end of the last string, if there are any.
 *    -  If flag1 == 1, it is the start of the current string
 * x  -  is where we are working.
 *
 * flag1 = 0 - Not printed, and we are not in a string
 *       = 1 - We are in a string
 *       = 2 - We have printed the line
 * flag2 = 0 - We have only seen white space so far on this line.
 *       = 1 - We have seen a a character on the line.
 */
    for (x1 = x, x2 = x; x <= bound; )
    {
    int flag1, flag2;
/*
 * Search forwards
 */
        for (i = 78, flag1=0, flag2 = 0; i; i--, x++)
        {
            if (x >= bound || *x == '\n' ||
               (flag1 == 0 && flag2 == 1 && *x == ' '
                && x != bound && *(x+1) == ' ' ))
            {
/*
 * We print the line so far if:
 * - We have reached the end of the statement
 * - We have encountered a line feed
 * - We are not in a string, we have seen a non-space character, and we
 *   encounter two adjacent spaces (the assumption being that multiple
 *   spaces are only used for indenting at the start of a line)
 */
                memcpy(dest, x1, (x-x1 + 1));
                dest += (x-x1+1);
                if (flag1 == 0 && flag2 == 1
                    && *x == ' ' && x != bound && *(x+1) == ' ')
                {
                    *dest = '\n';
                    dest++;
                }
                x1 = x + 1;
                x = x1;
                x2 = x1;
                flag1 = 2;    /* ie. We have printed it */
                break;
            }
            else
            if (*x == '\'')
            {
/*
 * We have encountered a quote character
 */
                flag2 = 1;    /* We have seen a non-space character */
                if (flag1 == 0)
                {
                    flag1 = 1; /* We are in a string */
                    x2 = x;
                }
                else
                if (*(x+1) != '\'' )
                {
                    x2 = x + 1;
                    flag1 = 0; /* We are out of the string */
                }
                else
                {
                    x++;  /* Skip the second quote */
                }
            }
            else
            if (*x != ' ')
                flag2 = 1;
        }
/*
 * At this point, either we have written the line (we found or
 * injected a line feed) or we have gone 78 characters. In this case, if we
 * are in a string, we search forwards for its end, otherwise, we move x
 * backwards until it points to a space, or we reach the bounds of a string
 * (that is, x2)
 */
        if ( flag1 == 1)
        {   /* We are in the middle of a string */
            if (x2 != x1)
            {    /* There was data before the string */
                memcpy(dest,x1,(x2 - x1));
                dest += (x2 - x1);
                *dest = '\n';
                dest++;
                x = x2;
                x1 = x2; 
            }
            else   /* The string goes past 78; search for its
                      end */
            {
                 for (x = x2 + 1;
                        x <= bound && *x != '\n' && *x != '\0';
                            x++)
                      if (*x == '\'')
                      {
                          if (x == bound || *(x+1) != '\'')
                              break;
                          else
                              x++;
                      }
                 memcpy(dest,x1, (x - x1 +1));
                 dest += (x - x1 +1);
                 x++;
/*
 * Inject a line feed
 */
                 if (x <= bound && *x != '\n')
                 {
                     *dest = '\n';
                     dest++;
                 }
                 x1 = x; 
                 x2 = x;
            }
        }
        else
        if ( flag1 == 0 )
        {
/*
 * Search back for a break character
 */
            for (; x > x2 && *x != ' ' && *x != ',' && *x != ')' ; x--);
            memcpy(dest,x1, (x - x1 + 1));
            dest += (x - x1 +1);
            if (x != x2)
            {
                *dest = '\n';
                dest++;
            }
            x1 = x + 1;
            x2 = x1;
        }
    } 
    *dest = '\0';
#else
    memcpy(dest, buf, (top - buf) + 1);
#endif
    return;
}
/*
 * Get the schema name for the collection user id.
 */
static char * get_collector()
{
int len;
char * name_ret;

    if (badsort_base.debug_level > 3)
        fputs("get_collector()\n",perf_global.errout);
    curse_parse(con, &dto_sys, CURS_TO_SYS, "select suser_name()") ;
    exec_dml(dto_sys);
    dto_sys->so_far = 0;
    if (! dyn_locate(dto_sys, &len, &name_ret))
        longjmp(ora_fail,1);         /* Give up if unexpected Sybase error */
    name_ret = strnsave(name_ret, len);
    if (badsort_base.debug_level > 3)
    {
        fputs("returns ", perf_global.errout);
        fputs(name_ret, perf_global.errout);
        fputc('\n', perf_global.errout);
        fflush(perf_global.errout);
    }
    dyn_reset(dto_sys);
    return name_ret;
}
/*
 * Get the schema name for a parsing user.
 */
char * get_user_name(user_id)
long user_id;
{
int len;
char * name_ret;
    if (badsort_base.debug_level > 3)
        fputs("get_schema_name()\n",perf_global.errout);
    add_bind(dget_user, E2FLONG, sizeof(long int), &user_id);
    exec_dml(dget_user);
    dget_user->so_far = 0;
    if (! dyn_locate(dget_user, &len, &name_ret))
        longjmp(ora_fail,1);         /* Give up if unexpected Sybase error */
    name_ret = strnsave(name_ret, len);
    if (badsort_base.debug_level > 3)
    {
        fputs("returns ", perf_global.errout);
        fputs(name_ret, perf_global.errout);
        fputc('\n', perf_global.errout);
        fflush(perf_global.errout);
    }
    dyn_reset(dget_user);
    return name_ret;
}
/****************************************************************************
 * Execute the SQL statement to get its plan. The session has had showplan
 * and noexec set.
 */
static void find_the_plan(statp)
struct stat_con * statp;
{
int len;

    if (badsort_base.debug_level > 3)
        fprintf(perf_global.errout, "find_the_plan(%lx) called\n",
                  (long) statp );
    if (!strncasecmp("open ",statp->sql,5)
     || !strncasecmp("fetch ",statp->sql,6)
     || !strncasecmp("close ",statp->sql,6))
        return;
#ifdef DEBUG
    statp_dump(statp, "Incoming");
#endif
    (void) dbms_option(dxplread->con, CS_OPT_SHOWPLAN, CS_TRUE);
    (void) dbms_option(dxplread->con, CS_OPT_NOEXEC, CS_TRUE);
/*
 * We may have to do something about the user and database name here, to get
 * the execution environment correct.
 */
    dxplread->statement = statp->sql;
/*
 * Deal with statements that are only allowed in stored procedures or cursor
 * declarations
 */ 
    len = strlen(statp->sql);
    if (len > 13 && !strncasecmp("for read only",statp->sql + len -13,13))
        *(statp->sql + len - 13) = '\0';
    else
        len = 0;
    dxplread->con->msgp = &(dxplread->con->message_block[0]);
#ifdef DEBUG
    statp_dump(statp, "Before prepare");
#endif
    prep_dml(dxplread);
#ifdef DEBUG
    statp_dump(statp, "Before exec");
#endif
    exec_dml(dxplread);
    if (dxplread->con->msgp != &(dxplread->con->message_block[0]))
    {
        *(dxplread->con->msgp) = '\0';
#ifdef DEBUG
        statp_dump(statp, "After exec");
#endif
        statp->plan = do_one_plan(&(dxplread->con->message_block[0]));
        dxplread->con->msgp = &(dxplread->con->message_block[0]);
        if (con_stats != (struct sess_con *) NULL
          && statp->plan != (struct plan_con *) NULL)
            do_oracle_plan(statp->plan);
    }
    if (len)
        *(statp->sql + len - 13) = 'F';
    return;
}
/****************************************************************************
 * Process a whole statement found in sybextlib.c
 */
static void do_one_state(cur_stat, slen)
struct stat_con *cur_stat;
int slen;
{
struct stat_con  *statp;

    if (badsort_base.debug_level > 3)
        fputs("do_one_state() called\n", perf_global.errout);
    printifysql(slen, cur_stat->sql, badsort_base.tlook);
#ifdef DEBUG
    fputs(badsort_base.tlook, perf_global.errout);
    fputc('\n', perf_global.errout);
    fflush(perf_global.errout);
#endif
/*
 * Process this statement
 */
    statp = do_one_sql(badsort_base.tlook, &(cur_stat->total), 2);
#ifdef DEBUG
    statp_dump(statp, "Initialised");
#endif
    if (statp->plan == (struct plan_con *) NULL
      && dxplread != (struct dyn_con * ) NULL)
    {
        statp->user_id = cur_stat->user_id;
        find_the_plan(statp);
    }
    do_oracle_sql(statp);
    if (badsort_base.debug_level > 2)
        stat_print(perf_global.errout,statp);
    return;
}
/****************************************************************************
 * Handle a SQL statement picked up by the network tracer.
 * -  Initialise the time stamp
 * -  Execute the statement
 * -  Loop through the fetched data.
 * -  Call the same logic we use for processing statements from files in
 *    order to see if we already have it.
 * -  If we do not have a plan for it, compute the plan using the appropriate
 *    explain ID. We will later need to add:
 *    -  Logic to set the optimiser setting as it was when the statement
 *       was parsed
 *    -  The ability to reset plans, to take account of changes
 *    -  The ability to track different plans used by different parsing
 *       user ID's.
 *
 * The function returns normally unless an unexpected Sybase error occurs, in
 * which case we assume that we need to log out, and log back in again.
 */
void do_one_capture(slen, sql, timestamp, response)
int slen;
unsigned char * sql;
time_t timestamp;
double response;
{
static struct stat_con cur_stat;

    if (badsort_base.debug_level > 3)
        fputs("do_one_capture() called\n", perf_global.errout);
    if (!slen)
        return;
    if (((int) timestamp - (int) cur_stat.total.t) > badsort_base.retry_time)
    {
        if (cur_stat.total.t != 0)
        {
/*
 * Checkpoint time.
 */
            if (badsort_base.uid_stats == (char *) NULL)
            {
                do_export(badsort_base.out_name);   /* Save the data */
                if (badsort_base.dont_save)
                    restart();
            }
/*
 * Once each pass, check whatever is in the parameter table
 */
            else
                do_oracle_parameter();
        }
        cur_stat.total.t = timestamp;
    }
    cur_stat.total.executions = 1.0;
    cur_stat.total.diff_time = badsort_base.retry_time;
    cur_stat.total.buffer_gets = response;
    cur_stat.sql = sql;
    do_one_state(&cur_stat, slen);
    return;
}
/*****************************************************************************
 * Handle unexpected errors
 */
extern int errno;
void scarper(file_name,line,message)
char * file_name;
int line;
char * message;
{
static int recurse_flag;

    if (recurse_flag)
        return;
    recurse_flag = 1;
        
    fflush(stdout);
    (void) fprintf(perf_global.errout,"Unexpected Error %s,line %d\n",
                   file_name,line);
    if (errno != 0)
    {
        perror(message);
        (void) fprintf(perf_global.errout,"UNIX Error Code %d\n", errno);
    }
    else
        fputs(message,perf_global.errout);
    if (con != (struct sess_con *) NULL && con->ret_status != 0)
        dbms_error(con);
    if (con_explain != (struct sess_con *) NULL
      && con_explain->ret_status != 0)
        dbms_error(con_explain);
    if (con_stats != (struct sess_con *) NULL
      && con_stats->ret_status != 0)
        dbms_error(con_stats);
    fflush(perf_global.errout);
    recurse_flag = 0;
    return;
}
/****************************************************************************
 * Functions that update the Sybase statistics.
 *
 * Get the next ID (Plan or SQL or Module).
 */
int get_new_id()
{
int len;
char * num_ret;
    if (badsort_base.debug_level > 3)
        fputs("get_new_id()\n",perf_global.errout);
    exec_dml(dnew_id);
    dnew_id->so_far = 0;
    if (! dyn_locate(dnew_id, &len, &num_ret))
        longjmp(ora_fail,1);         /* Give up if unexpected Sybase error */
    len = atoi(num_ret);
    if (badsort_base.debug_level > 3)
    {
        fputs("returns \n", perf_global.errout);
        fputs(num_ret, perf_global.errout);
        fputc('\n', perf_global.errout);
    }
    dyn_reset(dnew_id);
    return len;
}
/***********************************************************************
 * Add a reference to a table for a plan
 */
void do_oracle_table(plan_id,len,table_name)
long int plan_id;
int len;
char * table_name;
{
    add_bind(dbadsort_table, E2FLONG, sizeof(long int), &plan_id);
    add_bind(dbadsort_table, FIELD, len, table_name);
    exec_dml(dbadsort_table);
    return;
}
/***********************************************************************
 * Add a reference to an index for a plan
 */
void do_oracle_index(plan_id,len,index_name)
long int plan_id;
int len;
char * index_name;
{
    add_bind(dbadsort_index, E2FLONG, sizeof(long int), &plan_id);
    add_bind(dbadsort_index, FIELD, len, index_name);
    exec_dml(dbadsort_index);
    return;
}
/*
 * Process a PLAN which we do not know to be in the database already
 */
static void do_oracle_plan(planp)
struct plan_con * planp;
{
char *p[2];
int l[2];
int len = strlen(planp->plan);
/*
 * Loop - process all Plans with the same hash_val
 * - Compare the Plan texts
 * - Break on match
 */
    if (con_stats == (struct sess_con *) NULL || planp->plan_id != 0)
        return;
    add_bind(dbadsort_chkp, E2FLONG, sizeof(int), &(planp->hash_val));
    exec_dml(dbadsort_chkp);
    dbadsort_chkp->so_far = 0;
    while (dyn_locate(dbadsort_chkp,&(l[0]),&(p[0])))
    {
        if (len == l[1] && !memcmp(p[1], planp->plan,len))
        {
            planp->plan_id = atoi(p[0]);
            break;
        }
    }
    dyn_reset(dbadsort_chkp);
/*
 * If there are no matches
 * - Get a new PLAN_ID
 * - Create a new PLAN record 
 */
    if (planp->plan_id == 0)
    {
        add_bind(dbadsort_plan, E2FLONG, sizeof(int), &(planp->hash_val));
        add_bind(dbadsort_plan, FIELD, len, planp->plan);
        exec_dml(dbadsort_plan);
        planp->plan_id = get_new_id();
/*
 * - Process any Indexes and Tables for this plan
 *   - The logic to find them is in do_ind_tab(), but this will need to
 *     be changed so that it distinguishes between Sybase and ACCESS output
 * Note the allocated or found PLAN_ID in the plan_con structure.
 */
        do_ind_tab(planp->plan, (FILE *) NULL, (FILE *) NULL, planp->plan_id);
    }
    return;
}
/*
 * Create new usage records. The when_seen structures are chained together
 * in reverse order of time. We continue until we fail with a duplicate, or
 * we find a record that has already been loaded.
 */
static void do_oracle_usage(sql_id,plan_id,anchor)
long int sql_id;
long int plan_id;
struct when_seen * anchor;
{
struct when_seen * w;
    for ( w = anchor;
            w != (struct when_seen *) NULL && w->ora_flag == 0;
                w = w->next)
    {
        if (w->disk_reads != 0 || w->buffer_gets != 0)
        {
        char *x = to_char("DD Mon YYYY HH24:MI:SS", (double) (w->t));
            add_bind(dbadsort_usage, E2FLONG, sizeof(int), &(sql_id));
            add_bind(dbadsort_usage, E2FLONG, ((plan_id)?sizeof(int):0),
                      &(plan_id));
            add_bind(dbadsort_usage, FIELD, strlen(x), x);
            add_bind(dbadsort_usage, FNUMBER, sizeof(double), &(w->executions));
            add_bind(dbadsort_usage, FNUMBER, sizeof(double), &(w->disk_reads));
            add_bind(dbadsort_usage, FNUMBER, sizeof(double), &(w->buffer_gets));
            add_bind(dbadsort_usage, E2FLONG, ((w->diff_time)?sizeof(int):0),
                                &(w->diff_time));
            exec_dml(dbadsort_usage);
        }
        w->ora_flag = 1;
        if (con_stats->ret_status != 0 && con != (struct sess_con *) NULL)
        {  /* We have already done this one. Clear the others, but keep 1 */
            clear_whens(w->next);
            w->next = (struct when_seen *) NULL;
            break;
        }
    }
    return;
}
/*
 * Load an SQL statement into the database
 */
void do_oracle_sql(statp)
struct stat_con * statp;
{
int plan_id;
char *p[3];
int l[3];
int len;

    if (con_stats == (struct sess_con *) NULL)
        return;
    len = strlen(statp->sql);
/*
 * Process the Plan first, if there is one, and it has not been done.
 */
    if (statp->plan != (struct plan_con *) NULL)
    {
        if (statp->plan->plan_id == 0)
            do_oracle_plan(statp->plan);
        plan_id = statp->plan->plan_id;
    }
    else
        plan_id = 0;
/*
 * Loop - process all SQL statements with the same hash_val
 * - Compare the SQL texts
 * - Break on match
 */
    if (statp->sql_id == 0)
    {
        add_bind(dbadsort_chks, E2FLONG, sizeof(int), &(statp->hash_val));
        exec_dml(dbadsort_chks);
        dbadsort_chks->so_far = 0;
        while (dyn_locate(dbadsort_chks,&(l[0]),&(p[0])))
        {
            if (len == l[2] && !memcmp(p[2], statp->sql,len))
            {
                statp->sql_id = atoi(p[0]);
                if (statp->plan != (struct plan_con *) NULL
                  && statp->plan->plan_id != 0
                  && statp->plan->plan_id != atoi(p[1]))
                {
                    add_bind(dbadsort_updsp, E2FLONG, sizeof(int),
                             &(statp->plan->plan_id));
                    add_bind(dbadsort_updsp, E2FLONG, sizeof(int),
                             &(statp->sql_id));
                    exec_dml(dbadsort_updsp);
                }
                break;
            }
        }
        dyn_reset(dbadsort_chks);
/*
 * If there are no matches
 * - Get a new SQL_ID
 * - Create a new SQL record 
 * If there is a match, we might think about updating the sql
 * In either case, we must note the SQL_ID in the stat_con structure.
 */
        if (statp->sql_id == 0)
        {
            add_bind(dbadsort_sql, E2FLONG, sizeof(int), &(statp->hash_val));
            add_bind(dbadsort_sql, E2FLONG, ((plan_id)?sizeof(int):0),
                         &(plan_id));
            add_bind(dbadsort_sql, FIELD, len, statp->sql);
            exec_dml(dbadsort_sql);
            statp->sql_id = get_new_id();
        }
    }
/*
 * In all cases create a new usage record.
 */
    do_oracle_usage(statp->sql_id,plan_id,statp->anchor);
    dbms_commit(con_stats);
    return;
}
/*
 * Load the Possible Module Mappings for an SQL statement into the database
 */
void do_oracle_mapping(statp)
struct stat_con * statp;
{
char *p;
int l;
struct prog_con * pc;
    if (con_stats == (struct sess_con *) NULL || statp->sql_id == 0)
        return;
/*
 * Clear down the statement first.
 */
    add_bind(dclear_mapping, E2FLONG, sizeof(int), &(statp->sql_id));
    exec_dml(dclear_mapping);
/*
 * Loop - process all possible modules for this statement
 * - Check if the module is already known
 * - If it is not, add it
 * - Then add the mapping
 */
    for (pc = statp->cand_prog; pc != (struct prog_con *) NULL; pc = pc->next)
    {
        add_bind(dchk_module, FIELD, strlen(pc->name), pc->name);
        exec_dml(dchk_module);
        dchk_module->so_far = 0;
        while (dyn_locate(dchk_module,&l,&p));
        if (dchk_module->so_far == 0)
        {
            add_bind(dnew_module, FIELD, strlen(pc->name), pc->name);
            exec_dml(dnew_module);
            pc->module_id = get_new_id();
        }
        else
            pc->module_id = atoi(p);
        dyn_reset(dchk_module);
        add_bind(dnew_mapping, E2FLONG, sizeof(int), &(pc->module_id));
        add_bind(dnew_mapping, E2FLONG, sizeof(int), &(statp->sql_id));
        exec_dml(dnew_mapping);
    }
    dbms_commit(con_stats);
    return;
}
/*
 * Initialise the tables used by the Sybase badsort implementation
 */
static char *badsort_ddl[] = { "create table BADSORT_PLAN (\n\
    PLAN_ID numeric identity,\n\
    HASH_VAL numeric not null,\n\
    PLAN_TEXT text not null,\n\
constraint BADSORT_PLAN_PK primary key (PLAN_ID))",
"create index BADSORT_PLAN_I1 on BADSORT_PLAN(HASH_VAL)",
"create table BADSORT_SQL (\n\
    SQL_ID numeric identity,\n\
    HASH_VAL numeric not null,\n\
    CUR_PLAN_ID numeric null,\n\
    SQL_TEXT text not null,\n\
constraint BADSORT_SQL_PK primary key (SQL_ID),\n\
constraint BADSORT_PLAN_FK\n\
    foreign key (CUR_PLAN_ID) references BADSORT_PLAN(PLAN_ID) disable)",
"create index BADSORT_SQL_I1 on BADSORT_SQL(HASH_VAL)",
"create table BADSORT_INDEX (\n\
    PLAN_ID numeric not null,\n\
    INDEX_NAME varchar(64) null,\n\
constraint BADSORT_INDEX_PK primary key(PLAN_ID,INDEX_NAME),\n\
constraint BADSORT_INDEX_PLAN_FK foreign key (PLAN_ID)\n\
     references BADSORT_PLAN(PLAN_ID) disable)",
"create unique index BADSORT_INDEX_UI1 on BADSORT_INDEX(INDEX_NAME,PLAN_ID)",
"create table BADSORT_TABLE(\n\
    PLAN_ID numeric not null,\n\
    TABLE_NAME varchar(64) not null,\n\
constraint BADSORT_TABLE_PK primary key(PLAN_ID,TABLE_NAME),\n\
constraint BADSORT_TABLE_PLAN_FK foreign key  (PLAN_ID)\n\
    references BADSORT_PLAN(PLAN_ID) disable)",
"create unique index BADSORT_TABLE_UI1 on BADSORT_TABLE(TABLE_NAME,PLAN_ID)",
"create table BADSORT_MODULE(\n\
    MODULE_ID numeric identity,\n\
    MODULE_NAME varchar(250) not null,\n\
constraint BADSORT_MODULE_PK primary key(MODULE_ID),\n\
constraint BADSORT_MODULE_U1 unique  (MODULE_NAME))",
"create table BADSORT_USAGE(\n\
    SQL_ID numeric not null,\n\
    END_DTTM datetime not null,\n\
    EXECS numeric not null,\n\
    DISK_READS numeric not null,\n\
    BUFFER_GETS numeric not null,\n\
    PLAN_ID numeric null,\n\
    KNOWN_TIME numeric null,\n\
constraint BADSORT_USAGE_UK unique (SQL_ID,END_DTTM,PLAN_ID),\n\
constraint BADSORT_USAGE_SQL_FK foreign key (SQL_ID)\n\
    references BADSORT_SQL(SQL_ID) disable,\n\
constraint BADSORT_USAGE_PLAN_FK foreign key (PLAN_ID)\n\
    references BADSORT_PLAN(PLAN_ID) disable)",
"create unique index BADSORT_USAGE_UI1 on BADSORT_USAGE(PLAN_ID,END_DTTM,SQL_ID)",
"create unique index BADSORT_USAGE_UI2 on BADSORT_USAGE(END_DTTM,SQL_ID,PLAN_ID)",
"create table BADSORT_MAPPING(\n\
    MODULE_ID numeric not null,\n\
    SQL_ID numeric not null,\n\
constraint BADSORT_MAPPING_PK primary key(MODULE_ID, SQL_ID),\n\
constraint BADSORT_MAPPING_SQL_FK foreign key (SQL_ID)\n\
    references BADSORT_SQL(SQL_ID) disable,\n\
constraint BADSORT_MAPPING_MODULE_FK foreign key (MODULE_ID)\n\
    references BADSORT_MODULE(MODULE_ID) disable)",
"create unique index BADSORT_MAPPING_UI1 on BADSORT_MAPPING(SQL_ID,MODULE_ID)",
NULL };
/*
 * Create the badsort tables in an Sybase database.
 */
void do_oracle_tables()
{
char **x;
    load_dict();
    if (oracle_init())
    {
        if (!setjmp(ora_fail))
        {
            for (x = badsort_ddl; *x != (char *) NULL; x++)
            {
                curse_parse(con, &dcost, CURS_COST, *x);
                exec_dml(dcost);
            }
            oracle_cleanup();
        }
    }
    return;
}
/*
 * Load up a badsort file into a database. Used for loading foreign data.
 */
void do_oracle_imp()
{
struct stat_con *statp;

    load_dict();
    if (oracle_init())
    {
        if (!setjmp(ora_fail))
        {
            for ( statp = badsort_base.anchor;
                    statp != (struct stat_con *) NULL ;
                        statp = statp->next)
            {
                do_oracle_sql(statp);
                do_oracle_mapping(statp);
            }
            oracle_cleanup();
        }
    }
    return;
}
/******************************************************************************
 * Commands to initialise a badsort working directory. These are Sybase
 * reserved words, other Sybase keywords, built-in functions and global
 * variables.
 */ 
static char * res_words[] = { "ABS", "ABSOLUTE", "ACOS", "ACTION", "ACTIVATION",
"ADD", "AFTER", "ALIAS", "ALL", "ALLOCATE", "ALTER", "AND", "ANY", "ARE",
"ARITH_OVERFLOW", "AS", "ASC", "ASCII", "ASIN", "ASSERTION", "ASYNC",
"AT", "ATAN", "ATN2", "AUTHORIZATION", "AVG", "BEFORE", "BEGIN",
"BETWEEN", "BIT", "BIT_LENGTH", "BOOLEAN", "BOTH", "BREADTH", "BREAK",
"BROWSE", "BULK", "BY", "CALL", "CASCADE", "CASCADED", "CASE", "CAST",
"CATALOG", "CEILING", "CHAR", "CHAR_CONVERT", "CHAR_LENGTH", "CHARACTER",
"CHARACTER_LENGTH", "CHARINDEX", "CHECK", "CHECKPOINT", "CLOSE",
"CLUSTERED", "COALESCE", "COL_LENGTH", "COL_NAME", "COLLATE", "COLLATION",
"COLUMN", "COMMIT", "COMPLETION", "COMPUTE", "CONFIRM", "CONNECT",
"CONNECTION", "CONSTRAINT", "CONSTRAINTS", "CONSUMERS", "CONTINUE",
"CONTROLROW", "CONVERT", "CORRESPONDING", "COS", "COT", "COUNT", "CREATE",
"CROSS", "CURRENT", "CURRENT_DATE", "CURRENT_TIME", "CURRENT_TIMESTAMP",
"CURRENT_USER", "CURSOR", "CURUNRESERVEDPGS", "CYCLE", "DATA", "DATA_PGS",
"DATABASE", "DATALENGTH", "DATE", "DATEADD", "DATEDIFF", "DATENAME",
"DATEPART", "DAY", "DB_ID", "DB_NAME", "DBCC", "DEALLOCATE", "DEC",
"DECIMAL", "DECLARE", "DEFAULT", "DEFERRABLE", "DEFERRED", "DEGREES",
"DELETE", "DEPTH", "DESC", "DESCRIBE", "DESCRIPTOR", "DIAGNOSTICS",
"DICTIONARY", "DIFFERENCE", "DISCONNECT", "DISK", "DISTINCT", "DOMAIN",
"DOUBLE", "DUMMY", "DUMP", "EACH", "ELSE", "ELSEIF", "END", "END-EXEC",
"ENDTRAN", "EQUALS", "ERRLVL", "ERRORDATA", "ERROREXIT", "ESCAPE",
"EXCEPT", "EXCEPTION", "EXCLUSIVE", "EXEC", "EXECUTE", "EXISTS", "EXIT",
"EXP", "EXTERNAL", "EXTRACT", "FALSE", "FETCH", "FILLFACTOR", "FIRST",
"FLOAT", "FLOOR", "FOR", "FOREIGN", "FOUND", "FROM", "FULL", "GENERAL",
"GET", "GETDATE", "GLOBAL", "GO", "GOTO", "GRANT", "GROUP", "HAVING",
"HEXTOINT", "HOLDLOCK", "HOST_ID", "HOST_NAME", "HOUR", "IDENTITY_INSERT",
"IDENTITY_START", "IF", "IGNORE", "IMMEDIATE", "IN", "INDEX", "INDEX_COL",
"INDICATOR", "INITIALLY", "INNER", "INPUT", "INSENSITIVE", "INSERT",
"INT", "INTEGER", "INTERSECT", "INTERVAL", "INTO", "INTTOHEX", "IS",
"IS_SEC_SERVICE_ON", "ISNULL", "ISOLATION", "JOIN", "KEY", "KILL",
"LANGUAGE", "LAST", "LCT_ADMIN", "LEADING", "LEAVE", "LEFT", "LESS",
"LEVEL", "LIKE", "LIMIT", "LINENO", "LOAD", "LOCAL", "LOG", "LOG10",
"LOOP", "LOWER", "LTRIM", "MATCH", "MAX", "MAX_ROWS_PER_PAGE",
"MEMBERSHIP", "MIN", "MINUTE", "MIRROR", "MIRROREXIT", "MODIFY",
"MODULE", "MONTH", "MUT_EXCL_ROLES", "NAMES", "NATIONAL", "NATURAL",
"NCHAR", "NEW", "NEXT", "NO", "NOHOLDLOCK", "NONCLUSTERED", "NONE",
"NOT", "NULL", "NULLIF", "NUMERIC", "NUMERIC_TRANSACTION", "OBJECT",
"OBJECT_ID", "OBJECT_NAME", "OCTET_LENGTH", "OF", "OFF", "OFFSETS",
"OID", "OLD", "ON", "ONCE", "ONLINE", "ONLY", "OPEN", "OPERATION",
"OPERATORS", "OPTION", "OR", "ORDER", "OTHERS", "OUTER", "OUTPUT", "OVER",
"OVERLAPS", "PAD", "PARAMETERS", "PARTIAL", "PARTITION", "PASSWD",
"PATINDEX", "PENDANT", "PERM", "PERMANENT", "PI", "PLAN", "POSITION",
"POWER", "PRECISION", "PREORDER", "PREPARE", "PRESERVE", "PRIMARY",
"PRINT", "PRIOR", "PRIVATE", "PRIVILEGES", "PROC", "PROC_ROLE",
"PROCEDURE", "PROCESSEXIT", "PROTECTED", "PROXY", "PTN_DATA_PGS",
"PUBLIC", "RADIANS", "RAISERROR", "RAND", "READ", "READTEXT", "REAL",
"RECONFIGURE", "RECURSIVE", "REF", "REFERENCES", "REFERENCING",
"RELATIVE", "REPLACE", "REPLICATE", "RESERVED_PGS", "RESIGNAL",
"RESTRICT", "RETURN", "RETURNS", "REVERSE", "REVOKE", "RIGHT", "ROLE",
"ROLE_CONTAIN", "ROLE_ID", "ROLE_NAME", "ROLLBACK", "ROUND", "ROUTINE",
"ROW", "ROWCNT", "ROWCOUNT", "ROWS", "RTRIM", "RULE", "SAVE", "SAVEPOINT",
"SCHEMA", "SCROLL", "SEARCH", "SECOND", "SECTION", "SELECT", "SENSITIVE",
"SEQUENCE", "SESSION", "SESSION_USER", "SET", "SETUSER", "SHARED",
"SHOW_ROLE", "SHOW_SEC_SERVICES", "SHUTDOWN", "SIGN", "SIGNAL", "SIMILAR",
"SIN", "SIZE", "SMALLINT", "SOME", "SOUNDEX", "SPACE", "SQL", "SQLCODE",
"SQLERROR", "SQLEXCEPTION", "SQLSTATE", "SQRT", "SS", "YY", "QQ", "MM",
"DY", "DD", "DW", "WK", "HH", "MI", "MS", "STATISTICS", "STR", "STRIPE",
"STRUCTURE", "STUFF", "SUBSTRING", "SUM", "SUSER_ID", "SUSER_NAME",
"SYB_IDENTITY", "SYB_RESTREE", "SYSTEM_USER", "TABLE", "TAN", "TEMP",
"TEMPORARY", "TEST", "TEXTPTR", "TEXTSIZE", "TEXTVALID", "THEN", "THERE",
"TIME", "TIMESTAMP", "TIMEZONE_HOUR", "TIMEZONE_MINUTE", "TO", "TRAILING",
"TRAN", "TRANSACTION", "TRANSLATE", "TRANSLATION", "TRIGGER", "TRIM",
"TRUE", "TRUNCATE", "TSEQUAL", "TYPE", "UNDER", "UNION", "UNIQUE",
"UNKNOWN", "UNPARTITION", "UPDATE", "UPPER", "USAGE", "USE", "USED_PGS",
"USER", "USER_ID", "USER_NAME", "USER_OPTION", "USING", "VALID_NAME",
"VALID_USER", "VALUE", "VALUES", "VARCHAR", "VARIABLE", "VARYING", "VIEW",
"VIRTUAL", "VISIBLE", "WAIT", "WAITFOR", "WHEN", "WHENEVER", "WHERE",
"WHILE", "WITH", "WITHOUT", "WORK", "WRITE", "WRITETEXT", "YEAR", "ZONE",
"@@IDENTITY","@@ROWCOUNT", "@@CONNECTIONS", "@@CPU_BUSY","@@DBTS", "@@ERROR",
"@@IDLE", "@@IO_BUSY","@@LANGID","@@LANGUAGE","@@MAX_CONNECTIONS","@@NESTLEVEL",
"@@PACK_ERRORS","@@PACK_RECEIVED","@@PACK_SENT","@@PROCID","@@ROWCOUNT",
"@@SERVERNAME","@@SPID","@@TEXTSIZE","@@TIMETICKS","@@TOTAL_ERRORS",
"@@TOTAL_READ","@@TOTAL_WRITE","@@TRANCOUNT","@@VERSION",
NULL};

void searchterms_ini()
{
FILE * nfp;
char **x;

    if ((nfp = fopen(searchterms, "wb")) != (FILE *) NULL)
    {
        for (x = res_words; *x != (char *) NULL; x++)
        {
            fputs(*x, nfp);
            fputc('\n', nfp);
        }
        (void) fclose(nfp);
    }
    return;
}
void irdocini(dname)
char * dname;
{
FILE * nfp;

#ifdef MINGW32
#ifdef LCC
    mkdir(dname, 0755);
#else
    mkdir(dname);
#endif
#else
    mkdir(dname, 0755);
#endif
    chdir(dname);
    if ((nfp = fopen(docind, "wb")) != (FILE *) NULL)
        (void) fclose(nfp);
    if ((nfp = fopen(docmem, "wb")) != (FILE *) NULL)
        (void) fclose(nfp);
    if ((nfp = fopen(docind, "wb")) != (FILE *) NULL)
        (void) fclose(nfp);
    if ((nfp = fopen("docidname.dir", "wb")) != (FILE *) NULL)
        (void) fclose(nfp);
    if ((nfp = fopen("docidname.pag", "wb")) != (FILE *) NULL)
        (void) fclose(nfp);
    searchterms_ini();
    return;
}
/*
 * Re-create the searchterms file from the database
 */
void refresh_searchterms()
{
char *p;
int l;
FILE * nfp;

    searchterms_ini();
    load_dict();
    if (oracle_init())
    {
        if (!setjmp(ora_fail))
        {
            curse_parse(con, &dcost, CURS_COST, "select distinct upper(name)\n\
from sysobjects\n\
union select upper(name) from syscolumns\n\
union select upper(name) from master..sysdatabases\n\
union select upper(name) from sysusers");
            exec_dml(dcost);
            dcost->so_far = 0;
            nfp = fopen(searchterms, "ab");
            while (dyn_locate(dcost,&l,&p))
                 fprintf(nfp, "%-.*s\n", l, p); 
            dyn_reset(dcost);
            (void) fclose(nfp);
            oracle_cleanup();
        }
    }
    return;
}
