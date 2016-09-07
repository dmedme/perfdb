/*
 * Bad SQL hunting logic for badsort.
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
#define SLEEP_FACTOR 1000
#else
#define SLEEP_FACTOR 1
#endif
#ifndef VCC2003
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#else
#include <WinSock2.h>
#endif
#else
#include <windows.h>
#include <winsock2.h>
#include "e2orant.h"
#define SLEEP_FACTOR 1000
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
#ifdef strdup
#undef strdup
#endif
extern char * strdup();
extern char * strnsave();
jmp_buf ora_fail;            /* Used to recover from ORACLE errors        */
static void status_dump();   /* Function to dump out internal status info */
static void do_oracle_plan();
static void do_oracle_usage();
static char * get_collector();
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
/*****************************************************************************
 *  Future event management
 */
static struct go_time
{
    struct arg_block * abp;
    time_t go_time;
} go_time[20];
static short int head=0, tail=0, clock_running=0;
static struct timeval poll_wait = {10,0};    /* wait 10 seconds timeval structure */
static long alarm_save;                 /* What's in the alarm clock; timer */
static void (*prev_alrm)();             /* Whatever is previously installed
                                           (either rem_time() or nothing) */
static void rem_time();
/**************************************************************************
 * ORACLE Elements
 */
struct sess_con * con;
static struct sess_con * con_explain;
static struct sess_con * con_stats;
/*
 * SQL statement (fragments)
 */
#define CURS_COST 1
static struct dyn_con * dcost;
#ifdef OR10
static char * scost = "select b.sql_id,\n\
       a.executions,\n\
       a.disk_reads,\n\
       a.buffer_gets,\n\
       b.sql_text,\n\
       a.parsing_schema_id,\n\
       a.command_type,\n\
       a.plan_hash_value,\n\
       a.cpu_time,\n\
       a.elapsed_time,\n\
       a.child_number,\n\
       a.direct_writes,\n\
       a.application_wait_time,\n\
       a.concurrency_wait_time,\n\
       a.cluster_wait_time,\n\
       a.user_io_wait_time,\n\
       a.plsql_exec_time,\n\
       a.java_exec_time\n\
from sys.v_$sql a,\n\
     sys.v_$sqltext_with_newlines b\n\
where a.cpu_time > :buf_cost\n\
and a.sql_id=b.sql_id\n\
and a.is_obsolete = 'N'\n\
and lower(substr(a.sql_text,1,7)) !=  'explain'\n\
order by a.child_number,a.parsing_schema_id,a.plan_hash_value,b.sql_id, b.piece";
#define CURS_PLAN 27
static struct dyn_con * dplan;
static char * splan ="select\n\
     lpad(' ',2*depth)||operation||' '||\n\
     options||' '|| object_name||' '|| object_type ||\n\
     decode(optimizer,NULL,'',' ('|| optimizer ||')')||\n\
     decode(cost,NULL,'',\n\
    '('||to_char(cost)||','||to_char(cardinality)||','||to_char(bytes)||','||\n\
     to_char(cpu_cost)||','||to_char(io_cost)||')'),\n\
     search_columns,\n\
     access_predicates,\n\
     filter_predicates\n\
from sys.v_$sql_plan\n\
where sql_id = :osql_id\n\
start with id = 0\n\
connect by prior id = parent_id\n\
and prior hash_value = hash_value\n\
and prior child_number = child_number\n\
order siblings by hash_value, id, position";
#else
#ifdef OR9
static char * scost = "select rawtohex(a.address),\n\
       a.executions,\n\
       a.disk_reads,\n\
       a.buffer_gets,\n\
       b.sql_text,\n\
       a.parsing_schema_id,\n\
       a.command_type,\n\
       a.plan_hash_value,\n\
       a.cpu_time,\n\
       a.elapsed_time,\n\
       a.child_number\n\
from sys.v_$sql a,\n\
     sys.v_$sqltext_with_newlines b\n\
where a.elapsed_time > :buf_cost\n\
and a.is_obsolete = 'N'\n\
and a.address=b.address\n\
and lower(substr(a.sql_text,1,7)) != 'explain'\n\
order by a.child_number,a.parsing_schema_id,a.plan_hash_value,1, b.piece";
#define CURS_PLAN 27
static struct dyn_con * dplan;
static char * splan="select lpad(' ',2*level)||operation||' '||\n\
     options||' '|| object_name||' '||\n\
     decode(optimizer,NULL,'',' ('|| optimizer ||')')||\n\
     decode(cost,NULL,'',\n\
    '('||to_char(cost)||','||to_char(cardinality)||','||to_char(bytes)||','||\n\
     to_char(cpu_cost)||','||to_char(io_cost)||')')\n\
from sys.v_$sql_plan\n\
where address = hextoraw(:osql_id)\n\
start with id = 0\n\
connect by prior id = parent_id\n\
and prior hash_value = hash_value\n\
and prior child_number = child_number\n\
order siblings by hash_value, id, position";
#else
#ifdef GLASGOW_HOUSING
static char * scost = "select b.address,\n\
       a.executions,\n\
       a.disk_reads,\n\
       a.buffer_gets,\n\
       b.sql_text,\n\
       a.parsing_schema_id,\n\
       a.command_type\n\
from sys.v_$sqlarea a,\n\
     sys.v_$sqltext b\n\
where :buf_cost*(a.buffer_gets + a.disk_reads *:disk_buf_ratio) > 1\n\
and a.address=b.address\n\
and lower(substr(a.sql_text,1,7)) !=  'explain'\n\
order by b.address, b.piece";
#else
static char * scost = "select b.address,\n\
       a.executions,\n\
       a.disk_reads,\n\
       a.buffer_gets,\n\
       b.sql_text,\n\
       a.parsing_schema_id,\n\
       a.command_type\n\
from sys.v$sqlarea a,\n\
     sys.v$sqltext b\n\
where :buf_cost*(a.buffer_gets + a.disk_reads *:disk_buf_ratio) > 1\n\
and lower(substr(a.sql_text,1,7)) !=  'explain'\n\
and a.address=b.address\n\
order by b.address, b.piece";
#endif
#endif
#endif
#define CURS_DEL 2
static struct dyn_con * ddel;
static char * sdel="delete badsort_plan_table where statement_id='badsort'";
#define CURS_EXP 3
static struct dyn_con * dexp;
static char * sexp="explain plan set statement_id='badsort' into badsort_plan_table for ";
/*
 * When we get a database that supports them to test with, we want to
 * examine the extra plan_table columns that later ORACLE releases support,
 * and incorporate them.
 *
 * When we support alternative plan computations, this statement and the
 * session switch statements will need to be associated with a vector of
 * dynamic statement control blocks, one for each ORACLE connection.
 */
#define CURS_XPLREAD 4
static struct dyn_con * dxplread;
static char * sxplread="select lpad(' ',2*level)||operation||' '||\n\
     options||' '|| object_name||' '|| object_type ||\n\
     decode(optimizer,NULL,'',' ('|| optimizer ||')')||\n\
     decode(cost,NULL,'',\n\
    '('||to_char(cost)||','||to_char(cardinality)||','||to_char(bytes)||','||\n\
     to_char(cpu_cost)||','||to_char(io_cost)||')')\n\
from badsort_plan_table\n\
where statement_id = 'badsort'\n\
connect by prior id = parent_id\n\
and prior statement_id = statement_id\n\
start with parent_id is null";
/*
 * Update the history in the ORACLE database
 */
#define CURS_BADSORT_PLAN 5
static struct dyn_con * dbadsort_plan;
static char * sbadsort_plan="insert into BADSORT_PLAN(PLAN_ID,HASH_VAL,\n\
PLAN_TEXT) values (:plan_id,:hash_val,:plan_text)";
#define CURS_BADSORT_SQL 6
static struct dyn_con * dbadsort_sql;
static char * sbadsort_sql="insert into BADSORT_SQL(SQL_ID,HASH_VAL,\n\
CUR_PLAN_ID,SQL_TEXT) values (:sql_id,:hash_val,:plan_id,:sql_text)";
#define CURS_BADSORT_INDEX 7
static struct dyn_con * dbadsort_index;
static char * sbadsort_index="insert into BADSORT_INDEX(PLAN_ID,INDEX_NAME)\n\
values (:plan_id,:index_name)";
#define CURS_BADSORT_TABLE 8
static struct dyn_con * dbadsort_table;
static char * sbadsort_table="insert into BADSORT_TABLE(PLAN_ID,TABLE_NAME)\n\
values (:plan_id,:table_name)";
#define CURS_BADSORT_USAGE 9
static struct dyn_con * dbadsort_usage;
static char * sbadsort_usage="insert into BADSORT_USAGE(SQL_ID,PLAN_ID,\n\
END_DTTM,EXECS,DISK_READS,BUFFER_GETS,KNOWN_TIME) values (:sql_id,:plan_id,\n\
to_date(:end_dttm,'DD Mon YYYY HH24:MI:SS'),:execs,:disk_reads,:buffer_gets,\n\
:known_time)";
#define CURS_BADSORT_CHKP 10
static struct dyn_con * dbadsort_chkp;
static char * sbadsort_chkp="select plan_id,plan_text from BADSORT_PLAN\n\
where hash_val=:hash_val";
#define CURS_BADSORT_CHKS 11
static struct dyn_con * dbadsort_chks;
static char * sbadsort_chks="select sql_id,cur_plan_id,sql_text from\n\
BADSORT_SQL where hash_val=:hash_val";
#define CURS_BADSORT_UPDSP 12
static struct dyn_con * dbadsort_updsp;
static char * sbadsort_updsp="update BADSORT_SQL set cur_plan_id=:plan_id\n\
where sql_id=:sql_id";
#define CURS_NEW_ID 13
static struct dyn_con * dnew_id;
static char * snew_id = "select badsort_seq.nextval from dual";
#define CURS_NEW_MODULE 14
static struct dyn_con * dnew_module;
static char * snew_module = "insert into BADSORT_MODULE(module_id, module_name)\n\
values (:module_id, :module_name)";
#define CURS_NEW_MAPPING 15
static struct dyn_con * dnew_mapping;
static char * snew_mapping = "insert into BADSORT_MAPPING(module_id, sql_id)\n\
values (:module_id, :sql_id)";
#define CURS_CLEAR_MAPPING 16
static struct dyn_con * dclear_mapping;
static char * sclear_mapping = "delete BADSORT_MAPPING where sql_id = :sql_id";
#define CURS_CHK_MODULE 17
static struct dyn_con * dchk_module;
static char * schk_module = "select module_id from BADSORT_MODULE\n\
where module_name = :module_name";
#define CURS_GET_DEFAULT 18
static struct dyn_con * dget_default;
static char * sget_default="select param_id,param_value from BADSORT_PARAMETER";
/****************************************************************************
 * Session manipulation statements
 */
