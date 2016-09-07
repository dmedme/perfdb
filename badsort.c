/************************************************************************
 * badsort.c - Maintain a single homogenised list of Duff SQL statements
 *
 * The program has a number of options and capabilities.
 *
 * For all of them, it maintains a table of statements using
 *  -  The file date
 *  -  The number of Executions
 *  -  The number of Disk Reads
 *  -  The number of Buffer Gets
 *  -  The text of the statement
 *  -  The query plan.
 *
 * It is able to read and write files in a number of formats related to
 * the badplan*.lis files written by badfind.sh.
 *
 * It will also merge these files with its output files to give a single
 * consolidated list.
 *
 * It will also run as a daemon, in which case, processing is as follows.
 * To begin with, remove the communication FIFO.
 *
 * Loop forever:
 * - Attempt to log on to ORACLE as internal
 * - Attempt to log on to ORACLE as an explain user (want to improve this)
 * - If failed, sleep 10 minutes and retry.
 * - Otherwise
 *   - Create the communication FIFO.
 *   - Loop
 *     - Fetch the SQL statements
 *     - See if we already have them.
 *     - If not
 *       - We need to get a plan for them.
 *       - We store the SQL
 *     - We store the usage
 *       - Multiply it up based on the number of different possible
 *         hierarchy combinations
 *     - Write a checkpoint
 *     - Go to sleep and await events
 */
static char * sccs_id = "Copyright (c) E2 Systems Limited 1994\n\
@(#) $Name$ $Id$";
#include <sys/types.h>
#ifdef MINGW32
typedef char * caddr_t;
#endif

#ifndef MINGW32
#include <sys/socket.h>
#include <netinet/in.h>
#else
#include <WinSock2.h>
#endif
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#ifndef LINUX
extern char * strdup();      /* Not in ANSI C!?           */
#endif
#include "hashlib.h"
#include "e2file.h"
#include "siinrec.h"
#include "badsort.h"
#include "IRfiles.h"
#include "orlex.h"
#include "senlex.h"
static char buf[BUFSIZ];
static char buf1[BUFSIZ];
static char buf2[BUFSIZ];
static char buf3[BUFSIZ];
static char buf4[BUFSIZ];
static char buf5[BUFSIZ];
void find_whereabouts();
void find_noindex();
void do_one_stream();
extern struct badsort_base badsort_base;
extern double strtod();
extern double fmod();
extern double gm_secs();
/*
 * Use to sort into descending approximate CPU cost order.
 */
static int ccomp(el1,el2)
struct stat_con * el1, * el2;
{
double cost1, cost2;

    if (badsort_base.single_flag == -1)
    {
        cost1 = ((el1)->total.elapsed);
        cost2 = ((el2)->total.elapsed);
    }
    else
    {
        cost1 = ((el1)->total.disk_reads) * badsort_base.disk_buf_ratio +
                ((el1)->total.buffer_gets);
        cost2 = ((el2)->total.disk_reads) * badsort_base.disk_buf_ratio +
                ((el2)->total.buffer_gets);
        if (badsort_base.single_flag)
        {
            if ((el1)->total.executions)
                cost1 = cost1/((el1)->total.executions);
            if ((el2)->total.executions)
                cost2 = cost2/((el2)->total.executions);
        }
    }
    if (cost1 == cost2)
        return 0;
    else
    if (cost1 < cost2)
        return 1;
    else
        return -1;
}
/*****************************************************************************
 * Recognising the structure of the badsort.exp and badplan**.lis files
 */
static enum badlook_status look_status;
static enum badtok_id look_badtok;
/*
 * Configurable items
 */
static char exec_marker1[40];
static char exec_marker2[40];
static int cm_len;
static int em_len1;
static int em_len2;
static char scan_marker[40];
static char plan_marker[40];
static int pm_len;
static char stat_marker[40];
static int sm_len;
static char when_marker[40];
static char cm_marker[40];
static int wm_len;
/**********************************************************************
 * Read the next badtoken
 */
enum badtok_id get_badtok(fp)
FILE * fp;
{
char max_line[4096];
char * p;
char * cur_pos;
/*
 * If look-ahead present, return it
 */
restart:
    if (look_status == BADPRESENT)
    {
        strcpy(badsort_base.tbuf,badsort_base.tlook);
        look_status = BADCLEAR;
        if (look_badtok != BADPARTIAL)
        {
            if (look_badtok ==  BADEXECUTION)
            {
                if ( badsort_base.merge_flag)
                    badsort_base.sql_now = 1;
            }
            return look_badtok;
        }
        else
        {
            cur_pos = badsort_base.tbuf + strlen(badsort_base.tbuf);
            if (!badsort_base.merge_flag)
                goto spaghetti;
            else
            if ( !strncmp(badsort_base.tbuf,cm_marker, cm_len))
                goto mod_spag;
            else
            if (!strncmp(badsort_base.tbuf,when_marker,wm_len))
                goto when_spag;
            else
            {
                cur_pos = badsort_base.tbuf;
                (void) fgets(max_line,sizeof(max_line) -1,fp);
                goto plan_spag;
            }
        }
    }
    else
        cur_pos = badsort_base.tbuf; 
    if (badsort_base.merge_flag && badsort_base.when_flag)
    {
        if (fgets(max_line,sizeof(max_line) -1,fp) == (char *) NULL)
            return BADPEOF;
        if (!strcmp(max_line,END_HISTORY))
            badsort_base.when_flag = 0;
        else
        {
            strcpy(badsort_base.tbuf,max_line);
            return BADWHEN;
        }
    }
    while (((p = fgets(max_line,sizeof(max_line) -1,fp)) != (char *) NULL)
            && strlen(max_line) < 2); /* skip any blank lines */
/*
 * Scarper if all done
 */
    if ( p == (char *) NULL )
        return BADPEOF;
/*
 * Pick up the next badtoken.
 */
    else
    if (badsort_base.merge_flag && !strncmp(max_line,when_marker,wm_len))
    {
when_spag:
        badsort_base.when_flag = 1;
        (void) fgets(max_line,sizeof(max_line) -1,fp);
        (void) fgets(max_line,sizeof(max_line) -1,fp);
        (void) fgets(max_line,sizeof(max_line) -1,fp);
        if (fgets(max_line,sizeof(max_line) -1,fp) == (char *) NULL)
            return BADPEOF;
        else
        if (!strcmp(max_line,END_HISTORY))
        {
            badsort_base.when_flag = 0;
            goto restart;
        }
        else
        {
            strcpy(badsort_base.tbuf, max_line);
            return BADWHEN;
        }
    }
    else
    if (badsort_base.merge_flag && (!strncmp(max_line,cm_marker,cm_len)))
    {
mod_spag:
        if (fgets(max_line,sizeof(max_line) -1,fp) == (char *) NULL)
            return BADPEOF;
        else
            badsort_base.cand_flag = 1;
    }
    if (!strncmp(max_line,exec_marker2,em_len2)
     ||(!strncmp(max_line,exec_marker1,em_len1)
         && (max_line[em_len1] == ' ' || max_line[em_len1] == '\t')))
    {
        badsort_base.cand_flag = 0;
        strcpy(badsort_base.tbuf,max_line);
        if (badsort_base.merge_flag)
            badsort_base.sql_now = 1;
        return BADEXECUTION;
    }
    else
    if (badsort_base.cand_flag)
    {
        for (cur_pos = max_line + strlen(max_line) - 1;
                cur_pos >= max_line && (*cur_pos == '\n' || *cur_pos == '\r');
                      cur_pos--);
        if (cur_pos < max_line)
            goto restart;
        cur_pos++;
        *cur_pos = '\0';
        strcpy(badsort_base.tbuf,max_line);
        return BADCAND;
    }
    else
    if ((badsort_base.merge_flag
      && badsort_base.sql_now) || !strncmp(max_line,stat_marker,sm_len))
    {
    char * x;

        if (badsort_base.merge_flag)
        {
        int l;
            l = strlen(max_line);
            if ((cur_pos + l) > (badsort_base.tbuf + WORKSPACE))
            {
                fputs("Token too big: File format error\n", 
                       perf_global.errout);
                return BADPEOF;
            } 
            strcpy(cur_pos, max_line);
            cur_pos += l;
        }
spaghetti:
        while (fgets(max_line,sizeof(max_line) -1,fp) != (char *) NULL)
        {
            if (badsort_base.merge_flag)
            {
                if (!strncmp(max_line,when_marker, wm_len)
                  || !strncmp(max_line,plan_marker, pm_len)
                  || !strncmp(max_line,cm_marker, cm_len))
                {
                    look_status = BADPRESENT;
                    look_badtok = BADPARTIAL;
                    strcpy(badsort_base.tlook,max_line);
                    badsort_base.sql_now = 0;
                    break;
                }
                else
                if (!strncmp(max_line,exec_marker2, em_len2))
                {
                    look_status = BADPRESENT;
                    strcpy(badsort_base.tlook,max_line);
                    look_badtok = BADEXECUTION;
                    badsort_base.sql_now = 0;
                    break;
                }
            }
            x = &max_line[badsort_base.indent];
            if (*x == ';')
            {
                badsort_base.sql_now = 0;
                break;
            }
            if (badsort_base.pl_len != 0
             && !strncmp(badsort_base.prompt_label,max_line,
                        badsort_base.pl_len))
            {
                if (!strncmp(max_line,exec_marker1, em_len1)
                    && (max_line[em_len1] == ' ' || max_line[em_len1] == '\t'))
                {
                    look_status = BADPRESENT;
                    strcpy(badsort_base.tlook,max_line);
                    look_badtok = BADEXECUTION;
                }
                else
                if (!strncmp(max_line,stat_marker, sm_len))
                {    /* Should never happen? */
                    look_status = BADPRESENT;
                    look_badtok = BADPARTIAL;
                }
                break;
                }
                while (*x != '\0')
                {
                    if (cur_pos >= (badsort_base.tbuf + WORKSPACE))
                    {
                        fputs("Token too big: File format error\n", 
                                    perf_global.errout);
                        return BADPEOF;
                    } 
                    if (*x == ';')
                        badsort_base.sql_now = 0;
                    *cur_pos++ = *x++;
                }
            }
            if (!strncmp(badsort_base.tbuf,SQL_EXPLAIN,12))
                badsort_base.tbuf[12] = '\0';
                                      /* Lump all the explains together */
            for (cur_pos--;
                   *cur_pos == ' ' || *cur_pos == '\n' /* || *cur_pos == '/' */
                     || *cur_pos == ';' || *cur_pos == '\t';
                        cur_pos--);     /* Strip trailing white space etc. */
            cur_pos++;
            *cur_pos = '\0';
            return BADSQL;
        }
        else      
        if (!strncmp(max_line,plan_marker,pm_len))
        {
        char * x;

        (void) fgets(max_line,sizeof(max_line) -1,fp);
plan_spag:
        while ((p = fgets(max_line,sizeof(max_line) -1,fp))
                        != (char *) NULL)
        {
            x = &max_line[0];
            if ((!badsort_base.merge_flag
              && (!strncmp(badsort_base.prompt_label,x, 
                                badsort_base.pl_len)
              || strlen(max_line) <= badsort_base.pl_len))
              || (badsort_base.merge_flag &&
#ifdef SYBASE
                      (!strncmp(x,when_marker,wm_len)
                    || !strncmp(x,cm_marker,cm_len)
                    || !strncmp(x,exec_marker2,em_len2))
#else
                      *x == '\n'
#endif
                 ))
                 break;
             while (*x != '\0')
             {
                 if (cur_pos >= (badsort_base.tbuf + WORKSPACE))
                 {
                     fputs("Token too big: File format error\n", 
                             perf_global.errout);
                     return BADPEOF;
                 } 
                 *cur_pos++ = *x++;
             }
        }
        for (cur_pos--;
                *cur_pos == ' ' || *cur_pos == '\n' /* || *cur_pos == '/' */
                  || *cur_pos == ';' || *cur_pos == '\t';
                     cur_pos--);     /* Strip trailing white space etc. */
        cur_pos++;
        *cur_pos = '\0';
        if ((!strncmp(max_line, exec_marker1, em_len1)
             && (max_line[em_len1] == ' ' || max_line[em_len1] == '\t'))
          ||  (badsort_base.merge_flag
            && !strncmp(max_line,exec_marker2,em_len2)))
        {
            look_status = BADPRESENT;
            strcpy(badsort_base.tlook,max_line);
            look_badtok = BADEXECUTION;
        }
        else
        if (!strncmp(max_line,stat_marker, sm_len)
           || (badsort_base.merge_flag
             && ( !strncmp(max_line,when_marker,wm_len)
               || !strncmp(max_line,cm_marker,cm_len))))
        {
            look_status = BADPRESENT;
            look_badtok = BADPARTIAL;
            strcpy(badsort_base.tlook,max_line);
            badsort_base.sql_now = 0;
        }
        return BADPLAN;
    }
    else
        goto restart;
}
/*
 * Check if a statement falls in the right time period
 */
