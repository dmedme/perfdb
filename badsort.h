/*
 * badsort.h - header file for badsort
 ***********************************************************************
 *This file is a Trade Secret of E2 Systems. It may not be executed,
 *copied or distributed, except under the terms of an E2 Systems
 *UNIX Instrumentation for CFACS licence. Presence of this file on your
 *system cannot be construed as evidence that you have such a licence.
 ***********************************************************************
 * Copyright (c) E2 Systems Limited 1997
 * %W% %D% %T% %E% %U
 */
#ifndef BADSORT_H
#define BADSORT_H
#ifdef WORKSPACE
#undef WORKSPACE
#endif
#define WORKSPACE 262144
#include "natregex.h"
#include "orlex.h"
enum badlook_status {BADCLEAR, BADPRESENT};
enum badtok_id {BADSQL, BADEXECUTION, BADPLAN, BADWHEN, BADPEOF, BADPARTIAL,
   BADCAND};
struct badsort_base {
/*
 * Truly Global Information
 */
HASH_CON * stat_tab;
HASH_CON * plan_tab;
int tabsize;       /* For hash function computations */
struct stat_con *anchor;
struct plan_con * panchor;
int stat_cnt;
int debug_level;
int retry_time;
int dont_save;
char * out_name;
/*
 * File format settings
 */
char * tbuf;
char * tlook;
int access_flag;       /* Set outputting for Microsoft ACCESS        */
int when_flag;         /* Flag that we are in a list of timings      */
int cand_flag;         /* Flag that we are in a list of candidates   */
int sql_now;           /* Flag that we are about to see SQL          */
int merge_flag;        /* Set when merging output files subsequently */
double disk_buf_ratio; /* Relative waiting of disk and buffered I/O  */
double buf_cost;       /* CPU cost of a buffer get                   */
int single_flag;       /* Single statement or all statement ranking  */
char *prompt_label;
int pl_len;
int indent;
/*
 * ORACLE Access details
 */
char * uid;
char * uid_explain;
char * uid_stats;
char * fifo_name;
struct sess_con * in_con;
char * optimiser_goals;
int plan_test_cnt;
struct dyn_con * plan_dyn[5];
/*
 * Information Retrieval master data
 */
struct scan_con * scan_con;
int (*restrct)();
char * qual_pattern;
reg_comp qual_compiled;
char * date_format;
double start_time;
double end_time;
};
struct badsort_base badsort_base;
struct when_seen {
    time_t t;
    long int diff_time;
    double accurate;
    double executions;
    double disk_reads;
    double buffer_gets;
    double cpu;
    double elapsed;
    double direct_writes;
    double application_wait_time;
    double concurrency_wait_time;
    double cluster_wait_time;
    double user_io_wait_time;
    double plsql_exec_time;
    double java_exec_time;
    int ora_flag;
    struct when_seen * next;
};
struct stat_con {
    char * sql;
    struct plan_con * plan;
    int other_plan_cnt;
    struct plan_con * other_plan[4];
    long sql_id;
    long hash_val;
    int child_number;
    long user_id;
    int cant_explain;
    char osql_id[64];
    double plan_hash_value;
    struct prog_con * cand_prog;
    struct when_seen total;
    struct when_seen last_seen;
    struct when_seen * anchor;
    struct stat_con * next;
};
struct plan_con {
    char * plan;
    long plan_id;
    long hash_val;
    struct plan_con * next;
};
struct prog_con {
    char * name;
    unsigned long module_id;
    struct prog_con * next;
};
/*
 * Various default attributes of the source files
 */
#define SQL_REM "REM"
#define SQL_EXPLAIN "explain plan"
/*
 * Strings that need to be both read and written
 */