#define CURS_TO_RULE 20
static struct dyn_con * dto_rule;
static char * sto_rule = "alter session set optimizer_goal=rule";
#define CURS_TO_CHOOSE 21
static struct dyn_con * dto_choose;
static char * sto_choose = "alter session set optimizer_goal=choose";
#define CURS_TO_FIRST 22
static struct dyn_con * dto_first;
static char * sto_first = "alter session set optimizer_goal=first_rows";
#define CURS_TO_ALL 23
static struct dyn_con * dto_all;
static char * sto_all = "alter session set optimizer_goal=all_rows";
#define CURS_TO_SYS 24
static struct dyn_con * dto_sys;
static char sto_sys[80];
#define CURS_TO_USER 25
static struct dyn_con * dto_user;
static char * sto_user = "alter session set current_schema=%s";
#define CURS_GET_USER 26
static struct dyn_con * dget_user;
static char * sget_user = "select name from sys.user$ where user#=:user_id";
/*
 * Make sure there is a suitable plan table
 */
static void make_bad_plan()
{
    curse_parse(con, &dcost, CURS_COST, "drop table badsort_plan_table");
    dcost->is_sel = 0;
    exec_dml(dcost);
#ifdef OR9
    curse_parse(con, &dcost, CURS_COST, "create table badsort_plan_table (\n\
        statement_id       varchar2(30),\
        plan_id            number,\
        timestamp          date,\
        remarks            varchar2(4000),\
        operation          varchar2(30),\
        options            varchar2(255),\
        object_node        varchar2(128),\
        object_owner       varchar2(30),\
        object_name        varchar2(30),\
        object_alias       varchar2(65),\
        object_instance    numeric,\
        object_type        varchar2(30),\
        optimizer          varchar2(255),\
        search_columns     number,\
        id                 numeric,\
        parent_id          numeric,\
        depth              numeric,\
        position           numeric,\
        cost               numeric,\
        cardinality        numeric,\
        bytes              numeric,\
        other_tag          varchar2(255),\
        partition_start    varchar2(255),\
        partition_stop     varchar2(255),\
        partition_id       numeric,\
        other              long,\
        distribution       varchar2(30),\
        cpu_cost           numeric,\
        io_cost            numeric,\
        temp_space         numeric,\
        access_predicates  varchar2(4000),\
        filter_predicates  varchar2(4000),\
        projection         varchar2(4000),\
        time               numeric,\
        qblock_name        varchar2(30)\
)");
#else
    curse_parse(con, &dcost, CURS_COST, "create table badsort_plan_table (\n\
        statement_id       varchar2(30),\n\
        timestamp          date,\n\
        remarks            varchar2(80),\n\
        operation          varchar2(30),\n\
        options            varchar2(30),\n\
        object_node        varchar2(128),\n\
        object_owner       varchar2(30),\n\
        object_name        varchar2(30),\n\
        object_instance    numeric,\n\
        object_type        varchar2(30),\n\
        optimizer          varchar2(255),\n\
        search_columns     numeric,\n\
        id                 numeric,\n\
        parent_id          numeric,\n\
        position           numeric,\n\
        cost               numeric,\n\
        cardinality        numeric,\n\
        bytes              numeric,\n\
        other_tag          varchar2(255),\n\
        partition_start    varchar2(255),\n\
        partition_stop     varchar2(255),\n\
        partition_id       numeric,\n\
        other              long,\n\
        distribution       varchar2(30))");
#endif
    dcost->is_sel = 0;
    exec_dml(dcost);
    curse_parse(con, &dcost, CURS_COST,
        "drop public synonym badsort_plan_table");
    dcost->is_sel = 0;
    exec_dml(dcost);
    curse_parse(con, &dcost, CURS_COST, 
        "create public synonym badsort_plan_table for badsort_plan_table");
    dcost->is_sel = 0;
    exec_dml(dcost);
    curse_parse(con, &dcost, CURS_COST, 
        "grant insert, update,delete,select on badsort_plan_table to public");
    dcost->is_sel = 0;
    exec_dml(dcost);
    return;
}
/****************************************************************************
 * Set up the SQL statements
 */