int restrict_date(s)
struct stat_con * s;
{
struct when_seen * w;

    for (w = s->anchor; w != (struct when_seen *) NULL; w = w->next)
    {
        if (badsort_base.start_time < 86400.0)
        {
            if ((fmod(((double) (w->t)),86400.0) >= badsort_base.start_time)
             && (fmod(((double) (w->t)),86400.0) <= badsort_base.end_time))
                return 1;
        }
        else
        {
            if ((((double) (w->t)) >= badsort_base.start_time)
             && (((double) (w->t)) <= badsort_base.end_time))
                return 1;
        }
    }
    return 0;
}
/*
 * Check if a statement matches the right pattern
 */
int restrict_pattern(s)
struct stat_con * s;
{
    return (re_exec(s->sql, badsort_base.qual_compiled) >0);
}
/*
 * Clean up response times skewed by server session termination.
 */
int restrict_rubbish(s)
struct stat_con * s;
{
struct when_seen * w;

    s->total.buffer_gets = 0.0;
    for (w = s->anchor; w != (struct when_seen *) NULL; w = w->next)
    {
        if (w->executions > 0)
        {
            if (w->buffer_gets > 1200.0 && w->buffer_gets/w->executions > 0.1)
            {
                s->total.executions -= w->executions;
                w->executions = 0;
                w->buffer_gets = 0.0;
            }
        }
        s->total.buffer_gets += w->buffer_gets;
    }
    return 1;
}
/*
 * Output the by-interval figures for cost
 *
 * The list is in reverse date order, so we recurse to the bottom of the
 * list
 */
void when_print(fp, s)
FILE *fp;
struct when_seen * s;
{
char *x;

    if (s->next != (struct when_seen *) NULL)
        when_print(fp, s->next);
    if ((s->disk_reads != 0.0 || s->buffer_gets != 0.0)
     && ((badsort_base.restrct != restrict_date)
       ||( badsort_base.start_time < 86400.0
         && (fmod((((double) (s->t))),86400.0) >= badsort_base.start_time)
         && (fmod(((double) (s->t)),86400.0) <= badsort_base.end_time))
       ||( ((((double) (s->t)) >= badsort_base.start_time)
         && (((double) (s->t)) <= badsort_base.end_time)))))
    {
        x = ctime(&(s->t));
#ifdef OR9
        fprintf(fp, WHEN_FORMAT,
            x+8, x+4, x+20, x+11, s->executions, s->disk_reads, s->buffer_gets,
             s->cpu, s->elapsed, s->direct_writes,
             s->application_wait_time,
                 s->concurrency_wait_time, s->cluster_wait_time,
                 s->user_io_wait_time, s->plsql_exec_time,
                 s->java_exec_time, s->diff_time);
#else
        fprintf(fp, WHEN_FORMAT,
            x+8, x+4, x+20, x+11, s->executions, s->disk_reads, s->buffer_gets,
             s->diff_time);
#endif
        if (badsort_base.debug_level > 3)
            fflush(fp);
    }
    return;
}
/*
 * Export a single plan
 */
void plan_print(fp, p)
FILE * fp;
struct plan_con * p;
{
    if (p != (struct plan_con *) NULL && p->plan != (char *) NULL)
    {
        fputs(EXEC_PLAN, fp);
        fputc('\n', fp);
        fputs(EXEC_UNDERLINE, fp);
        fputc('\n', fp);
        fputs(p->plan, fp);
        fputs("\n\n", fp);
    }
    return;
}
/*
 * Export a single statement, plan and their activity history and candidates
 */
void stat_print(fp, s)
FILE * fp;
struct stat_con * s;
{
struct prog_con * pc;
int i;

#ifdef OR9
    fprintf(fp, EXEC_FORMAT,
       s->total.executions,
       s->total.disk_reads,
       s->total.buffer_gets,
       s->total.cpu,
       s->total.elapsed,
       s->total.direct_writes,
       s->total.application_wait_time,
       s->total.concurrency_wait_time, s->total.cluster_wait_time,
       s->total.user_io_wait_time, s->total.plsql_exec_time,
       s->total.java_exec_time);
#else
    fprintf(fp, EXEC_FORMAT,
       s->total.executions,
       s->total.disk_reads,
       s->total.buffer_gets);
#endif
#ifdef DEBUG
    if (s->total.executions > 100000000.0)
    {
       fputs((char *) s, fp);
       fputc('\n', fp);
    }
#endif
    if (s->sql != (char *) NULL)
    {
        fputs(s->sql, fp);
        fputc('\n', fp);
        if (badsort_base.debug_level > 3)
            fflush(fp);
    }
    if (s->plan != (struct plan_con *) NULL)
    {
        plan_print(fp, s->plan);
        for (i = 0; i < s->other_plan_cnt; i++)
            plan_print(fp, s->other_plan[i]);
        if (badsort_base.debug_level > 3)
            fflush(fp);
    }
    if (s->anchor != (struct when_seen *) NULL)
    {
        fputs(ACTIVITY_HISTORY, fp);
        fputc('\n', fp);
        fputs("----------------\n", fp);
#ifdef SYBASE
 fputs("When                 Executions    Reserved     Response  Interval\n",
            fp);
#else
#ifdef OR9
 fputs("When                 Executions  Disk Reads  Buffer Gets  CPU Time         Elapsed          Direct Writes    App. Wait Time   Con. Wait Time   Clu. Wait Time   U IO Wait Time   PLSQL Exe Time   Java Exec Time   Interval\n",
            fp);
#else
 fputs("When                 Executions  Disk Reads  Buffer Gets  Interval\n",
            fp);
#endif
#endif
#ifdef OR9
 fputs("-------------------  ----------  ----------  -----------  --------------   --------------   --------------   --------------   --------------   --------------   --------------   --------------   --------------   --------\n",
            fp);
#else
 fputs("-------------------  ----------  ----------  -----------  --------\n",
            fp);
#endif
        when_print(fp, s->anchor);
        fputs(END_HISTORY, fp);
        fputc('\n', fp);
        if (badsort_base.debug_level > 3)
            fflush(fp);
    }
    else
        fputc('\n', fp);
    pc = s->cand_prog;
    if (pc != (struct prog_con *) NULL)
    {
        fputs(cm_marker,fp);
        fputc('\n',fp);
    }
    for (; pc != (struct prog_con *) NULL; pc = pc->next)
        prog_print(fp,pc);
    return;
}
/*
 * Recalculate the totals for a single statement, excluding items outside the
 * time range.
 */
void stat_retotal(s)
struct stat_con * s;
{
struct when_seen * w;
    