#define EXEC_PLAN "Execution Plan"
#define EXEC_UNDERLINE "--------------"
#define ACTIVITY_HISTORY "Activity History"
#define CANDIDATES "Candidate Modules"
#ifdef SYBASE
#define ACTIVITY_HISTORY_LEN 16
#define CANDIDATES_LEN 17
#define EXEC_TOTAL_LEN 17
#define EXEC_FORMAT "Total Executions: %.0f Reserved: %.0f Response Time: %-16.3f\n"
#define EXEC_SCANF "Total Executions: %lf Reserved: %lf Response Time: %lf"
#define WHEN_FORMAT "%2.2s %3.3s %4.4s %8.8s %10.0f  %10.0f %12.3f  %8d\n"
#define WHEN_SCANF  "%lf %lf %lf %ld"
#else
#ifdef OR9
#define EXEC_FORMAT "Total Executions: %.0f Disk Reads: %.0f Buffer Gets: %.0f CPU Time: %.6f Elapsed: %.6f Direct Writes: %.0f Application Wait Time: %.6f Concurrency Wait Time: %.6f Cluster Wait Time: %.6f IO Wait Time: %.6f PL/SQL Exec Time: %.6f Java Exec Time: %.6f\n"
#define EXEC_SCANF "Total Executions: %lf Disk Reads: %lf Buffer Gets: %lf CPU Time: %lf Elapsed: %lf Direct Writes: %lf Application Wait Time: %lf Concurrency Wait Time: %lf Cluster Wait Time: %lf IO Wait Time: %lf PL/SQL Exec Time: %lf Java Exec Time: %lf\n"
#else
#define EXEC_FORMAT "Total Executions: %.0f Disk Reads: %.0f Buffer Gets: %.0f\n"
#define EXEC_SCANF "Total Executions: %lf Disk Reads: %lf Buffer Gets: %lf"
#endif
#ifdef OR9
#define WHEN_FORMAT "%2.2s %3.3s %4.4s %8.8s %10.0f  %10.0f   %10.0f  %14.6f  %14.6f  %14.6f  %14.6f  %14.6f  %14.6f  %14.6f  %14.6f  %14.6f  %8d\n"
#define WHEN_SCANF "%lf  %lf  %lf  %lf  %lf  %lf  %lf  %lf  %lf  %lf  %lf  %lf  %ld\n"
#else
#define WHEN_FORMAT "%2.2s %3.3s %4.4s %8.8s %10.0f  %10.0f   %10.0f  %8d\n"
#ifdef OSF
#define WHEN_SCANF  "%lf %lf %lf %ld"
#else
#define WHEN_SCANF  "%lf %lf %lf %d"
#endif
#endif
#endif
#ifdef OR9
#define END_HISTORY "===================  ==========  ==========  ===========  ===============  ===============  ========\n"
#else
#define END_HISTORY "===================  ==========  ==========  ===========  ========\n"
#endif
struct plan_con * do_one_plan(); /* Process an Execution Plan                */
struct stat_con * do_one_sql();  /* Process a SQL Statement                  */
struct stat_con * mstrc_one_rec();
                                 /* Process a SQL Statement from a file      */
struct stat_con * mstrc_bin_rec();
                                 /* Process a MS Profiler a file             */
void proc_args();                /* Process arguments                        */
void do_export();                /* Produce an export file (badsort format)  */
void do_access_exp();            /* Process Microsoft Access exports         */
void do_one_file();              /* Process a file                           */
void do_oracle_table();          /* Associate a table with a plan            */
void do_oracle_index();          /* Associate an index with a plan           */
void oracle_cleanup();
void do_oracle_imp();
int oracle_init();
void do_oracle_sql();
void do_oracle_mapping();
/*
 * Event scheduling
 */
struct arg_block {
    int argc;
    char ** argv;
};
void add_time();              /* Add an event for future execution        */
void do_one_pass();           /* One pass through the ORACLE data         */
extern int getopt();
extern char *  optarg;
extern int optind;
extern int opterr;
void (*sigset())();
char * to_char();
void clear_progs();
void clear_whens();
void restart();
void ir_ini();
#endif