static void open_all_sql()
{
char * x, *x1;

    if (badsort_base.debug_level > 3)
        fputs("open_all_sql() called\n",perf_global.errout);
    set_def_binds(1,20);
    if (con != (struct sess_con *) NULL)
    {
        make_bad_plan();
        curse_parse(con, &dcost, CURS_COST, scost) ;
        dcost->is_sel = 1;
#ifdef OR9
        curse_parse(con, &dplan, CURS_PLAN, splan) ;
        dplan->is_sel = 1;
#endif
        curse_parse(con, &dget_user, CURS_GET_USER, sget_user) ;
        dget_user->is_sel = 1;
        x = get_collector();
        sprintf(sto_sys,sto_user,x);
        free(x);
        curse_parse(con, &dto_sys, CURS_TO_SYS, sto_sys) ;
        dto_sys->is_sel = 0;
        dto_user = dyn_init(con, CURS_TO_USER) ;
        dto_user->is_sel = 0;
    }
    if (con_explain != (struct sess_con *) NULL)
    {
        curse_parse(con_explain, &ddel, CURS_DEL, sdel) ;
        ddel->is_sel = 0;
        dexp = dyn_init(con_explain, CURS_EXP) ;
        dexp->is_sel = 0;
        curse_parse(con_explain, &dxplread, CURS_XPLREAD, sxplread) ;
        dxplread->is_sel = 1;
        if (badsort_base.optimiser_goals != (char *) NULL)
        {
            badsort_base.plan_test_cnt = 0;
            for (x = badsort_base.optimiser_goals; *x != '\0'; x++)
                if (islower(*x))
                    *x = toupper(*x);
                                     /* Convert lower case to upper case */
           if (badsort_base.debug_level)
               (void) fprintf(perf_global.errout, "Explain string: %s\n",
                                          badsort_base.optimiser_goals);
            for (x1 = x, x = badsort_base.optimiser_goals;
                    badsort_base.plan_test_cnt < 4 && x < x1;)
            {
                switch(*x)
                {
                case 'A':
                    if (((x + 8) == x1 || (x + 8 < x1 && *(x + 8) == ','))
                     && !strncmp(x,"ALL_ROWS",8))
                    {
                        curse_parse(con_explain, &dto_all, CURS_TO_ALL,
                                                  sto_all);
                        dto_all->is_sel = 0;
                        badsort_base.plan_dyn[badsort_base.plan_test_cnt]
                                = dto_all;
                        badsort_base.plan_test_cnt++;
                        if (badsort_base.debug_level)
                            (void) fputs("ALL_ROWS-based explain\n",
                                          perf_global.errout);
                        x += 8;
                    }
                    break;
                case 'C':
                    if (((x + 6) == x1 || (x + 6 < x1 && *(x + 6) == ','))
                     && !strncmp(x,"CHOOSE",6))
                    {
                        curse_parse(con_explain, &dto_choose, CURS_TO_CHOOSE,
                                                  sto_choose);
                        badsort_base.plan_dyn[badsort_base.plan_test_cnt]
                                = dto_choose;
                        dto_choose->is_sel = 0;
                        badsort_base.plan_test_cnt++;
                        x += 6;
                        if (badsort_base.debug_level)
                            (void) fputs("CHOOSE-based explain\n",
                                          perf_global.errout);
                    }
                    break;
                case 'F':
                    if (((x + 10) == x1 || (x + 10 < x1 && *(x + 10) == ','))
                     && !strncmp(x,"FIRST_ROWS",10))
                    {
                        curse_parse(con_explain, &dto_first, CURS_TO_FIRST,
                                                  sto_first);
                        dto_first->is_sel = 0;
                        badsort_base.plan_dyn[badsort_base.plan_test_cnt]
                                = dto_first;
                        badsort_base.plan_test_cnt++;
                        x += 10;
                        if (badsort_base.debug_level)
                            (void) fputs("FIRST_ROWS-based explain\n",
                                          perf_global.errout);
                    }
                    break;
                case 'R':
                    if (((x + 4) == x1 || (x + 4 < x1 && *(x + 4) == ','))
                     && !strncmp(x,"RULE",4))
                    {
                        curse_parse(con_explain, &dto_rule, CURS_TO_RULE,
                                                  sto_rule);
                        badsort_base.plan_dyn[badsort_base.plan_test_cnt]
                                = dto_rule;
                        dto_rule->is_sel = 0;
                        badsort_base.plan_test_cnt++;
                        x += 4;
                        if (badsort_base.debug_level)
                            (void) fputs("RULE-based explain\n",
                                          perf_global.errout);
                    }
                    break;
                default:
                    break;
                }
                while(x < x1 && *x != ',')
                    x++;
                if (*x == ',')
                    x++;
            }
        }
    }
    if (con_stats != (struct sess_con *) NULL)
    {
        curse_parse(con_stats, &dget_default, CURS_GET_DEFAULT,
            sget_default);
        dget_default->is_sel = 1;
        curse_parse(con_stats, &dbadsort_plan, CURS_BADSORT_PLAN,
            sbadsort_plan);
        dbadsort_plan->is_sel = 0;
        curse_parse(con_stats, &dbadsort_sql, CURS_BADSORT_SQL,
            sbadsort_sql);
        dbadsort_sql->is_sel = 0;
        curse_parse(con_stats, &dbadsort_index, CURS_BADSORT_INDEX,
            sbadsort_index);
        dbadsort_index->is_sel = 0;
        curse_parse(con_stats, &dbadsort_table, CURS_BADSORT_TABLE,
            sbadsort_table);
        dbadsort_table->is_sel = 0;
        curse_parse(con_stats, &dbadsort_usage, CURS_BADSORT_USAGE,
            sbadsort_usage);
        dbadsort_usage->is_sel = 0;
        curse_parse(con_stats, &dbadsort_chkp, CURS_BADSORT_CHKP,
            sbadsort_chkp);
        dbadsort_chkp->is_sel = 1;
        curse_parse(con_stats, &dbadsort_chks, CURS_BADSORT_CHKS,
            sbadsort_chks);
        dbadsort_chks->is_sel = 1;
        curse_parse(con_stats, &dbadsort_updsp, CURS_BADSORT_UPDSP,
            sbadsort_updsp);
        dbadsort_updsp->is_sel = 0;
        curse_parse(con_stats, &dnew_id, CURS_NEW_ID, snew_id);
        dnew_id->is_sel = 1;
        curse_parse(con_stats, &dnew_module, CURS_NEW_MODULE, snew_module);
        dnew_module->is_sel = 0;
        curse_parse(con_stats, &dnew_mapping, CURS_NEW_MAPPING, snew_mapping);
        dnew_mapping->is_sel = 0;
        curse_parse(con_stats, &dclear_mapping, CURS_CLEAR_MAPPING,
                    sclear_mapping);
        dclear_mapping->is_sel = 0;
        curse_parse(con_stats, &dchk_module, CURS_CHK_MODULE, schk_module);
        dchk_module->is_sel = 1;
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
    set_long(16384);
    dict = new_dict( 20);
    dict_add(dict,":disk_buf_ratio",ORA_NUMBER,  sizeof(double));
    dict_add(dict,":buf_cost",ORA_NUMBER,  sizeof(double));
    dict_add(dict, ":plan_id", ORA_INTEGER, sizeof(long));
    dict_add(dict, ":hash_val", ORA_INTEGER, sizeof(long));
    dict_add(dict, ":plan_text", ORA_LONG, 16384);
    dict_add(dict, ":sql_id", ORA_INTEGER, sizeof(long));
    dict_add(dict, ":osql_id", ORA_CHAR, 14);
    dict_add(dict, ":plan_hash_value", ORA_NUMBER, sizeof(double));
    dict_add(dict, ":child_number", ORA_INTEGER, sizeof(long));
    dict_add(dict, ":user_id", ORA_INTEGER, sizeof(long));
    dict_add(dict, ":execs", ORA_INTEGER, sizeof(long));
    dict_add(dict, ":disk_reads", ORA_INTEGER, sizeof(long));
    dict_add(dict, ":buffer_gets", ORA_INTEGER, sizeof(long));
    dict_add(dict, ":sql_text", ORA_LONG, 16384);
    dict_add(dict, ":index_name", ORA_CHAR, 65);
    dict_add(dict, ":table_name", ORA_CHAR, 65);
    dict_add(dict, ":end_dttm", ORA_CHAR, 21);
    dict_add(dict, ":known_time", ORA_INTEGER, sizeof(long));
    dict_add(dict, ":module_id", ORA_INTEGER, sizeof(long));
    dict_add(dict, ":module_name", ORA_CHAR, 250);
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
        longjmp(ora_fail,1);         /* Give up if unexpected ORACLE error */
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
char *x;

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
        if ((x = strchr(badsort_base.tbuf,'/')) != NULL)
            *x = '\0';
        dto_user->statement = badsort_base.tbuf;
        prep_dml(dto_user);
        exec_dml(dto_user);
    }
    if (con_explain->ret_status != 0)
        scarper(__FILE__,__LINE__,"Unexpected Error switching schema");
    return;
}
/*
 * Check the FIFO for something to do
 */
static int do_user_request()
{
char * so_far;
char * fifo_args[30];                           /* Dummy arguments to process */
char fifo_line[BUFSIZ];                         /* Dummy arguments to process */
int read_cnt;                                   /* Number of bytes read */
register char * x;
short int i;
struct stat stat_buf;

    if (badsort_base.debug_level > 3)
    {
        fputs("do_user_request()\n", perf_global.errout);
        fflush(perf_global.errout);
    }
    (void) getcwd(fifo_line,sizeof(fifo_line));
#ifdef SOLAR
    if ((perf_global.fifo_fd = fifo_accept(fifo_line,perf_global.listen_fd))
            < 0)
#else
#ifdef MINGW32
    if ((perf_global.fifo_fd = fifo_accept(badsort_base.fifo_name,
                  perf_global.listen_fd))
            < 0)
#else
    if (( perf_global.fifo_fd = fifo_open()) < 0)
#endif
#endif
    {
        alarm(0);
        rem_time();
        return 0;          /*  Get back to a state of readiness */
    }
/*
 * There is
 */
    alarm(0);
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
    fprintf(perf_global.errout,"fifo_line:\n%s\n",fifo_line);
    fflush(perf_global.errout);
#endif
#else
#ifdef MINGW32
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
    if (badsort_base.debug_level > 3)
    {
        fprintf(perf_global.errout,"fifo_line:\n%s\n",fifo_line);
        fflush(perf_global.errout);
    }
#else
       ;
#endif
#endif
#ifndef SOLAR
#ifndef MINGW32
    (void) fclose(perf_global.fifo);
#endif
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
    for (i=2;
             i < 30 && (fifo_args[i]=strtok(NULL," \n")) != (char *) NULL;
                 i++);
 
    fifo_args[0] = "";
    opterr=1;                /* turn off getopt() messages */
    optind=1;                /* reset to start of list */
    proc_args(i,fifo_args);
    rem_time();              /* Gets the clock running again if appropriate */
    return 1;
}
/*
 * Set up submission fifo
 */
static int setup_fifo()
{
#ifdef NEED_SECURITY
    (void) umask(07);        /* allow owner only to submit */
#else
    (void) umask(0);         /* allow anyone access */
#endif
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
#ifdef MINGW32
    if ((perf_global.listen_fd = fifo_listen(perf_global.fifo_name)) < 0)
    {
        char * x=
    "Failed to open the FIFO Named Pipe; aborting";
        (void) fprintf(perf_global.errout,"%s: %s\nError: %d\n",perf_global.fifo_name,x,errno);
        perror("Cannot Open FIFO");
        (void) unlink(perf_global.fifo_name);
        return 0;
    }
#else
    if ((perf_global.listen_fd = mkfifo(perf_global.fifo_name,0010660)) < 0)
    { /* create the input FIFO */
        char * x ="Failed to create the FIFO; aborting";
        (void) fprintf(perf_global.errout,"%s: %s\nError: %d\n",perf_global.fifo_name,x,errno);
        perror("Cannot Create FIFO");
        return 0;
    }
#endif
#endif
    return 1;
}
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
    exit(0);
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
    "-G","",           /* Optimiser goal settings for plans       */
    "-n","",           /* Explain ID                              */
    "-q","",0};        /* SQL Collection ID                       */
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
    if (badsort_base.plan_test_cnt > 0)
    {
        argv[i] = "-G";
        i++;
        argv[i] = badsort_base.optimiser_goals;
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
    argv[i] = (char *) NULL;
    execvp(argv[0],argv);
    exit(1);
    perror("execvp() failed");
    exit(0);
}
/*****************************************************************************
 *  Things that only need be done once per invocation
 */