    s->total.executions = 0.0;
    s->total.disk_reads = 0.0;
    s->total.buffer_gets = 0.0;
    s->total.cpu = 0.0;
    s->total.elapsed = 0.0;
    s->total.direct_writes = 0.0;
    s->total.application_wait_time = 0.0;
    s->total.concurrency_wait_time = 0.0;
    s->total.cluster_wait_time = 0.0;
    s->total.user_io_wait_time = 0.0;
    s->total.plsql_exec_time = 0.0;
    s->total.java_exec_time = 0.0;
    for (w = s->anchor; w != (struct when_seen *) NULL; w = w->next)
    {
        if (badsort_base.start_time < 86400.0)
        {
            if ((fmod(((double) (w->t)),86400.0) >= badsort_base.start_time)
             && (fmod(((double) (w->t)),86400.0) <= badsort_base.end_time))
            {
                s->total.executions += w->executions;
                s->total.disk_reads += w->disk_reads;
                s->total.buffer_gets += w->buffer_gets;
#ifdef OR9
                s->total.cpu += w->cpu;
                s->total.elapsed += w->elapsed;
                s->total.direct_writes = w->direct_writes;
                s->total.application_wait_time = w->application_wait_time;
                s->total.concurrency_wait_time = w->concurrency_wait_time;
                s->total.cluster_wait_time = w->cluster_wait_time;
                s->total.user_io_wait_time = w->user_io_wait_time;
                s->total.plsql_exec_time = w->plsql_exec_time;
                s->total.java_exec_time = w->java_exec_time;
#endif
            }
        }
        else
        {
            if ((((double) (w->t)) >= badsort_base.start_time)
             && (((double) (w->t)) <= badsort_base.end_time))
            {
                s->total.executions += w->executions;
                s->total.disk_reads += w->disk_reads;
                s->total.buffer_gets += w->buffer_gets;
#ifdef OR9
                s->total.cpu += w->cpu;
                s->total.elapsed += w->elapsed;
                s->total.direct_writes = w->direct_writes;
                s->total.application_wait_time = w->application_wait_time;
                s->total.concurrency_wait_time = w->concurrency_wait_time;
                s->total.cluster_wait_time = w->cluster_wait_time;
                s->total.user_io_wait_time = w->user_io_wait_time;
                s->total.plsql_exec_time = w->plsql_exec_time;
                s->total.java_exec_time = w->java_exec_time;
#endif
            }
        }
    }
    return;
}
/***********************************************************************
 * Step through the plan, finding the applicable tables and indexes
 * -  Find the length of the plan
 * -  Loop - Line at a time, until all lines done
 *    -   Loop - word at a time, until end of line
 *        -  If the word is index or table, set the appropriate type
 *        -  If we have not yet seen index or table, do nothing
 *        -  Else if the word is one of the stop words, do nothing
 *        -  Else write out to the file, and go to the next line.
 */
void do_ind_tab(plan,atable,aindex, plan_id)
char * plan;
FILE * atable;
FILE * aindex;
int plan_id;
{
FILE * out_file;
int wlen;
char *w, *ew;
char *l;
char **cw;
static char * stopwords[] = {"ACCESS",
"FULL",
"RANGE",
"SCAN",
"UNIQUE",
"BY",
"ROWID",
"" }; 
int tab_flag;
     
    l = plan;
    w = l;
/*
 * Loop - for all lines in the plan
 */
    while (*l != '\0')
    {
        while (*l != '\n' && *l != '\0')
            l++;                   /* Position to the end of the line */
        out_file = (FILE *) NULL;  /* Clear the output file           */
        tab_flag = 0;
/*
 * Loop - for all words on the line
 */
        while (w < l)
        {
            while (w < l && (*w == ' ' || *w == '\t'))
                w++;               /* Position to the first character */ 
            if (w >= l)
                break;
            ew = w + 1;
            while (ew < l && *ew != ' ' && *ew != '\t')
                ew++;              /* Position to the first character */ 
            wlen = ew - w;
            if (tab_flag == 0)
            {
                if (!strncmp(w,"TABLE",wlen))
                {
                    tab_flag = 1;
                    out_file = (FILE *) atable;
                }
                else
                if (!strncmp(w,"INDEX",wlen))
                {
                    tab_flag = 2;
                    out_file = (FILE *) aindex;
                }
                else
                    break;        /* INDEX or TABLE will be the first word */
            }
            else
            {
                for (cw = &stopwords[0]; **cw != '\0'; cw++)
                    if (!strncmp(*cw,w,wlen))
                        break;     /* Is this a stopword?            */
                if (**cw == '\0')
                {                  /* We have a table or index       */
                    if (out_file != (FILE *) NULL)
                        fprintf(out_file, "%u}%*.*s\r\n",
                           (unsigned long) plan, wlen,wlen,w); 
                    if (plan_id)
                    {
                        if (tab_flag == 1)
                            do_oracle_table(plan_id,wlen,w);
                        else
                            do_oracle_index(plan_id,wlen,w);
                    }
                    break;         /* Go to the next line            */
                }
            }
            w = ew + 1;     /* Advance to look for the next word     */
        }
        w = l + 1;          /* Advance to look at the next line      */ 
        if (*l != '\0')
            l++;
    } 
    return;
}
/***************************************************************************
 * Main Program Starts Here
 * VVVVVVVVVVVVVVVVVVVVVVVV
 */
#ifdef SYBASE
#define main badsort_main
#endif
int main(argc, argv)
int argc;
char ** argv;
{
int i;
/*
 * The following steps deal with possible restarts
 */
#ifndef MINGW32
    while(wait(0) != -1);     /* Reap any zombie children */
#endif
    for (i = 3; i < 256; i++)
        (void) close(i);      /* Make sure that the file descriptors go away */
/*
 * Set up some static values
 */
    em_len2 = sprintf(exec_marker2,"%17.17s",EXEC_FORMAT);
    cm_len = sprintf(cm_marker,"%17.17s",CANDIDATES);
    wm_len = sprintf(when_marker,"%14.14s",ACTIVITY_HISTORY);
    pm_len = sprintf(plan_marker,"%s",EXEC_PLAN);
    perf_global.errout   = stderr;
#ifdef MINGW32
    badsort_base.fifo_name = "\\\\.\\pipe\\badsort_fifo";
#else
    badsort_base.fifo_name = "badsort_fifo";
#endif
    perf_global.fifo_name = badsort_base.fifo_name;
    badsort_base.stat_tab = hash(1500000,string_hh,strcmp);
    badsort_base.plan_tab = hash(1500000,string_hh,strcmp);
    for (i = 2; i < 150000; i = (i << 1));
    badsort_base.tabsize = i;
    badsort_base.out_name = "badsort.exp";
    badsort_base.debug_level = 0;
    
    badsort_base.tbuf =  (char *) malloc(WORKSPACE);
    badsort_base.tlook = (char *) malloc(WORKSPACE);
    badsort_base.merge_flag = 0;   /* Default behaviour is badplan format  */
    badsort_base.single_flag = 0;  /* Default behaviour is all format      */
    badsort_base.prompt_label = "SQL> ";
    badsort_base.pl_len = 5;
    badsort_base.indent = 5;
    badsort_base.retry_time = 1200;
    badsort_base.disk_buf_ratio = 20.0; 
                              /* Default is File System, with UNIX buffers */
                              /* 50 is probably appropriate for RAW access */
#ifdef OR9
    badsort_base.buf_cost = 1000000.0;  /* Units of CPU microseconds */ 
#else
    badsort_base.buf_cost = 0.000023; 
                              /* Default based on HP K460                  */
#endif
    ir_ini();
                              /* Read in the search terms                  */
    for (i = 1; i < argc; i++)
    {
        if (*argv[i] == '-')
        {
            optind = i;
            proc_args(argc,argv);
            i = optind - 1;
        }
/*
 * The files must be presented in date order if they have no usage history
 */
        else
            do_one_file(argv[i]);
    }
    if (badsort_base.access_flag)
        do_access_exp();
    else
    {
#ifndef SYBASE
        if (badsort_base.uid_explain != (char *) NULL
         && badsort_base.uid == (char *) NULL)
            do_all_plans();
#endif
        do_export(badsort_base.out_name);
    }
#ifdef SYBASE
    optind = argc;
    return 1;
#endif
    exit(0);
}
/*
 * Process a set of arguments
 */
void proc_args( argc, argv)
int argc;
char ** argv;
{
FILE * errout_save = perf_global.errout;
int ch;
int i;
double d;
int quit_flag = 0;
static char * hlp = "Provide an optional indent size (-i) and prompt (-p)\n\
and a list of badplan files in date order, or -m and badsort output files\n\
or -M and Microsoft Profiler output files\n\
the desired sort weighting (-r) the output file name (-o) and/or single cost\n\
weighting (-s) or elapsed time order (-R)\n\
-A and directory name to initialise the badsort operating system files\n\
-B and database sign-on to refresh the search terms from the target schema\n\
-C and database sign-on to create results tables in a database\n\
-D to delete bogus response times\n\
-E to select statements matching the given regular expression\n\
-N to use a different name for the FIFO, (eg. Windows - \\\\.\\pipe\\badsort_fifoSID)\n\
-u and database sign-on to store results in a database\n\
-f and database sign-on to import results to the database and exit\n\
-n and database sign-on to explain with a different database connection\n\
-G and comma-separated optimiser goals settings to find alternate plans \n\
-q and database sign-on to search for interesting (expensive) SQL\n\
-t and interval in seconds to set the periodicity (default 1200 seconds)\n\
-g and date/time format to indicate the date range selection format\n\
-y start date/time selection. A time alone picks a time range every day.\n\
-z end date/time selection. A time alone picks a time range every day.\n\
-a to format the statements for later import to eg. Microsoft Access\n\
-b to dump out the index\n\
-c and notional per ORACLE 'buffer get' CPU cost\n\
-d and debug level (4 gives maximum verbosity)\n\
-h to output this message\n\
-Q (via the FIFO) to exit\n\
-v search for all statements in the given list of files, not via the index\n\
-w as for -v, but unidentified statements only\n\
-x and directory name to index; NULL to index the database stored procedures\n\
-j to hunt for the contents of a passed file via the index\n\
-k to hunt for unlocated SQL statements via the index\n\
-l to hunt for all our SQL statements afresh via the index\n";

    if (badsort_base.debug_level > 3)
    {
        fputs("proc_args()\n",perf_global.errout);
        dump_args(argc,argv);
        fflush(perf_global.errout);
    }

    while ((ch = getopt(argc, argv,
                       "1abc:d:e:f:g:hi:j:klmn:o:p:q:r:st:u:vwx:y:z:A:B:C:DE:G:N:QM:R"))
                 != EOF)
    {
        switch (ch)
        {
        char * x;
        case '1':
#ifndef SYBASE
            do_one_pass();                  /* Request a SQL process cycle    */
#endif
            break;
        case 'a':
            badsort_base.access_flag = 1;   /* Access-compatible output files */
            break;
        case 'b':
            inddump(docind);                /* Dump out the index */
            break;
        case 'c':
            d = strtod(optarg, (char **) NULL);
            if (d > 0.0)
                badsort_base.buf_cost = d;  /* Set the Buffer Get CPU Cost    */
            break;
        case 'd':
            badsort_base.debug_level = atoi(optarg);
            break;
        case 'e':
/*
 * Change the disposition of error output. Used with e2sub for remote control.
 */
            sighold(SIGALRM);
            sigset(SIGPIPE, restart);
#ifdef SOLAR
            if ((perf_global.errout =
                fdopen(perf_global.fifo_fd,"w")) == (FILE *) NULL)
            {
                perror("Response Connect() failed");
                perf_global.errout = errout_save;
            }
            else
                setbuf(perf_global.errout,NULL);
#else
#ifdef MINGW32
            if (badsort_base.debug_level > 3)
            {
                fputs("About to re-direct error output to client\n",
                     perf_global.errout);
                fflush(perf_global.errout);
            }
            if ((perf_global.errout =
                fdopen(perf_global.fifo_fd,"wb")) == (FILE *) NULL)
            {
                perror("Response Connect() failed");
                perf_global.errout = errout_save;
                fputs("Failed to re-direct error output to client\n",
                     perf_global.errout);
                fflush(perf_global.errout);
            }
            else
                setbuf(perf_global.errout,NULL);
            if (badsort_base.debug_level > 3)
            {
                fputs("After re-direction of error output to client\n",
                     perf_global.errout);
                fflush(perf_global.errout);
            }
#else
            if ((perf_global.errout = fopen(optarg,"wb")) == (FILE *) NULL)
            {
                perror("Response FIFO open failed");
                perf_global.errout = errout_save;
            }
            else
                setbuf(perf_global.errout,NULL);
#endif
#endif
            sigrelse(SIGALRM);
            break;
        case 'N':
            if (strlen(optarg))
                badsort_base.fifo_name = optarg;
            break;
        case 'A':
            if (strlen(optarg))
                irdocini(optarg);
            break;
/*
 * Options that require an ORACLE user/password
 */
        case 'B':
            if (strlen(optarg))
            {
                badsort_base.uid = strdup(optarg);
                for (x = optarg; *x != '\0'; *x++ = '\0');
                      /* Rub out the userid/passwords on the command line */
            }
            refresh_searchterms();
            break;
        case 'C':
            if (strlen(optarg))
            {
                badsort_base.uid_stats = strdup(optarg);
                for (x = optarg; *x != '\0'; *x++ = '\0');
                      /* Rub out the userid/passwords on the command line */
            }
            do_oracle_tables();
            break;
        case 'f':
            if (strlen(optarg))
            {
                badsort_base.uid_stats = strdup(optarg);
                for (x = optarg; *x != '\0'; *x++ = '\0');
                      /* Rub out the userid/passwords on the command line */
            }
            do_oracle_imp();
            break;
        case 'G':
            badsort_base.optimiser_goals = optarg;
            break;
        case 'n':
            badsort_base.uid_explain = strdup(optarg);
            for (x = optarg; *x != '\0'; *x++ = '\0');
                      /* Rub out the userid/passwords on the command line */
            break;
        case 'q':
            badsort_base.uid = strdup(optarg);
            for (x = optarg; *x != '\0'; *x++ = '\0');
                      /* Rub out the userid/passwords on the command line */
            run_daemon();
            break;
        case 'u':
            if (strlen(optarg))
            {
                badsort_base.uid_stats = strdup(optarg);
                for (x = optarg; *x != '\0'; *x++ = '\0');
                      /* Rub out the userid/passwords on the command line */
            }
            break;
/*
 * Non-indexed search options
 */
        case 'v':
        case 'w':
            if (badsort_base.scan_con == (struct scan_con *) NULL)
                badsort_base.scan_con = scan_setup(searchterms);
            else
                scan_reset(badsort_base.scan_con);
            if (badsort_base.scan_con == (struct scan_con *) NULL)
            {
                fputs("You must initialise the search terms first\n", 
                             perf_global.errout);
                break;
            }
            find_noindex(ch, argc, argv);
            break;
/*
 * Indexing set-up and search options
 */
        case 'j':
        case 'k':
        case 'l':
        case 'x':
            if (badsort_base.debug_level > 3)
            {
                switch(ch)
                {
                     case 'l':
                         fputs("Searching from scratch for candidate modules\n",
                                 perf_global.errout);
                         break;
                     case 'k':
                         fputs("Searching for missing candidate modules\n",
                                 perf_global.errout);
                         break;
                     case 'j':
                         fprintf(perf_global.errout,
                               "Searching for candidate modules for %s\n",
                                optarg);
                         break;
                     case 'x':
                         fprintf(perf_global.errout, "Indexing %s\n", optarg);
                     default:
                         break;
                }
                fflush(perf_global.errout);
            }
            sighold(SIGALRM);
            if (badsort_base.scan_con == (struct scan_con *) NULL)
                badsort_base.scan_con = scan_setup(searchterms);
            else
                scan_reset(badsort_base.scan_con);
            if (badsort_base.scan_con == (struct scan_con *) NULL)
            {
                fputs("You must initialise the search terms first\n", 
                             perf_global.errout);
                break;
            }
            if (ch == 'l' || ch == 'k')
                find_whereabouts(ch);         /* Hunt through the indexes */
            else
            if (ch == 'x')
            {
                if (*optarg != '/' && *(optarg +1) != ':')
                    do_some_indexing(badsort_base.scan_con, (char *) NULL,
                                     badsort_base.in_con);
                else
                    do_some_indexing(badsort_base.scan_con, optarg,
                                    (struct sess_con *) NULL);
            }
            else /* (ch == 'j') */
                do_sql_search(optarg, perf_global.errout, (struct prog_con *)
                              NULL);
            sigrelse(SIGALRM);
            break;
        case 'r':
            d = strtod(optarg, (char **) NULL);
            if (d >= 1.0)
                badsort_base.disk_buf_ratio = d;
                               /* Set the disk read/buffer get CPU cost ratio */
            break;
        case 's':
            badsort_base.single_flag = 1;    /* Single Statement Output Order */
            break;
        case 'R':
            badsort_base.single_flag = -1;   /* Response Time Order */
            break;
        case 't':
            badsort_base.retry_time = atoi(optarg);
                                             /* Periodicity               */
            if (badsort_base.retry_time < 1)
                badsort_base.retry_time = 1200;
            break;
        case 'm':
            badsort_base.merge_flag = 1;     /* Merging output files      */
            badsort_base.indent = 0;
            badsort_base.pl_len = 0;
            break;
        case 'M':
            badsort_base.merge_flag = 1;     /* Merging output files      */
            badsort_base.indent = 0;
            badsort_base.pl_len = 0;
            do_one_stream(optarg);
            break;
        case 'o':
            badsort_base.out_name = optarg;
            break;
        case 'i':
            i = atoi(optarg);
            if (i >= 0)
                badsort_base.indent = i;
            badsort_base.merge_flag = 0;     /* Not merging output files      */
            break;
        case 'p':
            badsort_base.prompt_label = optarg;
            badsort_base.pl_len = strlen(badsort_base.prompt_label);
            badsort_base.merge_flag = 0;     /* Not merging output files      */
            break;
        case 'D' :
            badsort_base.restrct = restrict_rubbish;
            break;
        case 'E' :
        {
        long int buf_size;
        long int exp_size;

            badsort_base.restrct = restrict_pattern;
            badsort_base.qual_pattern = optarg;
/*
 * Attempt to compile the regular expression
 */
            for (buf_size=ESTARTSIZE;;)
            {
                if ((badsort_base.qual_compiled =
                    (reg_comp) malloc(buf_size))==NULL)
                {
                    fprintf(stderr,
                       "Failed to allocate %d for compilation of %s\n",
                          buf_size, optarg);
                    exit(1);
                }
                exp_size = (long) re_comp(optarg, badsort_base.qual_compiled,
                                buf_size);
                if (exp_size != (buf_size + 1))
                    break;
                buf_size = (buf_size << 1);
                free(badsort_base.qual_compiled);
            }
        }
            break;
/*
 * Date selection options
 */
        case 'g':
            badsort_base.date_format = optarg;
            break;
        case 'y' :
        case 'z' :
            badsort_base.restrct = restrict_date;
            if ( badsort_base.date_format != (char *) NULL)
            {
                if ( !date_val(optarg,badsort_base.date_format,&x,&d))
/*
 * Time argument is not a valid date
 */
                {
                    (void) fputs(hlp, perf_global.errout);
                    break;
                }
            }
            else
                d = strtod(optarg, (char **) NULL);
            d = gm_secs((time_t) d);
            if (ch == 'y')
                badsort_base.start_time = d;
            else
                badsort_base.end_time = d;
            break;
        case 'Q':
            quit_flag = 1;
            break;
        case 'h':
        default:
            fputs( hlp, perf_global.errout);
            return;
        }
    }
/*
 * Restore the error file pointer
 */
    if (perf_global.errout != errout_save)
    {
        fputs("\nOK\n", perf_global.errout);   /* Outcome string */
        (void) fclose(perf_global.errout);
        perf_global.errout = errout_save;
        if (badsort_base.debug_level > 3)
        {
            fputs("Restored normal error output handling\n",
                     perf_global.errout);
            fflush(perf_global.errout);
        }
    }
    if (quit_flag)
        finish();
    return;
}
void clear_whens(pc)
struct when_seen * pc;
{
struct when_seen * ppc;

    while (pc != (struct when_seen *) NULL)
    {
        ppc = pc;
        pc = pc->next;
        free(ppc);
    }
    return;
}
/*
 * Search for all SQL (l) or hitherto unknown SQL (k) using the index
 */
void find_whereabouts(ch)
int ch;
{
struct stat_con * statp;
char * fname;
FILE * sqlf;

    for (statp = badsort_base.anchor;
            statp != (struct stat_con *) NULL;
                statp = statp->next)
    {
        if (statp->sql == (char *) NULL || *(statp->sql) == '\0')
            continue;
        if (ch == 'k' && statp->cand_prog != (struct prog_con *) NULL)
            continue;
#ifdef SYBASE
        if (!strncasecmp("open ",statp->sql,5)
         || !strncasecmp("fetch ",statp->sql,6)
         || !strncasecmp("close ",statp->sql,6))
            continue;
#endif
        if (statp->sql_id != 0)
            fname = filename("sql%u", statp->sql_id);
        else
            fname = filename("sql%u", (unsigned long) statp);
        if ((sqlf = fopen(fname,"wb")) != (FILE *) NULL)
        {
            setbuf(sqlf, NULL);
            if (fwrite(statp->sql, sizeof(char), strlen(statp->sql), sqlf)< 1)
                perror("fwrite() failed");
            fclose(sqlf);
            if (statp->cand_prog != (struct prog_con *) NULL)
                clear_progs(statp->cand_prog);
            statp->cand_prog = (struct prog_con *) NULL;
            scan_reset(badsort_base.scan_con);
            do_sql_search(fname,perf_global.errout, &(statp->cand_prog));
            do_oracle_mapping(statp);
            unlink(fname);
        }
        else
            perror("SQL file open failed");
        free(fname);
    }
}
/*******************************************************************************
 * Allocate a prog_con structure
 */