void daemon_init()
{
char buf[128];
    (void) fputs("badsql.c - Performance Base SQL Capture Daemon\n", perf_global.errout);
    perf_global.retry_time = 300;        /* ORACLE login retry time   */
    perf_global.errout = stderr;
    perf_global.fifo_name = badsort_base.fifo_name;
    (void) unlink(perf_global.fifo_name);
    sprintf(buf,"%s.lck",perf_global.fifo_name);
    perf_global.lock_name = strdup(buf );
    (void) unlink(perf_global.lock_name);
/*
 * Forever; establish an ORACLE connexion, and process files
 */
    sigset(SIGBUS, restart);
    sigset(SIGPIPE, restart);
    sigset(SIGSEGV, restart);
    sigset(SIGUSR1, finish);
    sigset(SIGTERM, finish);
    load_dict();                /* Identify the bind variable types    */
    return;
}
/*
 * Things that need to be done at the start of each ORACLE session
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
#ifdef NT4
    if (badsort_base.uid == (char *) NULL || (*(badsort_base.uid) == '\0'))
    {
        if ((con = do_oracle_session(NULL, "ORACLE Collector Login"))
                 == (struct sess_con *) NULL)
        {
            (void) fputs( "Collector Database Connect Failed\n",
                           perf_global.errout);
            return 0;
        }
        badsort_base.in_con = con;
    }
    else
#endif
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
    if (badsort_base.debug_level && badsort_base.uid != (char *) NULL)
        (void) fprintf(perf_global.errout, "Base connexion using %s\n",
                          badsort_base.uid);
    if (badsort_base.uid_explain != (char *) NULL
     && (badsort_base.uid == (char *) NULL
       || strcmp(badsort_base.uid, badsort_base.uid_explain)))
    {
#ifdef NT4
        if (*(badsort_base.uid_explain) == '\0')
        {
            if ((con_explain = do_oracle_session(NULL, "ORACLE Explain Login"))
                 == (struct sess_con *) NULL)
            {
                (void) fputs( "Explain Database Connect Failed\n",
                               perf_global.errout);
                if (con != (struct sess_con *) NULL)
                {
                    dyn_disconnect(con);
                    con = (struct sess_con *) NULL;
                }
                return 0;
            }
        }
        else
#endif
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
            if (badsort_base.debug_level
             && badsort_base.uid_explain != (char *) NULL)
                (void) fprintf(perf_global.errout,"Explain connexion uses %s\n",
                          badsort_base.uid_explain);
        }
    }
    else
        con_explain = con;
    if (badsort_base.uid_stats != (char *) NULL)
    {
#ifdef NT4
        if (*(badsort_base.uid_stats) == '\0')
        {
            if ((con_explain = do_oracle_session(NULL, "ORACLE Explain Login"))
                 == (struct sess_con *) NULL)
            {
                (void) fputs( "Statistics Database Connect Failed\n",
                               perf_global.errout);
            }
        }
        else
#endif
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
            if (badsort_base.debug_level
             && badsort_base.uid_stats != (char *) NULL)
                (void) fprintf(perf_global.errout,
                               "Statistics connexion uses %s\n",
                                      badsort_base.uid_stats);
        }
    }
    if (con != (struct sess_con *) NULL
     && badsort_base.fifo_name != (char *) NULL)
        if (!setup_fifo())
            setup_fifo();          /* Retry if cannot create the fifo */
/*
 * Fail if no connexions to ORACLE at all
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
 * an ORACLE session terminated, but that does not work reliably. We therefore
 * restart always.
 */