struct prog_con * prog_con_name(canchor, fname)
struct prog_con * canchor;
char * fname;
{
struct prog_con * nc;

    if (canchor == (struct prog_con *) NULL || strcmp(canchor->name, fname))
    {                   /* Ignore duplicates */
        nc = canchor;
        canchor = (struct prog_con *) malloc(sizeof(struct prog_con));
        canchor->next = nc;
        canchor->name = strdup(fname);
        canchor->module_id = (int) (canchor->name);
    }
    return canchor;
}
/*
 * Search for all SQL (v) or hitherto unknown SQL (w) without the index
 */
void find_noindex(ch, argc, argv)
int ch;
int argc;
char ** argv;
{
struct stat_con * statp, **sortp, **wpp;
int search_cnt;
int **sent_track;
int *sent_base;
int *sent_curr;
struct open_results * doc_object;
struct word_results * word_object;
struct sen_results * sen_object;
struct sen_scan_con * sscp;
char * x;
int i;
/*
 * Allocate space for a list of SQL statement structure pointers.
 */
    if ((sortp = (struct stat_con **) malloc (badsort_base.stat_cnt *
           sizeof ( struct stat_con *))) == (struct stat_con **) NULL)
    {
        perror("Failed to allocate space for statement list");
        return;
    }
/*
 * Turn the statement texts into list of word indices.
 *
 * Use the storage pointed to by badsort_base.tlook and badsort_base.tbuf
 * to build up the statement pointers
 */
    for (search_cnt = 0,
         wpp = sortp,
         sent_track = (int **) badsort_base.tbuf,
         sent_base = &(((int *) badsort_base.tlook)[0]),
         sent_curr =  &(((int *) badsort_base.tlook)[1]),
         statp = badsort_base.anchor;
            statp != (struct stat_con *) NULL;
                statp = statp->next)
    {
        if (statp->sql == (char *) NULL || *(statp->sql) == '\0')
            continue;
        if (ch == 'w' && statp->cand_prog != (struct prog_con *) NULL)
            continue;
        else
        if (statp->cand_prog != (struct prog_con *) NULL)
            clear_progs(statp->cand_prog);
        statp->cand_prog = (struct prog_con *) NULL;
        search_cnt++;
        *wpp++ = statp;
        scan_reset(badsort_base.scan_con);
#ifdef DEBUG_FIND
        fputs(statp->sql, perf_global.errout);
        fputc('\n', perf_global.errout);
#endif
        for (x = statp->sql; *x != '\0'; x++)
        {
#ifdef DEBUG_FIND
            fputc(*x, perf_global.errout);
#endif
            if ((word_object = get_mem_word(*x))
                      != (struct word_results *) NULL)
            {
#ifdef DEBUG_FIND
                fprintf(perf_global.errout,"%d %*.*s\n",word_object->match_ind,
                                     word_object->word_length,
                                     word_object->word_length,
                                     word_object->word_ptr);
                fflush(perf_global.errout);
#endif
                *sent_curr = word_object->match_ind;
                sent_curr++;
                if (sent_curr >= ((int *) &badsort_base.tlook[WORKSPACE]))
                {
                    fprintf(perf_global.errout, "Too many words: limit is %d\n",
                                    WORKSPACE/sizeof(int));
                    exit(1);
                }
            }
        }
        while ((word_object = get_mem_word(0)) != (struct word_results *) NULL)
        {
#ifdef DEBUG_FIND
            fprintf(perf_global.errout,"%d %*.*s\n",word_object->match_ind,
                                     word_object->word_length,
                                     word_object->word_length,
                                     word_object->word_ptr);
            fflush(perf_global.errout);
#endif
            *sent_curr = word_object->match_ind;
            sent_curr++;
            if (sent_curr >= ((int *) &badsort_base.tlook[WORKSPACE]))
            {
                fprintf(perf_global.errout, "Too many words: limit is %d\n", 
                                WORKSPACE/sizeof(int));
                exit(1);
            }
        }
        *sent_track = sent_base;
        *sent_base = sent_curr - sent_base - 1;
        sent_track++;
        sent_base = sent_curr;
        sent_curr++;
    }
/*
 * Compile the sentence scanner
 */
    if ((sscp = sen_scan_setup(badsort_base.scan_con->words_seen,
                               search_cnt, (int *) (badsort_base.tbuf)))
                                == (struct sen_scan_con *) NULL)
    {
        fputs("Failed to compile sentence scanner\n", perf_global.errout);
        exit(1);
    }
/*
 * Now scan the input files, until we run out or encounter a command option
 */
    for ( ; optind < argc && argv[optind][0] != '-';optind++)
    {
        if ((doc_object = openbyname(argv[optind]))
                               == (struct open_results *) NULL
          || doc_object->doc_fd < 0)
        {
            perror("openbyname()");
            fprintf(perf_global.errout,
                       "Failed to open search target file %s\n",
                                                        argv[optind]);
            continue;
        }
#ifdef DEBUG_FIND
        fputs(argv[optind], perf_global.errout);
        fputc('\n', perf_global.errout);
        fflush(perf_global.errout);
#endif
        scan_reset(badsort_base.scan_con);
        sen_scan_reset(sscp);
        while ((word_object = (*doc_object->word_func)(doc_object))
                      != (struct word_results *) NULL)
        {
#ifdef DEBUG_FIND
            fprintf(perf_global.errout,"%d %*.*s\n",word_object->match_ind,
                                     word_object->word_length,
                                     word_object->word_length,
                                     word_object->word_ptr);
            fflush(perf_global.errout);
#endif
            if ((sen_object = get_sentence(sscp, word_object->match_ind))
                             != (struct sen_results *) NULL)
            {
                if ((i = sort_ind_xlat(sscp,sen_object->match_ind)) < 0)
                    fprintf(perf_global.errout,
                             "Failed to translate statement index %d\n",
                           sen_object->match_ind);
                else
/*
 * Add the file to the list for this statement
 */
                    sortp[i]->cand_prog = prog_con_name(sortp[i]->cand_prog,
                                                        argv[optind]);
            }
        }
        while ((sen_object = get_sentence(sscp, -1))
                             != (struct sen_results *) NULL)
        {
            if ((i = sort_ind_xlat(sscp,sen_object->match_ind)) < 0)
                fprintf(perf_global.errout,
                            "Failed to translate statement index %d\n",
                           sen_object->match_ind);
            else
                sortp[i]->cand_prog = prog_con_name(sortp[i]->cand_prog,
                                         argv[optind]);
        }
        (*doc_object->close_func)(doc_object->doc_channel);
    }
    free(sortp);
    return;
}
/*
 * Output a badsort export
 */
void do_export(out_name)
char * out_name;
{
struct stat_con *statp = (struct stat_con *) NULL,
               **sortp,
               **wpp;
FILE *fp;
int i;
/*
 * Allocate an array of pointers
 */
    if (badsort_base.dont_save || !badsort_base.stat_cnt)
        return;
    else
        badsort_base.dont_save = 1;
    strcpy(&buf[0], out_name);
    strcat(&buf[0], "_bk");
#ifdef MINGW32
    unlink(&buf[0]);
#endif
    rename(out_name, buf);    /* Long stop backup */
    if ((fp = fopen(out_name, "wb")) == (FILE *) NULL)
    {
        perror("Failed to create checkpoint file");
        return;
    }
    if ((sortp = (struct stat_con **) malloc (badsort_base.stat_cnt *
           sizeof ( struct stat_con *))) == (struct stat_con **) NULL)
    {
        perror("Failed to allocate space for statement sort");
        fprintf(perf_global.errout, "Location %s:%d\n", __FILE__, __LINE__);
        (void) fclose(fp);
#ifdef MINGW32
        unlink(out_name);
#endif
        rename(buf, out_name);    /* Restore last version */
        return;
    }
    else
    if (badsort_base.debug_level)
    {
        fputs("Memory allocated for sort\n", perf_global.errout);
        fflush(perf_global.errout);
    }
    setbuf(fp, buf);
    if (badsort_base.restrct == restrict_date)
    {
        for (statp = badsort_base.anchor;
            statp != (struct stat_con *) NULL;
                statp = statp->next)
            stat_retotal(statp);
    }
    for (wpp =  sortp, statp = badsort_base.anchor, i = badsort_base.stat_cnt;
            statp != (struct stat_con *) NULL && i > 0;
                statp = statp->next, i--)
    {
        *(wpp++) = statp;
    }
    if (statp != (struct stat_con *) NULL
     || (wpp - sortp) != badsort_base.stat_cnt)
        fprintf(perf_global.errout,
            "Logic Error: chain cnt (%d) != badsort_base.stat_cnt (%d)\n",
                 (wpp - sortp),  badsort_base.stat_cnt);
/*
 * Generate the desired order 
 */
    if (badsort_base.debug_level)
    {
        fprintf(perf_global.errout,
                 "Statements will be ranked assuming that disk reads\n\
are %g times more expensive than buffer gets\n", badsort_base.disk_buf_ratio);
        fflush(perf_global.errout);
    }
    (void) qwork((char *) sortp, badsort_base.stat_cnt, ccomp);
    if (badsort_base.debug_level)
    {
        fputs("Statements now sorted\n", perf_global.errout);
        fflush(perf_global.errout);
    }
/*
 * Output the data
 */
    for (wpp = sortp, i = badsort_base.stat_cnt; i > 0; i--, wpp++)
    {
        if (strlen((*wpp)->sql) != 0)
        {
            if (badsort_base.restrct != NULL
              && !(*(badsort_base.restrct))(*wpp))
                continue;
            stat_print(fp, *wpp);
        }
    }
    (void) fclose(fp);
    badsort_base.dont_save = 0;
    return;
}
/*
 * Process a single SQL statement and its associated resource costs.
 */
struct stat_con * do_one_sql(sql_text, total, compute_flag)
char * sql_text;
struct when_seen * total;
int compute_flag;       /* Work out the interval from the last saved */
{
struct stat_con *statp;
HIPT item, *ps;
struct when_seen *w;

    if ((ps = lookup(badsort_base.stat_tab, sql_text)) == (HIPT *) NULL)
    {
        badsort_base.stat_cnt++;
        item.name = strdup(sql_text);
        if ((item.body = (char *) malloc(
         sizeof(struct stat_con))) == (char *) NULL)
        {
            perror("malloc() failed");
            fprintf(perf_global.errout, "Location %s:%d\n",
                    __FILE__, __LINE__);
            return (struct stat_con *) NULL;
        }
        statp =  ((struct stat_con *) item.body);
        statp->sql_id = 0;
        statp->cant_explain = 0;
        statp->cand_prog = (struct prog_con *) NULL;
        statp->next = badsort_base.anchor;
        badsort_base.anchor = statp;
        statp->sql = item.name;
        statp->hash_val = string_hh(item.name, badsort_base.tabsize);
        statp->total = *total;
        statp->last_seen = *total;
        statp->plan = (struct plan_con *) NULL;
        statp->user_id = 0;
        statp->other_plan_cnt = 0;
        if (compute_flag)
        {
            if ((statp->anchor = (struct when_seen *)
                  malloc( sizeof(struct when_seen)))
                    ==(struct when_seen *) NULL)
            {
                perror("malloc() failed");
                fprintf(perf_global.errout, "Location %s:%d\n",
                       __FILE__, __LINE__);
                return (struct stat_con *) NULL;
            }
            *(statp->anchor) = *total;
        }
        else
            statp->anchor = (struct when_seen *) NULL;
        if (insert(badsort_base.stat_tab, item.name, item.body)
             == (HIPT *) NULL)
        {
            perror("hash insert() failed");
            fprintf(perf_global.errout, "Location %s:%d\n",
                    __FILE__, __LINE__);
            return (struct stat_con *) NULL;
        }
    }
    else
    {
        statp = ((struct stat_con *) (ps->body));
/*
 * There is a problem if the same SQL statement is retrieved more than once
 * from the shared SQL area. The SQL text normalisation process can certainly
 * cause this, and there is a suspicion that ORACLE itself occasionally stores
 * duplicate statements. The heuristics with respect to whether we should add
 * the observed units, or compute the differences, break down, since we do not
 * know what the incoming accumulator values should be compared to.
 * The solution for now is to store the readings without attempting to adjust
 * them. We may be able to sort things out better later.
 */ 
        if (compute_flag == 1)
        {
            if ((w= (struct when_seen *) malloc(sizeof
               (struct when_seen))) ==
                     (struct when_seen *) NULL)
            {
                perror("malloc() failed");
                fprintf(perf_global.errout, "Location %s:%d\n",
                        __FILE__, __LINE__);
                return (struct stat_con *) NULL;
            }
            w->next = statp->anchor;
            w->ora_flag = 0;
            statp->anchor = w;
            w->t = total->t;
            if (w->next != (struct when_seen *) NULL)
            {
                w->diff_time = w->t - w->next->t;
                if (w->diff_time > 2*badsort_base.retry_time)
                    w->diff_time = badsort_base.retry_time;
            }
            else
                w->diff_time = 0;
            if ((total->executions >=
                statp->last_seen.executions)
             && (total->disk_reads >=
                statp->last_seen.disk_reads)
             && (total->buffer_gets >=
                statp->last_seen.buffer_gets))
            {
                 w->executions = 
                     total->executions -
                     statp->last_seen.executions ;
                 w->disk_reads =
                     total->disk_reads -
                     statp->last_seen.disk_reads ;
                 w->buffer_gets =
                     total->buffer_gets -
                     statp->last_seen.buffer_gets;
#ifdef OR9
                 w->cpu =
                     total->cpu -
                     statp->last_seen.cpu;
                 w->elapsed =
                     total->elapsed -
                     statp->last_seen.elapsed;
                 statp->total.cpu += w->cpu;
                 statp->total.elapsed += w->elapsed;
                 w->direct_writes = total->direct_writes - statp->last_seen.direct_writes;
                 statp->total.direct_writes += w->direct_writes;
                 w->application_wait_time = total->application_wait_time - statp->last_seen.application_wait_time;
                 statp->total.application_wait_time += w->application_wait_time;
                 w->concurrency_wait_time = total->concurrency_wait_time - statp->last_seen.concurrency_wait_time;
                 statp->total.concurrency_wait_time += w->concurrency_wait_time;
                 w->cluster_wait_time = total->cluster_wait_time - statp->last_seen.cluster_wait_time;
                 statp->total.cluster_wait_time += w->cluster_wait_time;
                 w->user_io_wait_time = total->user_io_wait_time - statp->last_seen.user_io_wait_time;
                 statp->total.user_io_wait_time += w->user_io_wait_time;
                 w->plsql_exec_time = total->plsql_exec_time - statp->last_seen.plsql_exec_time;
                 statp->total.plsql_exec_time += w->plsql_exec_time;
                 w->java_exec_time = total->java_exec_time - statp->last_seen.java_exec_time;
                 statp->total.java_exec_time += w->java_exec_time;
#endif
                 statp->total.executions += w->executions;
                 statp->total.disk_reads += w->disk_reads;
                 statp->total.buffer_gets += w->buffer_gets;
                 statp->last_seen = *total;
            }
            else
            if ( w->diff_time > 0 )
            {
                 w->executions = total->executions ;
                 w->disk_reads = total->disk_reads;
                 w->buffer_gets = total->buffer_gets ;
#ifdef OR9
                 w->cpu = total->cpu;
                 w->elapsed = total->elapsed;
                 statp->total.cpu += w->cpu;
                 statp->total.elapsed += w->elapsed;
                 statp->total.direct_writes += w->direct_writes;
                 statp->total.application_wait_time += w->application_wait_time;
                 statp->total.concurrency_wait_time += w->concurrency_wait_time;
                 statp->total.cluster_wait_time += w->cluster_wait_time;
                 statp->total.user_io_wait_time += w->user_io_wait_time;
                 statp->total.plsql_exec_time += w->plsql_exec_time;
                 statp->total.java_exec_time += w->java_exec_time;
#endif
                 statp->total.executions += total->executions;
                 statp->total.disk_reads += total->disk_reads;
                 statp->total.buffer_gets += total->buffer_gets;
                 statp->last_seen = *total;
            }
            else
/*
 * The duplicate case; do not attempt to adjust the totals
 */
            {
                 w->executions = total->executions ;
                 w->disk_reads = total->disk_reads;
                 w->buffer_gets = total->buffer_gets ;
#ifdef OR9
                 w->cpu = total->cpu;
                 w->elapsed = total->elapsed;
                 w->direct_writes = total->direct_writes;
                 w->application_wait_time = total->application_wait_time;
                 w->concurrency_wait_time = total->concurrency_wait_time;
                 w->cluster_wait_time = total->cluster_wait_time;
                 w->user_io_wait_time = total->user_io_wait_time;
                 w->plsql_exec_time = total->plsql_exec_time;
                 w->java_exec_time = total->java_exec_time;
#endif
            }
        }
        else
        {
            statp->total.executions += total->executions;
            statp->total.disk_reads += total->disk_reads;
            statp->total.buffer_gets += total->buffer_gets;
#ifdef OR9
            statp->total.cpu += total->cpu;
            statp->total.elapsed += total->elapsed;
            statp->total.direct_writes += total->direct_writes;
            statp->total.application_wait_time += total->application_wait_time;
            statp->total.concurrency_wait_time += total->concurrency_wait_time;
            statp->total.cluster_wait_time += total->cluster_wait_time;
            statp->total.user_io_wait_time += total->user_io_wait_time;
            statp->total.plsql_exec_time += total->plsql_exec_time;
            statp->total.java_exec_time += total->java_exec_time;
#endif
            if (compute_flag == 2)
            {
                if (statp->anchor == (struct when_seen *) NULL)
                {
                    if ((statp->anchor = (struct when_seen *)
                          malloc( sizeof(struct when_seen)))
                            ==(struct when_seen *) NULL)
                    {
                        perror("malloc() failed");
                        fprintf(perf_global.errout, "Location %s:%d\n",
                               __FILE__, __LINE__);
                        return (struct stat_con *) NULL;
                    }
                    *(statp->anchor) = *total;
                }
                else
                if (statp->anchor->t == total->t)
                {
                    statp->anchor->executions += total->executions;
                    statp->anchor->disk_reads += total->disk_reads;
                    statp->anchor->buffer_gets += total->buffer_gets;
#ifdef OR9
                    statp->anchor->cpu += total->cpu;
                    statp->anchor->elapsed += total->elapsed;
                    statp->anchor->direct_writes += total->direct_writes;
                    statp->anchor->application_wait_time += total->application_wait_time;
                    statp->anchor->concurrency_wait_time += total->concurrency_wait_time;
                    statp->anchor->cluster_wait_time += total->cluster_wait_time;
                    statp->anchor->user_io_wait_time += total->user_io_wait_time;
                    statp->anchor->plsql_exec_time += total->plsql_exec_time;
                    statp->anchor->java_exec_time += total->java_exec_time;
#endif
                }
                else
                {
                    if ((w= (struct when_seen *) malloc(sizeof
                               (struct when_seen))) ==
                                     (struct when_seen *) NULL)
                    {
                        perror("malloc() failed");
                        fprintf(perf_global.errout, "Location %s:%d\n",
                                __FILE__, __LINE__);
                        return (struct stat_con *) NULL;
                    }
                    w->next = statp->anchor;
                    w->ora_flag = 0;
                    statp->anchor = w;
                    w->t = total->t;
                    w->diff_time = total->diff_time;
                    w->executions = total->executions;
                    w->disk_reads = total->disk_reads;
                    w->buffer_gets = total->buffer_gets;
#ifdef OR9
                    w->cpu = total->cpu;
                    w->elapsed = total->elapsed;
                    w->direct_writes = total->direct_writes;
                    w->application_wait_time = total->application_wait_time;
                    w->concurrency_wait_time = total->concurrency_wait_time;
                    w->cluster_wait_time = total->cluster_wait_time;
                    w->user_io_wait_time = total->user_io_wait_time;
                    w->plsql_exec_time = total->plsql_exec_time;
                    w->java_exec_time = total->java_exec_time;
#endif
                }
            }
        }
    }
    return statp;
}
/*
 * Process a single Plan
 */