void run_daemon()
{
static int rec_flag;
int i;
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
    if (!setjmp(ora_fail) && oracle_init())
    {
/*
 * Process user requests and timed events Until ORACLE fails
 */
        if (!setjmp(ora_fail))
        {
            do_one_pass();
            if (badsort_base.uid_stats != (char *) NULL)
            {
                do_export(badsort_base.out_name);   /* Save the data */
                if (badsort_base.dont_save)
                    restart();
            }
            for(;;)
                (void) do_user_request();
        }
        clearup();          /* Save the data         */
    }
#ifdef MINGW32
#ifdef VCC2003
    _sleep(perf_global.retry_time * SLEEP_FACTOR);
#else
    sleep(perf_global.retry_time * SLEEP_FACTOR);
#endif
#else
    for (i = perf_global.retry_time * SLEEP_FACTOR; (i = sleep(i)) != 0;)
    {
        perror("sleep() didn't sleep");
        fprintf(stderr, "Returned %d\n", i);
        fflush(stderr);
        if (i < 0)
            exit(1);
    }
#endif
/*
 * We do not appear to be able to re-login to ORACLE successfully
 * So we exec() another process.
 */
    restart();
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
/*
 * Break up the statement into separate lines:
 * - Do not split strings over lines.
 * - Inject line feeds when multiple spaces are not in a string and
 *   The multiples do not appear at the start of the line.
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
        for (i = 132, flag1=0, flag2 = 0; i; i--, x++)
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
 * injected a line feed) or we have gone 132 characters. In this case, if we
 * are in a string, we search forwards for its end, otherwise, we move x
 * backwards until it points to a space, or we reach the bounds of a string
 * (that is, x2)
 *
 * The long line length is in case the programmer was using the full screen
 * of a work station, and to try to avoid chopping early within a 
 * 'to the end of line' comment.
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
            else   /* The string goes past 132; search for its
                      end */
            {
                 for (x = x2 + 1;
                        x <= bound && *x != '\n' && *x != '\0';
                            x++)
                      if (*x == '\'')
                          if (x == bound || *(x+1) != '\'')
                              break;
                          else
                              x++;
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
    strcpy(sto_sys, "select user from sys.dual");
    curse_parse(con, &dto_sys, CURS_TO_SYS, sto_sys) ;
    dto_sys->is_sel = 1;
    exec_dml(dto_sys);
    dto_sys->so_far = 0;
    if (! dyn_locate(dto_sys, &len, &name_ret))
        longjmp(ora_fail,1);         /* Give up if unexpected ORACLE error */
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
        fputs("get_user_name()\n",perf_global.errout);
    add_bind(dget_user, E2FLONG, sizeof(long int), &user_id);
    exec_dml(dget_user);
    dget_user->so_far = 0;
    if (! dyn_locate(dget_user, &len, &name_ret))
        longjmp(ora_fail,1);         /* Give up if unexpected ORACLE error */
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
/*
 * Routine to see if an SQL statement is an insert, a select, a delete or an
 * update (in which case, it is explainable) or not
 */
static int check_explainable(x)
char * x;
{
    while (*x == ' ' || *x == '\t' || *x == '\r' || *x == '\n')
        x++;
    if (!strcasecmp(x,"insert")
     || !strcasecmp(x,"select")
     || !strcasecmp(x,"update")
     || !strcasecmp(x,"delete"))
        return 1;
    return 0;
}
/****************************************************************************
 * Clear out the plan table, explain the SQL statement, retrieve the
 * plan and store it away.
 * - badsort.tlook has the SQL statement to explain.
 * - badsort.tlook and tbuf are disposable.
 * To be able to investigate the different possibilities for different
 * optimiser settings, this code needs to work on:
 * - A vector of optimiser settings (eg. RULE, CHOOSE, FIRST_ROWS, ALL_ROWS)
 * - A vector of ORACLE connections (eg. Current Version 7, Test Version 8).
 */
static void find_the_plan(statp)
struct stat_con * statp;
{
char * p;
int i, l;
char * cur_pos;
struct plan_con * plan;

    if (badsort_base.debug_level > 3)
    {
        fputs("find_the_plan()called\n", perf_global.errout);
        fputs(badsort_base.tlook, perf_global.errout);
        fputc('\n', perf_global.errout);
    }
    if (con == con_explain && con_explain != (struct sess_con *) NULL)
    {
        if (statp->user_id == 0)
        {
            statp->cant_explain = 1;
/*            exec_dml(dto_sys); */
        }
        else
        {
            switch_schema(badsort_base.uid);
            exec_dml(ddel);                  /* Clear out the PLAN_TABLE     */
            cur_pos = get_user_name(statp->user_id); 
#ifdef STELLAR_ONLY
            if (cur_pos != NULL
              && *(cur_pos) == 'S'
              && *(cur_pos + 1) == 'T')
                switch_schema(cur_pos);
            else
                statp->cant_explain = 1;
#else
            switch_schema(cur_pos);
#endif
            free(cur_pos);
        }
    }
    strcpy(badsort_base.tbuf,sexp);
    strcat(badsort_base.tbuf, badsort_base.tlook);
    dexp->statement = badsort_base.tbuf;
    i = 0;
    if (statp->cant_explain != 1)
    do
    {
        if (i < badsort_base.plan_test_cnt)
            exec_dml(badsort_base.plan_dyn[i]);
        i++;
        prep_dml(dexp);
        exec_dml(dexp);                  /* Explain the statement        */
        if (dexp->con->ret_status != 0)
        {
            statp->cant_explain = 1;
            break;
        }
        switch_schema(badsort_base.uid);
        exec_dml(dxplread);              /* Read the explanation         */
        cur_pos = badsort_base.tlook;
        dxplread->so_far = 0;
        while (dyn_locate(dxplread,&l,&p))
        {
            if (badsort_base.debug_level > 3)
            {
                fwrite(p, sizeof(char), l, perf_global.errout);
                fputc('\n', perf_global.errout);
            }
            memcpy(cur_pos,p,l);
            for( cur_pos += l - 1; *cur_pos == ' '; cur_pos--);
            cur_pos++;
            *cur_pos = '\n';
            cur_pos++;
        }
        dyn_reset(dxplread);
        dbms_commit(con_explain);
        if (cur_pos != badsort_base.tlook)
        {
            cur_pos--;
            *cur_pos = '\0';
            plan = do_one_plan(badsort_base.tlook);
            if (statp->plan == (struct plan_con *) NULL)
                statp->plan = plan;
            else
            if (statp->plan != plan)
            {
                for (l = 0; l < statp->other_plan_cnt; l++)
                    if (statp->other_plan[l] == plan)
                        break;
                if (l >= statp->other_plan_cnt && l < 4)
                {
                    statp->other_plan[l] = plan;
                    statp->other_plan_cnt = l + 1;
                }
            }
            if (con_stats != (struct sess_con *) NULL
              && plan != (struct plan_con *) NULL)
                do_oracle_plan(plan);
        }
    }
    while (i < badsort_base.plan_test_cnt);
    return;
}
#ifdef OR9
static void find_or9_plan(statp)
struct stat_con * statp;
{
char * p[4];
int i, l[4];
char * cur_pos;
struct plan_con * plan;

    if (badsort_base.debug_level > 3)
    {
        fprintf(perf_global.errout, "find_or9_plan(%s) called\n",
                statp->osql_id);
        fputs(badsort_base.tlook, perf_global.errout);
        fputc('\n', perf_global.errout);
    }
    add_bind(dplan, FIELD, strlen(statp->osql_id), &(statp->osql_id[0]));
    exec_dml(dplan);
    dplan->so_far = 0;
    cur_pos = badsort_base.tlook;
    while (dyn_locate(dplan,&(l[0]),&(p[0])))
    {
        if (badsort_base.debug_level > 3)
        {
            fwrite(p[0], sizeof(char), l[0], perf_global.errout);
#ifdef OR10
            for (i = 1; i < 4; i++)
            {
                fputc('}', perf_global.errout);
                fwrite(p[i], sizeof(char), l[i], perf_global.errout);
            }
#endif
            fputc('\n', perf_global.errout);
        }
        if (((cur_pos + l[0] + 32) - badsort_base.tlook) > WORKSPACE)
        { /* The extra is allowing for the header on disk */
            fprintf(perf_global.errout, "find_or9_plan(%s) (%d + %d) out of space\n%.*s",
                    statp->osql_id, l[0], (cur_pos - badsort_base.tlook),
                                     (cur_pos - badsort_base.tlook),
                                     badsort_base.tlook);
            break;
        }
        memcpy(cur_pos, p[0], l[0]);
        for( cur_pos += l[0] - 1; *cur_pos == ' '; cur_pos--);
        cur_pos++;
#ifdef OR10
        for (i = 1; i < 4; i++)
        {
            if (((cur_pos + l[i] + 32) - badsort_base.tlook) > WORKSPACE)
            { /* The extra is allowing for the header on disk */
                fprintf(perf_global.errout, "find_or9_plan(%s) (%d + %d) out of space\n%.*s",
                    statp->osql_id, l[i], (cur_pos - badsort_base.tlook),
                                      (cur_pos - badsort_base.tlook),
                                     badsort_base.tlook);
                break;
            }
            *cur_pos = '}';
            memcpy(cur_pos, p[i], l[i]);
            cur_pos += l[i];
        }
#endif
        *cur_pos = '\n';
        cur_pos++;
    }
    dyn_reset(dplan);
    if (cur_pos != badsort_base.tlook)
    {
        cur_pos--;
        *cur_pos = '\0';
        plan = do_one_plan(badsort_base.tlook);
        if (statp->plan == (struct plan_con *) NULL)
            statp->plan = plan;
        else
        if (statp->plan != plan)
        {
            for (i = 0; i < statp->other_plan_cnt; i++)
                if (statp->other_plan[i] == plan)
                    break;
            if (i >= statp->other_plan_cnt && i < 4)
            {
                    statp->other_plan[i] = plan;
                    statp->other_plan_cnt = i + 1;
            }
        }
        if (con_stats != (struct sess_con *) NULL
          && plan != (struct plan_con *) NULL)
            do_oracle_plan(plan);
    }
    return;
}
#endif
/****************************************************************************
 * Process a whole statement found by do_one_pass
 */
static void do_one_state(cur_stat, cur_pos, command_type)
struct stat_con *cur_stat;
char * cur_pos;
int command_type;
{
struct stat_con  *statp;

    *cur_pos = '\0';
#ifdef OR9
    if (strstr(badsort_base.tbuf, "--") != NULL)
        strcpy( badsort_base.tlook,badsort_base.tbuf);
    else
#endif
        printifysql(cur_pos - badsort_base.tbuf,
                badsort_base.tbuf, badsort_base.tlook);
/*
 * Process this statement
 */
    statp = do_one_sql(badsort_base.tlook, &(cur_stat->total), 1);
#ifdef OR9
    strcpy(statp->osql_id, cur_stat->osql_id);
    statp->plan_hash_value = cur_stat->plan_hash_value;
#ifdef DONT_CARE_IT_COSTS_SO_MUCH
    if (statp->plan == (struct plan_con *) NULL
      && statp->cant_explain != 1
      && (command_type == 2 || command_type == 3
      || command_type == 6 || command_type == 7))
    {
        statp->user_id = cur_stat->user_id;
        find_or9_plan(statp);
    }
#endif
#endif
    if (statp->plan == (struct plan_con *) NULL
      && statp->cant_explain != 1
      && (command_type == 2 || command_type == 3
      || command_type == 6 || command_type == 7))
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
 * Make a pass through the stuff from the ORACLE kernel
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
 * The function returns normally unless an unexpected ORACLE error occurs, in
 * which case we assume that we need to log out, and log back in again.
 */
void do_one_pass()
{
static char * argv[] ={"","-1"}; 
static struct arg_block arg_block = {
2, argv};
char *p[18]; /* Must not be less than the select columns for statement scost */
int l[18];
struct stat_con cur_stat,
*statp = (struct stat_con *) NULL;
char last_addr[17];
char last_child[17];
int last_child_len;
int last_len;
char * cur_pos;
int command_type;

    sighold(SIGALRM);
    if (badsort_base.debug_level > 3)
        fputs("do_one_pass() called\n", perf_global.errout);
/*
 * Once each pass, check whatever is in the parameter table
 */
    if (con_stats != (struct sess_con *) NULL)
        do_oracle_parameter();
    memset((char *) &cur_stat,0,sizeof(cur_stat));
    cur_stat.total.t = time((time_t *) NULL);

    exec_dml(dto_sys);      /* Make sure collector id is our current schema */
    if (con->ret_status != 0 && con->ret_status != 1403)
        scarper(__FILE__,__LINE__,"Unexpected Error switching schema to SYS");
    add_bind(dcost, FNUMBER, sizeof(double), &(badsort_base.buf_cost));
#ifndef OR9
    add_bind(dcost, FNUMBER, sizeof(double), &(badsort_base.disk_buf_ratio));
#endif
    exec_dml(dcost);
    if (con->ret_status != 0 && con->ret_status != 1403)
    {
        scarper(__FILE__,__LINE__,"Unexpected Error fetching bad statements");
        longjmp(ora_fail,1);         /* Give up if unexpected ORACLE error */
    }
    memset(&last_addr[0],0,sizeof(last_addr));
    last_len = 0;
    last_child_len = -1;
    dcost->so_far = 0;
    while (dyn_locate(dcost,&(l[0]),&(p[0])))
    {

        if (last_len != l[0]
          || memcmp(p[0], &last_addr[0], last_len)
          || last_child_len != l[10]
          || memcmp(p[10], &last_child[0], last_child_len))
        {
/*
 * Process this statement
 */
            if (last_len != 0)
            {
/*
 * Deal with problems due to multiple parsing ID's. We will see if multiple
 * plans for the same statement will also give us a problem. We don't know
 * if the plan hash value is invariant for a given plan.
 */
#ifdef OR9
                cur_pos += sprintf(cur_pos - 1, " /* %.*s:%.*s:%.*s */\n",
                        l[5], p[5], l[7], p[7], l[10], p[10]);
#else
                cur_pos += sprintf(cur_pos - 1, " /* %.*s */\n",
                        l[5], p[5]);
#endif
                do_one_state(&cur_stat, cur_pos, command_type);
            }
            last_len = l[0];
            last_child_len = l[10];
            memcpy(&cur_stat.osql_id[0], p[0], last_len);
            cur_stat.osql_id[last_len] = '\0';
            memcpy(&last_addr[0], p[0], last_len);
            memcpy(&last_child[0], p[10], last_child_len);
            cur_pos = badsort_base.tbuf;
            cur_stat.total.executions = strtod(p[1], (char **) NULL);
            cur_stat.total.disk_reads = strtod(p[2], (char **) NULL);
            cur_stat.total.buffer_gets = strtod(p[3], (char **) NULL);
            cur_stat.user_id = atol(p[5]);
            cur_stat.child_number = atol(p[10]);
            command_type = atol(p[6]);
#ifdef OR9
            cur_stat.plan_hash_value = strtod(p[7], (char **) NULL);
            cur_stat.total.cpu = strtod(p[8], (char **) NULL)/1000000.0;
#ifdef OR10
            cur_stat.total.elapsed = strtod(p[9], (char **) NULL)/1000000.0;
            cur_stat.total.direct_writes = strtod(p[11], (char **) NULL);
            cur_stat.total.application_wait_time = strtod(p[12], (char **) NULL)/1000000.0;
            cur_stat.total.concurrency_wait_time = strtod(p[13], (char **) NULL)/1000000.0;
            cur_stat.total.cluster_wait_time = strtod(p[14], (char **) NULL)/1000000.0;
            cur_stat.total.user_io_wait_time = strtod(p[15], (char **) NULL)/1000000.0;
            cur_stat.total.plsql_exec_time = strtod(p[16], (char **) NULL)/1000000.0;
            cur_stat.total.java_exec_time = strtod(p[17], (char **) NULL)/1000000.0;
#endif
#endif
        }
        if (badsort_base.debug_level > 3)
        {
        int hsh;

            memcpy((char *) &hsh, p[0], sizeof(int));
            fflush(stdout);
            fprintf(perf_global.errout,"%*.*s:%x:%*.*s\n",
                  l[5], l[5], p[5], hsh, l[4], l[4], p[4]);
            fflush(perf_global.errout);
        }
        if (cur_pos + l[4] >= badsort_base.tbuf + WORKSPACE)
        {
/*            fprintf(perf_global.errout,"Out of space after:%.*s\n",
 *                (cur_pos - badsort_base.tbuf), badsort_base.tbuf);
 */
            fprintf(perf_global.errout,"Out of space dropped:%.*s\n",
                    l[4], p[4]);
        }
        else
        {
            memcpy(cur_pos, p[4], l[4]);
            cur_pos += l[4];
        }
    }
/*
 * Process the last statement
 */
    if (last_len != 0)
        do_one_state(&cur_stat, cur_pos, command_type);
    dyn_reset(dcost);
    if (badsort_base.uid_stats == (char *) NULL)
    {
        do_export(badsort_base.out_name);   /* Save the data */
        if (badsort_base.dont_save)
            restart();
    }
    exec_dml(dto_sys);
    sigrelse(SIGALRM);
    add_time(&arg_block,badsort_base.retry_time);   /* Schedule another     */
    return;
}
/***************************************************************************
 * Clock functions
 *
 * add_time();  add a future event
 * - This function moves the buffer head, but not its tail.
 * - If it bumps into the head, it does not add any more, on the grounds that
 *   there will be other opportunities to add things.
 */
void add_time(abp,delta)
struct arg_block * abp;
int delta;
{
short int cur_ind;
short int next_ind;
time_t t;
struct go_time sav_time;
time_t new_time;

    if (badsort_base.debug_level > 3)
    {
        fputs("add_time() called\n",perf_global.errout);
        fflush(perf_global.errout);
    }
    t = (time_t) time((time_t *) 0);
    new_time = t + delta;
    if (badsort_base.debug_level > 1)
    {
        (void) fprintf(perf_global.errout,"add_time(): delta %d\n", delta);
        dump_args(abp->argc, abp->argv);
    }
    sighold(SIGALRM);
/*
 * Find the correct place to insert, and get rid of any duplicates before it.
 * Later duplicates don't matter; they will be cleared out later.
 */
    for (cur_ind = tail, next_ind = cur_ind;
        ;
        cur_ind = (cur_ind + 1) % (sizeof(go_time)/sizeof(struct go_time)),
        next_ind = (next_ind + 1) % (sizeof(go_time)/sizeof(struct go_time)))
    {
         while (next_ind != head && go_time[next_ind].abp == abp)
                            /* Match, increment skip count */
             next_ind = (next_ind + 1) %
                           (sizeof(go_time)/sizeof(struct go_time));
         if (next_ind == head)
         {               /* Run out, break */
             head = cur_ind;
             break;
         }
         if (next_ind != cur_ind)
         {                   /* Are we skipping? */
             go_time[cur_ind] = go_time[next_ind]; /* Pull down the next */
         }
         if ( go_time[cur_ind].go_time >= new_time)
             break;
    }
    if (next_ind == (cur_ind + 1) % (sizeof(go_time)/sizeof(struct go_time)))
    {  /* The easy case; we just slot it in. Note that either next_ind and
          cur_ind entries are the same, or they do not matter */
        go_time[cur_ind].abp = abp;
        go_time[cur_ind].go_time = new_time;
        if (head == cur_ind)
            head = next_ind;
    }
    else
    if (cur_ind == head || next_ind == cur_ind)
    {        /* Need to shift up */
        for (; cur_ind != head && go_time[cur_ind].abp != abp;
             cur_ind = (cur_ind + 1) % (sizeof(go_time)/sizeof(struct go_time)))
        {
            sav_time = go_time[cur_ind];
            go_time[cur_ind].go_time = new_time;
            go_time[cur_ind].abp = abp;
            new_time = sav_time.go_time;
            abp = sav_time.abp;
        }
        if ( head == cur_ind
          && tail != (head + 1) % (sizeof(go_time)/sizeof(struct go_time)))
        {
            go_time[head].go_time = new_time;
            go_time[head].abp = abp;
            head = (head + 1) % (sizeof(go_time)/sizeof(struct go_time));
        }
    }
    else
    {    /* Need to shift down */
        go_time[(cur_ind + 1) % (sizeof(go_time)/sizeof(struct go_time))] =
                go_time[cur_ind];
        go_time[cur_ind].abp = abp;
        go_time[cur_ind].go_time = new_time;
        cur_ind = (cur_ind + 1) % (sizeof(go_time)/sizeof(struct go_time));
        for (; next_ind != head ;
             cur_ind = (cur_ind + 1) %
                        (sizeof(go_time)/sizeof(struct go_time)),
             next_ind = (next_ind + 1) %
                        (sizeof(go_time)/sizeof(struct go_time)))
             go_time[cur_ind] = go_time[next_ind]; /* Pull down the next */
        head = cur_ind;
    }
    if (clock_running != 0)
        alarm(0);
    sigrelse(SIGALRM);
    rem_time();
    return;
}
/*
 * rem_time(); tidy up the list, removing times from the tail as they
 * expire.
 *
 * This function is NEVER called if the clock is running, and is not
 * called recursively, since a single call should be able to loop through
 * everything outstanding.
 *
 * This routine is the main scheduler.
 *
 * There may be an issue with the argument blocks; how can they be freed? We
 * can think about upgrading our malloc() to hold a reference count.
 */
void rem_time()
{
static int rec_call;
int sleep_int;
time_t cur_time;

    if (badsort_base.debug_level > 3)
    {
        fputs("rem_time() called\n",perf_global.errout);
        fflush(perf_global.errout);
    }
    if (rec_call >= 1)
        return;
    else
       rec_call++;
    clock_running = 0;
    cur_time = (time_t) time((time_t *) 0);
    if (badsort_base.debug_level > 1)
        (void) fprintf(perf_global.errout,"rem_time(): Clock Running %d\n",clock_running);
    while ( tail !=head && go_time[tail].go_time <= cur_time )
    {                      /* Loop - process any PIDs due an inspection */
    struct go_time this_time;

        this_time = go_time[tail];
        tail = (tail + 1) % (sizeof(go_time)/sizeof(struct go_time));
        opterr=1;                /* turn off getopt() messages */
        optind=1;                /* reset to start of list */
        proc_args(this_time.abp->argc, this_time.abp->argv);
    }
    if (tail != head)
    {
        (void) sigset(SIGALRM,rem_time);
        sleep_int = go_time[tail].go_time - cur_time;
        clock_running ++;
        (void) alarm(sleep_int);
    }
    rec_call--;
    return;
} 
/*
 * Reset the time buffers
 */
static void reset_time()
{
    alarm(0);
    sigset(SIGALRM,SIG_IGN);
    tail = head;
    clock_running = 0;
    return;
}
/*
 * Dump out the timed event control list.
 */
static void dump_time()
{
short int cur_ind;
time_t t;

    t = (time_t) time((time_t *) 0);
    sighold(SIGALRM);
    fputs("Time Control\n", perf_global.errout);
    fputs("============\n", perf_global.errout);
    fprintf(perf_global.errout,"clock_running: %d array: %d tail: %d head: %d now: %u\n",
             clock_running, sizeof(go_time)/sizeof(struct go_time),
                    tail, head, t);
    fputs(
    "==================================================================\n",
          perf_global.errout);
    fputs("PID         Next Due\n", perf_global.errout);
    fputs("========    ========\n", perf_global.errout);
    for (cur_ind = tail; cur_ind != head; cur_ind = (cur_ind + 1) % 
          (sizeof(go_time)/sizeof(struct go_time)))
    {
        fprintf(perf_global.errout,"%u ==>\n",go_time[cur_ind].go_time);
        dump_args(go_time[cur_ind].abp->argc, go_time[cur_ind].abp->argv);
    }
    sigrelse(SIGALRM);
    return;
} 
/*
 * read_timeout(); interrupt a network read that is taking too long
 */
static void read_timeout()
{
    return;
}
/*
 * Routine to temporarily pre-empt the normal clock handling
 */
void alarm_preempt()
{
    prev_alrm = sigset(SIGALRM,read_timeout);
    alarm_save = alarm(poll_wait.tv_sec);
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
/*****************************************************************************
 * Handle unexpected errors
 */
extern int errno;
void scarper(file_name,line,message)
char * file_name;
int line;
char * message;
{
    fflush(stdout);
    fflush(stderr);
    (void) fprintf(perf_global.errout,"Unexpected Error %s,line %d\n",
                   file_name,line);
    if (errno != 0)
    {
        perror(message);
        fflush(stderr);
        (void) fprintf(perf_global.errout,"UNIX Error Code %d\n", errno);
    }
    else
        fputs(message,perf_global.errout);
    if (con != (struct sess_con *) NULL && con->ret_status != 0)
        dbms_error(con);
    if (con_explain != (struct sess_con *) NULL && con_explain->ret_status != 0)
        dbms_error(con_explain);
    if (con_stats != (struct sess_con *) NULL && con_stats->ret_status != 0)
        dbms_error(con_stats);
    fflush(stderr);
    fflush(perf_global.errout);
    return;
}
/****************************************************************************
 * Functions that update the ORACLE statistics.
 *
 * Get the next ID (Plan or SQL or Module).
 */
int get_new_id( )
{
int len;
char * num_ret;
    if (badsort_base.debug_level > 3)
        fputs("get_new_id()\n",perf_global.errout);
    exec_dml(dnew_id);
    dnew_id->so_far = 0;
    if (! dyn_locate(dnew_id, &len, &num_ret))
        longjmp(ora_fail,1);         /* Give up if unexpected ORACLE error */
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
        planp->plan_id = get_new_id();
        add_bind(dbadsort_plan, E2FLONG, sizeof(int), &(planp->plan_id));
        add_bind(dbadsort_plan, E2FLONG, sizeof(int), &(planp->hash_val));
        add_bind(dbadsort_plan, FIELD, len, planp->plan);
        exec_dml(dbadsort_plan);
/*
 * - Process any Indexes and Tables for this plan
 *   - The logic to find them is in do_ind_tab(), but this will need to
 *     be changed so that it distinguishes between ORACLE and ACCESS output
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
int len = strlen(statp->sql);
    if (con_stats == (struct sess_con *) NULL)
        return;
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
            statp->sql_id = get_new_id();
            add_bind(dbadsort_sql, E2FLONG, sizeof(int), &(statp->sql_id));
            add_bind(dbadsort_sql, E2FLONG, sizeof(int), &(statp->hash_val));
            add_bind(dbadsort_sql, E2FLONG, ((plan_id)?sizeof(int):0),
                         &(plan_id));
            add_bind(dbadsort_sql, FIELD, len, statp->sql);
            exec_dml(dbadsort_sql);
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
            pc->module_id = get_new_id();
            add_bind(dnew_module, E2FLONG, sizeof(int), &(pc->module_id));
            add_bind(dnew_module, FIELD, strlen(pc->name), pc->name);
            exec_dml(dnew_module);
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
 * Initialise the tables used by the ORACLE badsort implementation
 */
static char *badsort_ddl[] = {
"create sequence BADSORT_SEQ",
"create table BADSORT_PLAN (\n\
    PLAN_ID number not null,\n\
    HASH_VAL number not null,\n\
    PLAN_TEXT long not null,\n\
constraint BADSORT_PLAN_PK primary key (PLAN_ID))",
"create index BADSORT_PLAN_I1 on BADSORT_PLAN(HASH_VAL)",
"create table BADSORT_SQL (\n\
    SQL_ID number not null,\n\
    HASH_VAL number not null,\n\
    CUR_PLAN_ID number,\n\
    SQL_TEXT long not null,\n\
constraint BADSORT_SQL_PK primary key (SQL_ID),\n\
constraint BADSORT_PLAN_FK\n\
    foreign key (CUR_PLAN_ID) references BADSORT_PLAN(PLAN_ID) disable)",
"create index BADSORT_SQL_I1 on BADSORT_SQL(HASH_VAL)",
"create table BADSORT_INDEX (\n\
    PLAN_ID number not null,\n\
    INDEX_NAME varchar2(64),\n\
constraint BADSORT_INDEX_PK primary key(PLAN_ID,INDEX_NAME),\n\
constraint BADSORT_INDEX_PLAN_FK foreign key (PLAN_ID)\n\
     references BADSORT_PLAN(PLAN_ID) disable)",
"create unique index BADSORT_INDEX_UI1 on BADSORT_INDEX(INDEX_NAME,PLAN_ID)",
"create table BADSORT_TABLE(\n\
    PLAN_ID number not null,\n\
    TABLE_NAME varchar2(64) not null,\n\
constraint BADSORT_TABLE_PK primary key(PLAN_ID,TABLE_NAME),\n\
constraint BADSORT_TABLE_PLAN_FK foreign key  (PLAN_ID)\n\
    references BADSORT_PLAN(PLAN_ID) disable)",
"create unique index BADSORT_TABLE_UI1 on BADSORT_TABLE(TABLE_NAME,PLAN_ID)",
"create table BADSORT_MODULE(\n\
    MODULE_ID number not null,\n\
    MODULE_NAME varchar2(250) not null,\n\
constraint BADSORT_MODULE_PK primary key(MODULE_ID),\n\
constraint BADSORT_MODULE_U1 unique  (MODULE_NAME))",
"create table BADSORT_USAGE(\n\
    SQL_ID number not null,\n\
    END_DTTM date not null,\n\
    EXECS number not null,\n\
    DISK_READS number not null,\n\
    BUFFER_GETS number not null,\n\
    PLAN_ID number,\n\
    KNOWN_TIME number,\n\
constraint BADSORT_USAGE_UK unique (SQL_ID,END_DTTM,PLAN_ID),\n\
constraint BADSORT_USAGE_SQL_FK foreign key (SQL_ID)\n\
    references BADSORT_SQL(SQL_ID) disable,\n\
constraint BADSORT_USAGE_PLAN_FK foreign key (PLAN_ID)\n\
    references BADSORT_PLAN(PLAN_ID) disable)",
"create unique index BADSORT_USAGE_UI1 on BADSORT_USAGE(PLAN_ID,END_DTTM,SQL_ID)",
"create unique index BADSORT_USAGE_UI2 on BADSORT_USAGE(END_DTTM,SQL_ID,PLAN_ID)",
"create table BADSORT_MAPPING(\n\
    MODULE_ID number not null,\n\
    SQL_ID number not null,\n\
constraint BADSORT_MAPPING_PK primary key(MODULE_ID, SQL_ID),\n\
constraint BADSORT_MAPPING_SQL_FK foreign key (SQL_ID)\n\
    references BADSORT_SQL(SQL_ID) disable,\n\
constraint BADSORT_MAPPING_MODULE_FK foreign key (MODULE_ID)\n\
    references BADSORT_MODULE(MODULE_ID) disable)",
"create unique index BADSORT_MAPPING_UI1 on BADSORT_MAPPING(SQL_ID,MODULE_ID)",
NULL };
/*
 * Create the badsort tables in an ORACLE database
 */
void do_oracle_tables()
{
char **x;
    load_dict();
    if (!setjmp(ora_fail) && oracle_init())
    {
        for (x = badsort_ddl; *x != (char *) NULL; x++)
        {
            curse_parse(con, &dcost, CURS_COST, *x);
            dcost->is_sel = 1;
            exec_dml(dcost);
        }
        oracle_cleanup();
    }
    return;
}
/*
 * Load up a badsort file into an ORACLE database. Used for loading foreign
 * data.
 */
void do_oracle_imp()
{
struct stat_con *statp;

    load_dict();
    if (!setjmp(ora_fail) && oracle_init())
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
    return;
}
/*
 * Find out what the plans would be for a collection of SQL statements.
 */
void do_all_plans()
{
struct stat_con *statp;

    load_dict();
    if (!setjmp(ora_fail) && oracle_init())
    {
        for ( statp = badsort_base.anchor;
                statp != (struct stat_con *) NULL ;
                    statp = statp->next)
        {
            if (check_explainable(statp->sql))
            {
                strcpy(badsort_base.tlook,statp->sql);
                find_the_plan(statp);
            }
            else
                statp->cant_explain = 1;
        }
        oracle_cleanup();
    }
    return;
}
/*
 * Commands to initialise a badsort working directory
 */ 
static char * res_words[] = { "ABORT", "ABS", "ACCEPT", "ACCESS", "ADD",
"ADD_MONTHS", "ALL", "ALL_ROWS", "ALTER", "ALWAYS", "ANALYZE", "ANALYZED",
"AND", "ANY", "ARCHIVELOG", "ARRAY", "ARRAYLEN", "AS", "ASC", "ASCII",
"ASSERT", "ASSIGN", "AT", "AUDIT", "AUTHORIZATION", "AUTOEXTEND", "AUTOMATIC",
"AVG", "BACKUP", "BACKUPS", "BASE_TABLE", "BEGIN", "BETWEEN", "BINARY_INTEGER",
"BODY", "BOOLEAN", "BY", "CASE", "CEIL", "CHAR", "CHARTOROWID", "CHAR_BASE",
"CHECK", "CHOOSE", "CHR", "CLOSE", "CLOSE_CACHED_OPEN_CURSORS", "CLUSTER",
"CLUSTERS", "COLAUTH", "COLUMN", "COLUMNS", "COMMENT", "COMMIT",
"COMMIT_POINT_STRENGTH", "COMPILE", "COMPRESS", "COMPUTE", "CONCAT", "CONNECT",
"CONST", "CONSTANT", "CONSTRAINT", "CONSTRAINTS", "CONTAINS", "CONTINUE",
"CONVERT", "COS", "COSH", "COUNT", "CRASH", "CREATE", "CURRENT", "CURRVAL",
"CURSOR", "DATABASE", "DATAFILE", "DATAFILES", "DATA_BASE", "DATE", "DBA", "DD",
"DEBUGOFF", "DEBUGON", "DECIMAL", "DECLARE", "DECODE", "DEFAULT", "DEFINE",
"DEFINITION", "DELAY", "DELETE", "DELTA", "DESC", "DESCRIBE", "DIGITS",
"DISPOSE", "DISTINCT", "DO", "DOUBLE", "DROP", "DUAL", "DUMP", "EACH", "EITHER",
"ELEMENT", "ELSE", "ELSIF", "ENABLE", "END", "ENTRY", "ERROR", "ERRORSTACK",
"ESCAPE", "ESTIMATE", "EVENT", "EVENT#", "EVENTS", "EVERYTHING", "EWORD",
"EXCEPT", "EXCEPTIONS", "EXCEPTION_INIT", "EXCLUSIVE", "EXEC", "EXECUTE",
"EXISTS", "EXIT", "EXP", "EXPLAIN", "EXTEND", "EXTENDS", "EXTENT",
"EXTENTS", "EXTERNAL", "EXTERNALLY", "FALSE", "FAST", "FATAL", "FETCH", "FILE",
"FILENAME", "FILES", "FILTER", "FIRST", "FLAG", "FLAGGER", "FLOAT", "FLOOR",
"FOR", "FORCE", "FOREIGN", "FOREVER", "FORM", "FRAGMENT", "FREE", "FREELIST",
"FREELISTS", "FROM", "FULL", "FUNC", "FUNCTION", "GENERAL", "GENERIC", "GET",
"GETHITRATIO", "GETHITS", "GETMISSES", "GETS", "GLB", "GLOBAL", "GOTO", "GRANT",
"GREATEST", "GREATEST_LB", "GROUP", "HASH", "HASHED", "HAVE", "HAVING",
"HEXTORAW", "HH24", "IDENTIFIED", "IF", "IMMEDIATE", "IN", "INCREMENT", "INDEX",
"INDEXES", "INDICATOR", "INITCAP", "INITIAL", "INITRANS", "INSERT", "INSTANCE",
"INSTR", "INSTRB", "INTEGER", "INTERSECT", "INTERSECTION", "INTO", "INVALID",
"IS", "JOIN", "JULIAN", "KEEP", "KEPT", "KEY", "KILL", "LABEL", "LAST_DAY",
"LEAST", "LEAST_UB", "LENGTH", "LENGTHB", "LEVEL", "LIKE", "LIMITED", "LINK",
"LN", "LOCK", "LOG", "LOGFILE", "LONG", "LOOP", "LOWER", "LPAD", "LTRIM", "LUB",
"MAX", "MAXARCHLOGS", "MAXDATAFILES", "MAXEXTENTS", "MAXIMUM",
"MAXIMUM_CONNECTIONS", "MAXINSTANCES", "MAXLOGFILES", "MAXLOGHISTORY",
"MAXLOGMEMBERS", "MAXSIZE", "MAXTRANS", "MAXVALUE", "MAX_VALUE", "MEMBER",
"MERGE", "MERGE_AJ", "MESSAGE", "MI", "MIN", "MINEXTENTS", "MINUS", "MINVALUE",
"MIN_VALUE", "MLSLABEL", "MOD", "MODE", "MODIFY", "MODULE", "MON", "MONITOR",
"MONTHS_BETWEEN", "MOUNT", "NAME", "NAMESPACE", "NATURAL", "NESTED", "NEW",
"NEW_TIME", "NEXT", "NEXTVAL", "NEXTVALUE", "NEXT_DAY", "NLS", "NLSSORT",
"NOARCHIVELOG", "NOAUDIT", "NOCACHE", "NOCOMPRESS", "NOCYCLE", "NOFORCE",
"NOMAXVALUE", "NOMINVALUE", "NOMOUNT", "NONE", "NOORDER", "NOOVERRIDE",
"NOPARALLEL", "NOPARALLELISM", "NOPREEMPT", "NORESETLOGS", "NOREVERSE",
"NORMAL", "NOSORT", "NOT", "NOTHING", "NOUNDO", "NOWAIT", "NO_EXPAND",
"NO_MERGE", "NULL", "NUMBER", "NUMBER_BASE", "NUMERIC", "NUM_ROWS", "NVL",
"OBJECT", "OCCURRED", "OCCURRENCES", "OF", "OFFLINE", "ON", "ONLINE", "OPEN",
"OPTIMIZER", "OPTION", "OPTIONS", "OR", "ORDER", "OTHERS", "OUT", "OUTER",
"OWN", "OWNED", "OWNER", "PACKAGE", "PARALLEL", "PARAMETER", "PARTITION",
"PARTITIONS", "PATH", "PCM", "PCTFREE", "PCTINCREASE", "PCTUSED", "PENDING",
"PERCENT", "PERMANENT", "PIECE", "PLAN", "POSITIVE", "POWER", "PRAGMA",
"PRECISION", "PRIMARY", "PRIOR", "PRIRO", "PRIVATE", "PRIVILEGE", "PRIV_NUMBER",
"PROCEDURE", "PROCESS", "PROFILE", "PROGRAM", "PROTOCOL", "PUBLIC", "QUERY",
"QUEUE", "QUEUED", "QUIT", "QUOTA", "RAISE", "RANGE", "RAW", "RAWTOHEX",
"RAWTOLAB", "RDBMS", "READ", "REAL", "REASON", "RECEIVE", "RECORD", "RECOVER",
"RECOVERY", "RECURSIVE", "REDO", "REF", "REFRESH", "RELEASE", "REMOTE",
"REMOTE_DEPENDENCIES_MODE", "REMR", "RENAME", "REPLACE", "REPLY", "REQUEST",
"RESETLOGS", "RESOURCE", "RESOURCE_LIMIT", "RESTRICTED", "RESUME", "RETURN",
"RETURNCODE", "REUSE", "REVERSAL", "REVERSE", "REVOKE", "ROLE", "ROLLBACK",
"ROUND", "ROW", "ROWIDTOCHAR", "ROWLABEL", "ROWNUM", "ROWTYPE", "ROW_LOCKING",
"RPAD", "RTRIM", "RUN", "SAMPLE", "SAVE", "SAVEDATA", "SAVEPOINT", "SCAN",
"SCHEMA", "SEGMENT", "SELECT", "SEPARATE", "SEQUENCE", "SERIALIZABLE",
"SERVER_TYPE", "SERVICE", "SESSION", "SESSIONID", "SESSIONS", "SET", "SHARE",
"SHORT", "SHRINK", "SHUTDOWN", "SIGN", "SIN", "SINH", "SIZE", "SMALLINT",
"SNAPSHOT", "SORT", "SOUNDEX", "SOURCE", "SPACE", "SPLIT", "SPOOL", "SQL",
"SQLCODE", "SQLERRM", "SQRT", "SS", "START", "STATEMENT", "STATEMENT_ID",
"STATISTIC", "STATUS", "STDDEV", "STOP", "SUBSTR", "SUBSTRB", "SUBTYPE", "SUM",
"SUSPEND", "SWITCH", "SYNONYM", "SYSDATE", "TABAUTH", "TABLE", "TABLES",
"TABLESPACE", "TAN", "TANH", "TASK", "TEMPORARY", "TERMINATE", "THEN", "THESE",
"THIS", "THREAD", "TO", "TO_CHAR", "TO_DATE", "TO_LABEL", "TO_MULTI_BYTE",
"TO_NUMBER", "TO_SINGLE_BYTE", "TRACE", "TRANSACTION", "TRANSLATE", "TRIGGER",
"TRUE", "TRUNC", "TRUNCATE", "TYPE", "UID", "UNION", "UNIQUE", "UNLIMITED",
"UNSIGNED", "UNTIL", "UPDATABLE", "UPDATE", "UPPER", "USAGE", "USE", "USED",
"USER", "USERENV", "USERID", "USERNAME", "USE_ANTI", "USE_CONCAT", "USE_HASH",
"USE_MERGE", "USE_NL", "USING", "VALIDATE", "VALUES", "VARCHAR", "VARCHAR2",
"VARIANCE", "VIEW", "VIEWS", "VSIZE", "WHEN", "WHENEVER", "WHERE", "WHILE",
"WITH", "WORK", "WRITE", "XOR", "YY", "YYYY", NULL };

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
    if (!setjmp(ora_fail) && oracle_init())
    {
#ifdef NOPRIV
        curse_parse(con, &dcost, CURS_COST, "select object_name from dba_objects\n\
union select column_name from dba_tab_columns\n\
union select username from dba_users");
#else
#ifdef OR9
        curse_parse(con, &dcost, CURS_COST, "select name from sys.obj$\n\
union select name from sys.col$\n\
union select name from sys.user$");
#else
        curse_parse(con, &dcost, CURS_COST, "select name from sys.obj$\n\
union select name from sys.col$\n\
union select name from sys.link$\n\
union select name from sys.user$");
#endif
#endif
        dcost->is_sel = 1;
        exec_dml(dcost);
        dcost->so_far = 0;
        nfp = fopen(searchterms, "ab");
        while (dyn_locate(dcost,&l,&p))
             fprintf(nfp, "%-.*s\n", l, p); 
        dyn_reset(dcost);
        (void) fclose(nfp);
        oracle_cleanup();
    }
    return;
}