struct plan_con * do_one_plan(plan_text)
char * plan_text;
{
HIPT item, *ps;
struct plan_con * cur_plan;

    if (badsort_base.debug_level > 3)
        fputs("do_one_plan() called\n", perf_global.errout);
    if ((ps = lookup(badsort_base.plan_tab, plan_text)) == (HIPT *) NULL)
    {
        item.name = strdup(plan_text);
        if ((item.body = (char *) malloc(sizeof(struct plan_con)))
                    == (char *) NULL)
        {
            perror("malloc() failed");
            fprintf(perf_global.errout, "Location %s:%d\n",
                    __FILE__, __LINE__);
            exit(1);
        }
        cur_plan = ((struct plan_con *) item.body);
        cur_plan->plan_id = 0;
        cur_plan->next = badsort_base.panchor;
        badsort_base.panchor = cur_plan;
        cur_plan->plan = item.name;
        cur_plan->hash_val = string_hh(item.name, badsort_base.tabsize);
        if (insert(badsort_base.plan_tab, item.name, item.body)
             == (HIPT *) NULL)
        {
            perror("hash insert() failed");
            fprintf(perf_global.errout, "Location %s:%d\n",
                    __FILE__, __LINE__);
            exit(1);
        }
    }
    else
        cur_plan = ((struct plan_con *) (ps->body));
    return cur_plan;
}
/*
 * Read a single file in the badsort output format or the badfind.sh format
 */
void do_one_file(file_to_do)
char * file_to_do;
{
int n;
double secs_since;
struct when_seen * w, *w1;
char * x;
struct stat sbuf; 
enum badtok_id badtok_id;
struct stat_con cur_stat,
               *statp = (struct stat_con *) NULL;
struct plan_con  *cur_plan;
FILE *fp;

    em_len1 = sprintf(exec_marker1,"%s%s",badsort_base.prompt_label,SQL_REM);
    sm_len = sprintf(stat_marker,"%s%s",badsort_base.prompt_label,SQL_EXPLAIN);
    (void) sprintf(scan_marker, "%s%s%s", badsort_base.prompt_label, SQL_REM,
                   " %lf %lf %lf");
    memset((char *) &cur_stat,0,sizeof(cur_stat));

    if (stat(file_to_do, &sbuf) > -1
     && (fp = fopen(file_to_do,"rb")) != (FILE *) NULL)
    {
        setbuf(fp, buf);
        badsort_base.when_flag = 0;  /* Flag that we are in a list of timings */
        badsort_base.sql_now = 0;    /* Flag that we are about to see SQL     */
        if (badsort_base.debug_level)
            fprintf(perf_global.errout, "Doing %s\n", file_to_do);
        if (!badsort_base.merge_flag)
            cur_stat.total.t = sbuf.st_mtime;
        look_status = BADCLEAR;
        badtok_id = get_badtok(fp);
        while (badtok_id != BADPEOF)
        {
#ifdef DEBUG
            fprintf(perf_global.errout, "badtok_id:%d\n", badtok_id);
            fputs(badsort_base.tbuf, perf_global.errout);
#endif
            switch (badtok_id)
            {
            case BADEXECUTION:
                if (sscanf(badsort_base.tbuf,scan_marker,
                         &(cur_stat.total.executions),
                         &(cur_stat.total.disk_reads),
                         &(cur_stat.total.buffer_gets)) != 3
#ifdef OR9
                 && sscanf(badsort_base.tbuf,EXEC_SCANF,
                         &(cur_stat.total.executions),
                         &(cur_stat.total.disk_reads),
                         &(cur_stat.total.buffer_gets),
                         &(cur_stat.total.cpu),
                         &(cur_stat.total.elapsed),
                         &(cur_stat.total.direct_writes),
                         &(cur_stat.total.application_wait_time),
                         &(cur_stat.total.concurrency_wait_time),
                         &(cur_stat.total.cluster_wait_time),
                         &(cur_stat.total.user_io_wait_time),
                         &(cur_stat.total.plsql_exec_time),
                         &(cur_stat.total.java_exec_time)
                         ) != 12
#else
                 && sscanf(badsort_base.tbuf,EXEC_SCANF,
                         &(cur_stat.total.executions),
                         &(cur_stat.total.disk_reads),
                         &(cur_stat.total.buffer_gets)) != 3
#endif
                  )
                {
                    if (badsort_base.debug_level)
                        fprintf(perf_global.errout, "Failed to match %s",
                                                     badsort_base.tbuf);
                    cur_stat.total.executions = 0.0;
                    cur_stat.total.disk_reads = 0.0;
                    cur_stat.total.buffer_gets = 0.0;
#ifdef OR9
                    cur_stat.total.cpu = 0.0;
                    cur_stat.total.elapsed = 0.0;
                    cur_stat.total.direct_writes = 0.0;
                    cur_stat.total.application_wait_time = 0.0;
                    cur_stat.total.concurrency_wait_time = 0.0;
                    cur_stat.total.cluster_wait_time = 0.0;
                    cur_stat.total.user_io_wait_time = 0.0;
                    cur_stat.total.plsql_exec_time = 0.0;
                    cur_stat.total.java_exec_time = 0.0;
#endif
                }
                break;
            case BADSQL:
                statp = do_one_sql(badsort_base.tbuf, &(cur_stat.total),
                             !(badsort_base.merge_flag));
/*
 * Make sure a single set of execution statistics is only used once
 */
                cur_stat.total.executions = 0.0;
                cur_stat.total.disk_reads = 0.0;
                cur_stat.total.buffer_gets = 0.0;
#ifdef OR9
                cur_stat.total.cpu = 0.0;
                cur_stat.total.elapsed = 0.0;
                cur_stat.total.direct_writes = 0.0;
                cur_stat.total.application_wait_time = 0.0;
                cur_stat.total.concurrency_wait_time = 0.0;
                cur_stat.total.cluster_wait_time = 0.0;
                cur_stat.total.user_io_wait_time = 0.0;
                cur_stat.total.plsql_exec_time = 0.0;
                cur_stat.total.java_exec_time = 0.0;
#endif
                break;
            case BADPLAN:
                cur_plan = do_one_plan(badsort_base.tbuf);
                if (statp != (struct stat_con *) NULL
                  && cur_plan != (struct plan_con *) NULL)
                {
                    if (statp->plan == (struct plan_con *) NULL)
                        statp->plan = cur_plan;
                    else
                    if (statp->plan != cur_plan)
                    {
                    int i;
                        for (i = 0; i < statp->other_plan_cnt; i++)
                            if (statp->other_plan[i] == cur_plan)
                                break;
                        if (i >= statp->other_plan_cnt && i < 4)
                        {
                            statp->other_plan[i] = cur_plan;
                            statp->other_plan_cnt = i + 1;
                        }
                    }
                }
                break;
            case BADCAND:
                if (statp != (struct stat_con *) NULL)
                {
                struct prog_con * nc = statp->cand_prog;
                    statp->cand_prog =
                           (struct prog_con *) malloc(sizeof(struct prog_con));
                    statp->cand_prog->next = nc;
                    statp->cand_prog->name = strdup(badsort_base.tbuf);
                    statp->cand_prog->module_id = -1;
                }
                break;
            case BADWHEN:
                if ((w= (struct when_seen *) malloc(sizeof
                   (struct when_seen))) == (struct when_seen *) NULL)
                {
                    perror("malloc() failed");
                    fprintf(perf_global.errout, "Location %s:%d\n",
                            __FILE__, __LINE__);
                    exit(1);
                }
                (void) date_val(badsort_base.tbuf,
                         "dd Mon yyyy hh24:mi:ss",&x,&secs_since);
#ifdef OR9
                if ( (n = sscanf(badsort_base.tbuf + 20,WHEN_SCANF,
                         &(w->executions),
                         &(w->disk_reads),
                         &(w->buffer_gets),
                         &(w->cpu),
                         &(w->elapsed),
                         &(w->direct_writes),
                         &(w->application_wait_time),
                         &(w->concurrency_wait_time),
                         &(w->cluster_wait_time),
                         &(w->user_io_wait_time),
                         &(w->plsql_exec_time),
                         &(w->java_exec_time),
                         &(w->diff_time))) != 13)
#else
                if ( (n = sscanf(badsort_base.tbuf + 20,WHEN_SCANF,
                         &(w->executions),
                         &(w->disk_reads),
                         &(w->buffer_gets),
                         &(w->diff_time))) != 4)
#endif
                {
                    fprintf(perf_global.errout, "Only saw %d; failed to match the numbers in %s",
                        n, badsort_base.tbuf + 20);
                    free((char *) w);
                }
                else
                {
                    secs_since = gm_secs((time_t) secs_since);
#ifdef DEBUG
                    fprintf(perf_global.errout, WHEN_SCANF,
                         (w->executions),
                         (w->disk_reads),
                         (w->buffer_gets),
#ifdef OR9
                         (w->cpu),
                         (w->elapsed),
                         (w->direct_writes),
                         (w->application_wait_time),
                         (w->concurrency_wait_time),
                         (w->cluster_wait_time),
                         (w->user_io_wait_time),
                         (w->plsql_exec_time),
                         (w->java_exec_time),
#endif
                         (w->diff_time));
                    fputc('\n', perf_global.errout);
#endif
/*
 * If we have already seen this one (possible if we are processing a
 * re-normalised input file) add the details to the existing accumulators.
 * Otherwise, we create a new entry.
 */
                    for (w1 = statp->anchor;
                            w1 != (struct when_seen *) NULL;
                                w1 = w1->next)
                    {
                        if (secs_since == w1->t)
                        {
                            w1->executions += w->executions;
                            w1->disk_reads += w->disk_reads;
                            w1->buffer_gets += w->buffer_gets;
#ifdef OR9
                            w1->cpu += w->cpu;
                            w1->elapsed += w->elapsed;
                            w1->direct_writes += w->direct_writes;
                            w1->application_wait_time += w->application_wait_time;
                            w1->concurrency_wait_time += w->concurrency_wait_time;
                            w1->cluster_wait_time += w->cluster_wait_time;
                            w1->user_io_wait_time += w->user_io_wait_time;
                            w1->plsql_exec_time += w->plsql_exec_time;
                            w1->java_exec_time += w->java_exec_time;
#endif
                            if (w1->diff_time == 0)
                                w1->diff_time = w->diff_time;
                            free(w);
                            break;
                        }
                    }
                    if (w1 == (struct when_seen *) NULL)
                    {
                        w->ora_flag = 0;
                        w->next = statp->anchor;
                        statp->anchor = w;
                        w->t = (time_t) secs_since;
                    }
                }
                break;
            default:
                fprintf(perf_global.errout, "%s:%d Syntax error in file %s\n",
                               __FILE__, __LINE__, file_to_do);
                fprintf(perf_global.errout, "TOKEN: %d   Value: %s\n",
                                 badtok_id,badsort_base.tbuf);
                break;
            }
            badtok_id = get_badtok(fp);
        }
        if (badsort_base.debug_level)
            fprintf(perf_global.errout, "%d Statements now seen\n",
                                badsort_base.stat_cnt);
        (void) fclose(fp);
    }
    else
    {
        perror("File Access/Open");
        fprintf(perf_global.errout, "Failed to process %s\n", file_to_do);
    }
    return;
}
/*
 * Read a single file with a separate scanner routine. New standard interface.
 */
void do_one_stream(file_to_do)
char * file_to_do;
{
time_t cur_time = 0;
char * x;
struct stat sbuf; 
struct stat_con cur_stat, * csp, *statp = (struct stat_con *) NULL;
FILE *fp;
#ifndef  SOLAR
    memset((char *) &cur_stat,0,sizeof(cur_stat));
    if (stat(file_to_do, &sbuf) > -1
     && (fp = fopen(file_to_do,"rb")) != (FILE *) NULL)
    {
        setbuf(fp, buf1);
        if (badsort_base.debug_level)
            fprintf(perf_global.errout, "Doing %s\n", file_to_do);
#ifdef MS_SQL_EXTRACT
        while((csp = mstrc_one_rec(fp, "`"))
                          != (struct stat_con *) NULL)
#else
        while((csp = mstrc_bin_rec(fp))
                          != (struct stat_con *) NULL)
#endif
        {
/*
            if (csp->total.buffer_gets < 1.0
             && csp->total.disk_reads < 1.0
             && csp->total.cpu < 1.0)
            {
                if (csp->sql != NULL)
                    free(csp->sql);
                continue;
            }
*/
            cur_stat.total = csp->total;
/*
 * Make a few adjustments.
 * - disk_reads = disk_reads
 * - buffer_gets = disk_writes
 * - cpu = CPU
 * - elapsed = elapsed.
 * Leaving report formats as they are:
 * - Add disk reads and writes
 * - Put CPU in buffer gets
 * Ratio must be .00003 or thereabouts
            cur_stat.total.disk_reads += cur_stat.total.buffer_gets;
            cur_stat.total.buffer_gets = cur_stat.total.cpu;
 */ 
            if (cur_time == 0)
                cur_time = cur_stat.total.t;
            if ((((int) cur_stat.total.t) - ((int) cur_time))
                    > badsort_base.retry_time)
            {
/*
 * Checkpoint time.
 */
                if (cur_stat.total.t != 0)
                    do_export(badsort_base.out_name);   /* Save the data */
                 cur_time += badsort_base.retry_time;
            }
            cur_stat.total.t = cur_time;
            cur_stat.total.diff_time = badsort_base.retry_time;
            if (csp->sql != NULL)
            {
                statp = do_one_sql(csp->sql, &(cur_stat.total), 2);
                free(csp->sql);
            }
        }
        if (badsort_base.debug_level)
            fprintf(perf_global.errout, "%d Statements now seen\n",
                                badsort_base.stat_cnt);
        (void) fclose(fp);
    }
    else
    {
        perror("File Access/Open");
        fprintf(perf_global.errout, "Failed to process %s\n", file_to_do);
    }
#endif
    return;
}
/*
 * Output is required for Microsoft Access. We create six files: one
 * for the SQL, one for the Plans, one for the usages, one for the tables,
 * one for the indexes and one for the modules.
 */
void do_access_exp()
{
FILE * asql;
FILE * aindex;
FILE * atable;
FILE * aplan;
FILE * ausage;
FILE * amodule;
struct prog_con * pc;
struct plan_con  *cur_plan;
struct when_seen  *w;
struct stat_con *statp;

    if ((asql = fopen("asql.txt","wb")) == (FILE *) NULL)
    {
        perror("Failed to create asql.txt");
        return;
    }
    setbuf(asql,buf);
    fprintf(asql,
#ifdef OR9
   "%s}%s}%s}%s}%s}%s}%s}%s}%s}%s}%s}%s}%s}%s\r\n",
#else
   "%s}%s}%s}%s}%s}%s}%s}%s}%s}%s}%s}%s\r\n",
#endif
            "SQL_ID",
            "HASH_VAL",
            "EXECS",
            "DISK_READS",
            "BUFFER_GETS",
#ifdef OR9
            "CPU",
            "ELAPSED",
#endif
            "CPU",
            "ELAPSED",
            "CUR_PLAN_ID",
            "OTHER_PLAN_CNT",
            "OTHER_PLAN_1",
            "OTHER_PLAN_2",
            "OTHER_PLAN_3",
            "OTHER_PLAN_4",
            "SQL_TEXT");
    if ((aplan = fopen("aplan.txt","wb")) == (FILE *) NULL)
    {
        perror("Failed to create aplan.txt");
        return;
    }
    setbuf(aplan, buf1);
    fprintf(aplan,"%s}%s}%s\r\n",
            "PLAN_ID",
            "HASH_VAL",
            "PLAN_TEXT");
    if ((aindex = fopen("aindex.txt","wb")) == (FILE *) NULL)
    {
        perror("Failed to create aindex.txt");
        return;
    }
    setbuf(aindex,buf2);
    fprintf(aindex,"%s}%s\r\n",
            "PLAN_ID",
            "INDEX_NAME");
    if ((atable = fopen("atable.txt","wb")) == (FILE *) NULL)
    {
        perror("Failed to create atable.txt");
        return;
    }
    setbuf(atable, buf3);
    fprintf(atable,"%s}%s\r\n",
            "PLAN_ID",
            "TABLE_NAME");
    if ((ausage = fopen("ausage.txt","wb")) == (FILE *) NULL)
    {
        perror("Failed to create ausage.txt");
        return;
    }
    setbuf(ausage, buf4);
    fprintf(ausage,
#ifdef OR9
            "%s}%s}%s}%s}%s}%s}%s}%s}%s\r\n",
#else
            "%s}%s}%s}%s}%s}%s}%s\r\n",
#endif
            "SQL_ID",
            "PLAN_ID",
            "END_DTTM",
            "EXECS",
            "DISK_READS",
            "BUFFER_GETS",
#ifdef OR9
            "CPU",
            "ELAPSED",
#endif
            "KNOWN_TIME");
    if ((amodule = fopen("amodule.txt","wb")) == (FILE *) NULL)
    {
        perror("Failed to create amodule.txt");
        return;
    }
    setbuf(amodule, buf5);
    fprintf(amodule,"%s}%s\r\n",
            "SQL_ID",
            "MODULE_NAME");
    for ( statp = badsort_base.anchor;
            statp != (struct stat_con *) NULL ;
                statp = statp->next)
    {
        fprintf(asql,
#ifdef OR9
             "%u}%u}%.0f}%.0f}%.0f}%.6f}%.6f}%u}%u}%u}%u}%u}%u}%s\r\n",
#else
             "%u}%u}%.0f}%.0f}%.0f}%u}%u}%u}%u}%u}%u}%s\r\n",
#endif
            (unsigned long) (statp->sql),
            statp->hash_val,
            statp->total.executions,
            statp->total.disk_reads,
            statp->total.buffer_gets,
#ifdef OR9
            statp->total.cpu,
            statp->total.elapsed,
#endif
            ((statp->plan == (struct plan_con *) NULL)? 0 :
                            (unsigned long) (statp->plan->plan)),
            statp->other_plan_cnt,
            ((statp->other_plan_cnt < 1)? 0 :
                            (unsigned long) (statp->other_plan[0])),
            ((statp->other_plan_cnt < 2)? 0 :
                            (unsigned long) (statp->other_plan[1])),
            ((statp->other_plan_cnt < 3)? 0 :
                            (unsigned long) (statp->other_plan[2])),
            ((statp->other_plan_cnt < 4)? 0 :
                            (unsigned long) (statp->other_plan[3])),
            statp->sql);
        for ( w = statp->anchor;
                w != (struct when_seen *) NULL ;
                    w = w->next)
        {
            if (w->disk_reads != 0 || w->buffer_gets != 0)
            {
            char *x = ctime(&(w->t));
            fprintf(ausage,
#ifdef OR9
                   "%u}%u}%2.2s-%3.3s-%4.4s %8.8s}%.0f}%.0f}%.0f}%.6f}%.6f}%u\r\n",
#else
                   "%u}%u}%2.2s-%3.3s-%4.4s %8.8s}%.0f}%.0f}%.0f}%u\r\n",
#endif
                    (unsigned long) (statp->sql),
                    ((statp->plan == (struct plan_con *) NULL)? 0 :
                            (unsigned long) (statp->plan->plan)),
                    x+8,
                    x+4,
                    x+20,
                    x+11,
                    w->executions,
                    w->disk_reads,
                    w->buffer_gets,
#ifdef OR9
                    w->cpu,
                    w->elapsed,
#endif
                    w->diff_time);
            }
        }
        for (pc = statp->cand_prog;
             pc != (struct prog_con *) NULL; pc = pc->next)
            fprintf(amodule,"%u}%s\r\n",
                    (unsigned long) (statp->sql),
                      pc->name);
    }
    for ( cur_plan = badsort_base.panchor;
            cur_plan != (struct plan_con *) NULL ;
                cur_plan = cur_plan->next)
    {
        fprintf(aplan,"%u}%u}%s\r\n",
            (unsigned long) cur_plan->plan,
            cur_plan->hash_val,
            cur_plan->plan);
        do_ind_tab(cur_plan->plan,atable,aindex,0);
    }
    (void) fclose(asql);
    (void) fclose(aplan);
    (void) fclose(aindex);
    (void) fclose(atable);
    (void) fclose(ausage);
    (void) fclose(amodule);
    badsort_base.access_flag = 0;
    return;
}
#ifdef NOCALC
/*
 * Used if there is no usable lex or yacc, and no copy of IRsearch.c or IRslex.c
 */
void do_sql_search(cp, fp, pp)
char * cp;
FILE * fp;
struct prog_con *pp;
{
    fputs("This program is not linked with the SQL search logic\n", fp);
    return;
}
#endif
