/************************************************************************
 * e2syblib.c - SYBASE support routines for e2sqllib.c
 *
 * The idea is that we can plug in anything here.
 */
static char * sccs_id =  "@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1993\n";
#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef NT4
#include <unistd.h>
#endif
#include <string.h>
#include <ctype.h>

#ifdef AIX
#include <memory.h>
#endif
#include "tabdiff.h"
extern char * nextasc();
static struct sess_con * curr_con; /* For the benefit of the server callback */
static void syb_datafmt_dump(f)
CS_DATAFMT *f;
{
    fprintf(stderr,"CS_DATAFMT:%s\n\
datatype:%d maxlength:%d status:%d count:%d precision:%d scale:%d\n", f->name, f->datatype,
         f->maxlength, f->status, f->count, f->precision, f->scale);
    fflush(stderr);
    return;
}
/************************************************************************
 * Statement Preparation
 */
int dbms_parse(dyn)
struct dyn_con * dyn;
{
    curr_con = dyn->con;
    (void) ct_cancel(NULL, dyn->cmd, CS_CANCEL_ALL);
    dyn->res_type = 0;
    if ((dyn->ret_status = ct_command(dyn->cmd, CS_LANG_CMD,
              dyn->statement, CS_NULLTERM,
                    CS_UNUSED)) != CS_SUCCEED)
    {
        dyn->con->ret_status = dyn->ret_status;
        scarper(__FILE__,__LINE__,"dbms_parse: ct_command() failed");
        return 0;
    }
    else
        return 1;
}
/*
 * Function to ensure that an option has the given value. Returns the previous
 * value of the option.
 */
CS_BOOL dbms_option(con, opt, new_value)
struct sess_con * con;
CS_INT opt;
CS_BOOL new_value;
{
CS_BOOL opt_flag;

    opt_flag = CS_FALSE;
    if (ct_options(con->connection, CS_GET, opt,
                &opt_flag, CS_UNUSED, NULL) != CS_SUCCEED)
    {
        ct_cancel(con, NULL, CS_CANCEL_ALL);
        if (ct_options(con->connection, CS_GET, opt,
                &opt_flag, CS_UNUSED, NULL) != CS_SUCCEED)
            scarper(__FILE__,__LINE__,"dbms_option: option get failed");
        return CS_FALSE;
    }
    if (opt_flag != new_value)
    {
        if (ct_options(con->connection, CS_SET, opt,
                &new_value, CS_UNUSED, NULL) != CS_SUCCEED)
        {
            scarper(__FILE__,__LINE__,"dbms_option: option set failed");
            return CS_FALSE;
        }
    }
    return opt_flag;
}
/************************************************************************
 * Find out the input and output variable types.
 */
int dbms_fix_types(dyn)
struct dyn_con * dyn;
{
static struct dyn_con * d;
register E2SQLDA * rdp, *rdp1;
char * x, *x1, *x2;
int len;
CS_INT i, j;
CS_BOOL opt_flag;
CS_INT map[256];
CS_INT * mp;

    dyn->need_dynamic = 0;
    curr_con = dyn->con;
    (void) ct_cancel(NULL, dyn->cmd, CS_CANCEL_ALL);
/*
 * Ready the structure used to establish the data formats
 */
    if (d == (struct dyn_con *) NULL
      &&  (d = dyn_init(dyn->con, DYN_SYB_CURS)) == (struct dyn_con *) NULL)
    {
        scarper(__FILE__,__LINE__,"dbms_fix_types: structure setup failed");
        return 0;
    }
    if (d->statement != (char *) NULL)
        free(d->statement);
    d->statement = (char *) malloc(strlen(dyn->statement));
/*
 * Construct a version of the input statement in the correct format for the
 * ct_dynamic() function. Replace the variables by ? place holders.
 */
    for (i = 0, mp = map, rdp = dyn->bdp, x = dyn->statement, x1 = d->statement;
            i < rdp->N;
                i++)
    {
        while(x < rdp->S[i])
        {
/*
 * Check to see if the same input variable occurs more than once
 */
            if (*x == '@')
            {
                if (*(x + 1) == '@')
                    *x1++ = *x++;
                else
                {
/*
 * Find the end of the variable name, and search for its prior reference
 */
                    for (x2 = x + 1;
                          *x2 == '_' || *x2 == '$'|| *x2 == '#' || isalnum(*x2);
                             x2++);
                    len = x2 - x;
                    for (j = 0; j < i; j++)
                        if (len == rdp->C[j] && !strncmp(rdp->S[j], x, len))
                            break;
                    if (j == i)
                        fprintf(stderr,
                             "Logic Error: cannot find duplicate name %-.*s\n",
                                      len, x);
                    else
                    {
                        *mp++ = j;
                        *x1++ = '?';
                        x = x2;
                    }
                }
            }
           *x1++ = *x++;
        }
        *x1++ = '?'; 
        *mp++ = i;
        x += rdp->C[i];
    }
/*
 * Deal with the trailing fragment of SQL statement
 */
    while (*x != '\0')
    {
        if (*x == '@')
        {
            if (*(x + 1) == '@')
                *x1++ = *x++;
            else
            {
/*
 * Find the end of the variable name, and search for its prior reference
 */
                for (x2 = x + 1;
                      *x2 == '_' || *x2 == '$'|| *x2 == '#' || isalnum(*x2);
                         x2++);
                len = x2 - x;
                for (j = 0; j < i; j++)
                    if (len == rdp->C[j] && !strncmp(rdp->S[j], x, len))
                        break;
                if (j == i)
                    fprintf(stderr,
                         "Logic Error: cannot find duplicate name %-.*s\n",
                                  len, x);
                else
                {
                    *mp++ = j;
                    *x1++ = '?';
                    x = x2;
                    continue;
                }
            }
        }
        *x1++ = *x++;
    }
    *x1 = '\0';
/*
 * See if we are allowed to execute anything, and make sure we can.
 */
    opt_flag = dbms_option(d->con, CS_OPT_NOEXEC, CS_FALSE);
/*
 * Now find out about it
 */
    ct_dynamic(d->cmd, CS_PREPARE, "fix_types", CS_NULLTERM,
                 d->statement, CS_NULLTERM);
    dbms_exec(d);
    ct_dynamic(d->cmd, CS_DESCRIBE_INPUT, "fix_types", CS_NULLTERM,
                 NULL, CS_UNUSED);
    dbms_exec(d);
    ct_dynamic(d->cmd, CS_DEALLOC, "fix_types", CS_NULLTERM,
                 NULL, CS_UNUSED);
    dbms_exec(d);
#ifdef DEBUG
    fprintf(stderr,"SQL: %s\nMessages:%s\n",d->statement,d->con->message_block);
#endif
    d->con->msgp = d->con->message_block;
    if (opt_flag == CS_TRUE)
        (void) dbms_option(d->con, CS_OPT_NOEXEC, CS_TRUE);
    (void) ct_cancel(NULL, d->cmd, CS_CANCEL_ALL);
    if (d->bdp == (E2SQLDA *) NULL)
    {
        scarper(__FILE__,__LINE__,"dbms_fix_types: Bind count mismatch");
        fprintf(stderr,"Before SQL: %s\n After SQL: %s\nMessages:%s\n",
                 dyn->statement, d->statement,d->con->message_block);
        return 0;
    }
/*
 * At this point, the input parameters are in the bind descriptor for d. They
 * need to be copied across to dyn. 
 */
    for (i = 0, j = 0, rdp = d->bdp, rdp1 = dyn->bdp, x = rdp1->base;
              j < rdp1->N; i++)
    {
        if (i >= rdp->N)
        {
            fputs("Logic Error: mapping failed\n", stderr);
            break;
        }
        if (map[i] == j)
        {
            rdp1->P[j] = rdp->P[i];
            rdp1->A[j] = rdp->A[i];
            rdp1->L[j] = rdp->L[i];
            rdp1->T[j] = rdp->T[i];
            rdp1->V[j] = x;
            x = x + (rdp->L[i])*rdp->arr;
            j++;
        }
    }
/*
 * We have finished with our temporary bind variable block.
 */
    sqlclu(rdp);
    d->bdp = (E2SQLDA *) NULL;
    if (d->sdp != (E2SQLDA *) NULL)
    {
        sqlclu(d->sdp);
        d->sdp = (E2SQLDA *) NULL;
    }
/*
 * Adjust the space for the real statement bind variable block.
 */
    if (x > (rdp1->bound + 1))
    {
        rdp = sqlald(rdp1->N, rdp1->arr,
                  1 + (x - rdp1->base)/(rdp1->N * rdp1->arr));
        for ( i = 0, x = rdp->base;
                i < rdp->N;
                    x = x + (rdp->L[i])*rdp->arr, i++)
        {
            rdp->P[i] = rdp1->P[i];
            rdp->A[i] = rdp1->A[i];
            rdp->S[i] = rdp1->S[i];
            rdp->C[i] = rdp1->C[i];
            rdp->L[i] = rdp1->L[i];
            rdp->T[i] = rdp1->T[i];
            rdp->V[i] = x;
        }
        sqlclu(rdp1);
        dyn->bdp = rdp;
    }
#ifndef OSF
    else
    if (rdp1->bound != (x - 1))
    {
        rdp1->bound = x - 1;
/*
 * Recover surplus space. Unbelievably, OSF will move something it is
 * shrinking. What a waste of machine cycles.
 */
        rdp = realloc(rdp1, x - (char *) rdp1);
        if (rdp != rdp1)
        {
            fputs( "Logic Error: realloc() moved structure\n", stderr);
            exit(1);
        }
        
    }
#endif
    d->bdp = (E2SQLDA *) NULL;
/*
 * Now get the original dynamic statement back to where it was.
 */
    dyn->res_type = 0;
    if ((dyn->ret_status = ct_command(dyn->cmd, CS_LANG_CMD,
              dyn->statement, CS_NULLTERM,
                    CS_UNUSED)) != CS_SUCCEED)
    {
        dyn->con->ret_status = dyn->ret_status;
        scarper(__FILE__,__LINE__,"dbms_fix_types: ct_command() failed");
        return 0;
    }
    do_rebind(dyn);
    return 1;
}
/************************************************************************
 * DML Statement Execution
 * Not sure that this can be array-processed; needs more research.
 */
int dbms_exec(dyn)
struct dyn_con * dyn;
{
    curr_con = dyn->con;
    if ((dyn->ret_status = ct_send(dyn->cmd)) != CS_SUCCEED)
    {
        dyn->con->ret_status = dyn->ret_status;
        scarper(__FILE__,__LINE__,"dbms_exec: ct_send() failed");
        return 0;
    }
    else
    {
        dyn->rows_sent++;
        return dbms_res_process((FILE *) NULL,(FILE *) NULL,dyn, NULL, 0);
    }
}
void dbms_error(con)
struct sess_con * con;
{
    curr_con = con;
    fprintf(stderr,"retcode:%d\n",con->ret_status);
    return;
}
/*
 * Function that captures bind variable data formats produced by ct_dynamic() 
 * calls.
 */
static void dbms_dyn_desc(d)
struct dyn_con * d;
{
char cbuf[32]; /* Same size as the value associated with the SQLDA */
struct bv bv, * anchor, *last_bv;
int  cnt, row_len, flen;
char *x, *j;
char ** v;
char ** s;
CS_SMALLINT **i;
CS_INT **r, **o;
CS_INT *l;
CS_INT *c, *t, *ot, *p, *a, *u;
CS_INT num_cols;

#ifdef DEBUG
    fprintf(stderr, "dbms_dyn_desc(%x)\n", (long) d);
    fflush(stderr);
#endif
    anchor = (struct bv *) NULL;
    bv.bname = &cbuf[0];
    bv.blen = sizeof(cbuf);
    bv.next = (struct bv *) NULL;
/*
 * Loop through the format list items, finding how many of them there will
 * be, and how much space we need to allocate.
 */
    for (cnt = 0,
         row_len = 0,
         ct_res_info(d->cmd,CS_NUMDATA,&num_cols, CS_UNUSED, NULL);
             cnt < num_cols && dbms_desc(d,cnt,&bv);
                 cnt++)
    {
        row_len += bv.dsize;
        if (anchor == (struct bv *) NULL)
        {
            anchor = (struct bv *) malloc(sizeof(struct bv));
            last_bv = anchor;
        }
        else
        {
            last_bv->next = (struct bv *) malloc(sizeof(struct bv));
            last_bv = last_bv->next;
        }
        *(last_bv) = bv;
        last_bv->bname = (char *) malloc(bv.blen+1);
        memcpy(last_bv->bname,cbuf,bv.blen);
        *(last_bv->bname + bv.blen) = '\0';
#ifdef DEBUG
        bv_dump(last_bv);
#endif
        bv.blen = sizeof(cbuf);
    }
    if (cnt != num_cols)
    {
        fprintf(stderr,"cnt:%d num_cols:%d\n",cnt,num_cols);
        scarper(__FILE__,__LINE__,"Describe() failed");
    }
    if (!cnt)
    {
        if (d->ret_status != CS_SUCCEED)
            scarper(__FILE__,__LINE__,"Describe() failed");
        return;
    }
/*
 * Allocate sufficient space for the bind data structures
 */
    sqlclu(d->bdp);
    d->bdp = sqlald(cnt, 1,1  + row_len/cnt);                    
    if ( !(d->sdt) )
    {                          /* Haven't allocated (*sdt)[] yet. */
        d->sdt = (CS_INT *) calloc(d->bdp->N, sizeof(CS_INT));
        d->scram_flags = (char *) calloc(d->bdp->N, sizeof(char));
    }
    else
    if (d->sdtl < d->bdp->N )
    {                          /* Need to reallocate saved type array. */
        d->sdt = (CS_INT *) realloc(d->sdt, sizeof(CS_INT) * d->bdp->N);
        d->scram_flags = (char *) realloc(d->scram_flags,
                                              sizeof(char)*d->bdp->N);
    }
    d->sdtl = cnt;
/*
 *  Loop through the bind variables, storing them in association with
 *  the SQLDA.
 */
    for (cnt = 1,
         j = d->scram_flags,
         ot = d->sdt,
         x = d->bdp->base,
         c = d->bdp->C,
         r = d->bdp->R,
         o = d->bdp->O,
         v = d->bdp->V,
         s = d->bdp->S,
         i = d->bdp->I,
         t = d->bdp->T,
         l = d->bdp->L,
         p = d->bdp->P,
         a = d->bdp->A,
         u = d->bdp->U,
         last_bv = anchor;
             last_bv != (struct bv *) NULL;
              x = x + (*l)*(d->bdp->arr),
              cnt++, ot++, c++, r++, o++, v++, s++, i++, t++, l++, j++, p++,
                             a++)
    {
/*
 * First, deal with the stored variable name.
 */
        *s = last_bv->bname;
        *c = last_bv->blen;
/*
 * Now the other values returned from the describe()
 */
        *ot = last_bv->dbtype;
        *l = last_bv->dbsize;
        *p = last_bv->prec;
        *a = last_bv->scale;
        *u = last_bv->nullok;
/*
 * Now reallocate the variable pointer to account for the true length
 */        
        *v = x;
        *j = 0;               /* Clear Scramble Flag */
        *t = last_bv->dbtype;

        switch (*t)
        {
        case CS_DECIMAL_TYPE:
        case CS_IMAGE_TYPE:
        case CS_TINYINT_TYPE:
        case CS_SMALLINT_TYPE:
        case CS_USHORT_TYPE:
        case CS_INT_TYPE:
        case CS_MONEY_TYPE:
        case CS_MONEY4_TYPE:
        case CS_REAL_TYPE:
        case CS_FLOAT_TYPE:
        case CS_NUMERIC_TYPE:
            break;
        default:
        {
        int k;
        char buf[ESTARTSIZE];
/*
 * Problems with extra zero bytes
 */
#ifdef SMALL
            if (*l < 255)
#endif
                (*l)++;
            *t = CS_CHAR_TYPE;
/*
 * Check for scramble
 */
            if (scram_cnt)
            {
                (void) sprintf(buf,"%.*s,\n",*c,*s);
                for (k = 0; k < scram_cnt; k++)
                    if (re_exec(buf,scram_cand[k]) > 0)
                    {
                        *j = 1;
                        break;
                    }
            }
        }
        }
        anchor = last_bv;
        last_bv = last_bv->next;
        free(anchor);
    }
#ifdef DEBUG
    type_print(stderr,d->bdp);
#endif
    d->so_far = 0;
    d->cur_ind = 0;
    d->to_do = 0;
    return;
}
/*
 * Process results. Either do all the fetching in one hit, or do single
 * fetches
 */
int dbms_res_process(fp1,fp2,dyn, output_function, all_flag)
FILE * fp1;
FILE * fp2;
struct dyn_con *dyn;
void (*output_function)();
int all_flag;
{
/*
 * Process the result set from a command. The function is restartable.
 */
    curr_con = dyn->con;
    if (dyn->res_type
      ||((dyn->ret_status = ct_results(dyn->cmd, &dyn->res_type))
                   == CS_SUCCEED))
    do
    {
    CS_INT msg_id;

        switch ((int)(dyn->res_type))
        {
        case CS_DESCRIBE_RESULT:
            dbms_dyn_desc(dyn);
            break;
        case CS_ROW_RESULT:
        case CS_PARAM_RESULT:
        case CS_STATUS_RESULT:
#ifdef DEBUG
            if (output_function != NULL)
            {
/* 
 * Print a result header based on the result type.
 */
                switch ((int)(dyn->res_type))
                {
                case  CS_ROW_RESULT:
                    if (fp1 != (FILE *) NULL)
                        fputs("ROW RESULTS\n", fp1);
                    if (fp2 != (FILE *) NULL)
                        fputs("ROW RESULTS\n", fp2);
                    break;
                case  CS_PARAM_RESULT:
                    if (fp1 != (FILE *) NULL)
                        fputs("PARAMETER RESULTS\n", fp1);
                    if (fp2 != (FILE *) NULL)
                        fputs("PARAMETER RESULTS\n", fp2);
                    break;
                case  CS_STATUS_RESULT:
                    if (fp1 != (FILE *) NULL)
                        fputs("STATUS RESULTS\n", fp1);
                    if (fp2 != (FILE *) NULL)
                        fputs("STATUS RESULTS\n", fp2);
                    break;
                }
            }
#endif
/*
 * All three of these result types are fetchable. Since the result model for
 * rpcs and rows are unified in the Sybase CT Library, we use common
 * code to display them.
 */
            if (dyn->sdp == (E2SQLDA *) NULL || !dyn->is_sel)
                desc_sel(dyn);
            dyn->is_sel = 1;
            if (all_flag)
                dyn_fetch(fp1, fp2, dyn, output_function);
            else
            {
                dyn->to_do = dyn->so_far;
                if (!dbms_fetch(dyn))
                {
                    scarper(__FILE__,__LINE__,"Fetch failed");
                    dyn->to_do = 0;
                    dyn->ret_status = CS_FAIL;
                }
                else
                    dyn->cur_ind = 0;
                dyn->to_do = dyn->so_far - dyn->to_do;
                if (dyn->to_do > 0)
                {
                    if (output_function != NULL)
                        (*output_function)(fp1, fp2, dyn);
                    dyn->ret_status = CS_SUCCEED;
                    dyn->con->ret_status = CS_SUCCEED;
                    if (fp1 != (FILE *) NULL || fp2 != (FILE *) NULL)
                    {
                        if (fp1 != (FILE *) NULL)
                            fwrite(dyn->con->message_block, sizeof(char),
                                dyn->con->msgp - dyn->con->message_block, fp1);
                        if (fp2 != (FILE *) NULL)
                            fwrite(dyn->con->message_block, sizeof(char),
                                dyn->con->msgp - dyn->con->message_block, fp2);
                        dyn->con->msgp = dyn->con->message_block;
                    }
                    return CS_SUCCEED;
                }
            }
            break;
        case CS_MSG_RESULT:
/*
 * This is some kind of status report
 */
            dyn->ret_status = ct_res_info(dyn->cmd, CS_MSGTYPE,
                    (CS_VOID *)&msg_id, CS_UNUSED, NULL);
            if (dyn->ret_status != CS_SUCCEED)
            {
                scarper(__FILE__,__LINE__,"ct_res_info(msgtype) failed");
                fputs(dyn->statement, stderr);
                fputc('\n',stderr);
            }
            if (output_function != NULL)
            {
                if (fp1 != (FILE *) NULL)
                    fprintf(fp1,
                      "ct_result returned CS_MSG_RESULT where msg id = %d.\n",
                           msg_id);
                if (fp2 != (FILE *) NULL)
                    fprintf(fp2,
                      "ct_result returned CS_MSG_RESULT where msg id = %d.\n",
                           msg_id);
            }
            break;
        case CS_CMD_SUCCEED:
/*
 * This means no rows were returned.
 */
        case CS_CMD_DONE:
/*
 * Done with result set.
 */
            dyn->is_sel = 0;
            break;
        case CS_CMD_FAIL:
/*
 * The server encountered an error while
 * processing our command.
 */
            if (fp1 != (FILE *) NULL)
            {
                fputs(dyn->statement, fp1);
                fputc('\n', fp1);
            }
            if (fp2 != (FILE *) NULL)
            {
                fputs(dyn->statement, fp2);
                fputc('\n', fp2);
            }
#ifdef PICKY
            scarper(__FILE__,__LINE__,"ct_results returned CS_CMD_FAIL.");
#endif
            break;

        default:
/*
 * We got something unexpected.
 */
            scarper(__FILE__,__LINE__,
                      "ct_results returned unexpected result type");
            break;
        }
    }
    while ((dyn->ret_status = ct_results(dyn->cmd, &dyn->res_type))
                   == CS_SUCCEED);
    dyn->res_type = 0;
/*
 * We're done processing results. Let's check the return value of ct_results()
 * to see if everything went ok.
 */
    switch ((int)dyn->ret_status)
    {
    case CS_END_RESULTS:
/*
 * Everything went fine.
 */
        dyn->ret_status = CS_SUCCEED;
        break;

    case CS_FAIL:
/*
 * Something went wrong
 */
#ifdef PICKY
        scarper(__FILE__,__LINE__,"ct_results() failed");
#endif
        break;

    default:
/*
 * We got an unexpected return value.
 */
#ifdef PICKY
        scarper(__FILE__,__LINE__,
                  "ct_results returned unexpected result type");
#endif
        break;
    }
    if (fp1 != (FILE *) NULL)
        fwrite(dyn->con->message_block, sizeof(char),
                    dyn->con->msgp - dyn->con->message_block, fp1);
    if (fp2 != (FILE *) NULL)
        fwrite(dyn->con->message_block, sizeof(char),
                    dyn->con->msgp - dyn->con->message_block, fp2);
    if (fp1 != (FILE *) NULL || fp2 != (FILE *) NULL || all_flag)
        dyn->con->msgp = dyn->con->message_block;
    dyn->con->ret_status = dyn->ret_status;
    if ( dyn->ret_status == CS_SUCCEED)
        return 1;
    else
        return 0;
}
int dbms_disconnect(con)
struct sess_con * con;
{
/*
 * Clean up the command handle used
 */
    curr_con = con;
    (void) ct_cancel(con->connection, NULL, CS_CANCEL_ALL);
    (void) ct_cmd_drop(con->cmd);
    if ((con->ret_status = ct_close(con->connection, CS_FORCE_CLOSE))
                 != CS_SUCCEED)
        scarper(__FILE__,__LINE__,"dbms_disconnect: ct_close() failed");
    if ((con->ret_status = ct_con_drop(con->connection)) != CS_SUCCEED)
        scarper(__FILE__,__LINE__,"dbms_disconnect: ct_con_drop() failed");
    if ((con->ret_status = ct_exit(con->context, CS_FORCE_EXIT)) != CS_SUCCEED)
        scarper(__FILE__,__LINE__,"dbms_disconnect: ct_exit() failed");
    if ((con->ret_status = cs_ctx_drop(con->context)) != CS_SUCCEED)
        scarper(__FILE__,__LINE__,"dbms_disconnect: cs_ctx_drop() failed");
    return 1;
}
/**********************************************************************
 * Initialise a dynamic statement control block. If the thing already
 * exists, dispose of it.
 */
int dbms_open(d)
struct dyn_con * d;
{
    curr_con = d->con;
    if ((d->ret_status = ct_cmd_alloc(d->con->connection, &(d->cmd)))
                  != CS_SUCCEED)
    {
        d->con->ret_status = d->ret_status;
        scarper(__FILE__,__LINE__,"dbms_open: ct_cmd_alloc() failed");
        return 0;
    }
    else
    {
        d->res_type = 0;                   /* An impossible value */
        return 1;
    }
}
/************************************************************************
 * Destroy a dynamic statement control block
 */
int dbms_close(d)
struct dyn_con * d;
{
    curr_con = d->con;
    (void) ct_cancel(NULL, d->cmd, CS_CANCEL_ALL);
    if ((d->ret_status = ct_cmd_drop(d->cmd)) != CS_SUCCEED)
    {
        d->con->ret_status = d->ret_status;
        scarper(__FILE__,__LINE__,"dbms_close: ct_cmd_drop() failed");
        return 0;
    }
    else
        return 1;
}
/********************************************************************
 * Process the Bind variables
 */
int dbms_bind(d, s, c, v, l, p, a, t, i, o, r)
struct dyn_con * d;
char * s;
CS_INT c;
char * v;
CS_INT l;
CS_INT p;
CS_INT a;
CS_INT t;
CS_SMALLINT *i;
CS_INT *o;
CS_INT *r;
{
CS_DATAFMT f;

    curr_con = d->con;
/*
 * Now execute the bind itself
 */
    memset((char *) &f, 0, sizeof(f));
    memcpy(f.name, s, c);
    f.name[c] = 0;
    f.namelen = CS_NULLTERM;
    f.datatype = t;
    f.precision = p;
    f.scale = a;
    f.maxlength = l;
    if (t == CS_NUMERIC_TYPE || t == CS_DECIMAL_TYPE)
    {
/*
 * Amazingly, the buffer must contain valid contents when bound, even if we
 * specify NOEXEC
 */
        *v = (char) f.precision;
        *(v + 1) = (char) f.scale;
        memset(v + 2,0,l - 2);
    }
    *o = l;
/*    f.status = CS_INPUTVALUE|CS_CANBENULL; */
    f.status = CS_INPUTVALUE;
#ifdef DEBUG
    syb_datafmt_dump(&f);
#endif
    if ((d->ret_status = ct_setparam(d->cmd, &f, v, o, i))
                    != CS_SUCCEED)
        return 0;
    else
        return 1;
}
/***********************************************************************
 * syb_clientmsg_cb()
 *
 * Type of function:
 *     example program client message handler
 *
 * Purpose:
 *     Installed as a callback into Open Client.
 *
 * Returns:
 *     CS_SUCCEED
 *
 * Side Effects:
 *     None
 */

static CS_RETCODE
#ifdef NT4
__stdcall
#endif
syb_clientmsg_cb(context, connection, errmsg)
CS_CONTEXT    *context;
CS_CONNECTION    *connection;    
CS_CLIENTMSG    *errmsg;
{
    fprintf(stderr, "\nOpen Client Message:\n");
    fprintf(stderr, "Message number: LAYER = (%ld) ORIGIN = (%ld) ",
        CS_LAYER(errmsg->msgnumber), CS_ORIGIN(errmsg->msgnumber));
    fprintf(stderr, "SEVERITY = (%ld) NUMBER = (%ld)\n",
        CS_SEVERITY(errmsg->msgnumber), CS_NUMBER(errmsg->msgnumber));
    fprintf(stderr, "Message String: %s\n", errmsg->msgstring);
    if (errmsg->osstringlen > 0)
    {
        fprintf(stderr, "Operating System Error: %s\n",
            errmsg->osstring);
    }

    return CS_SUCCEED;
}

/*
 * syb_servermsg_cb()
 *
 * Type of function:
 *     example program server message handler
 *
 * Purpose:
 *     Installed as a callback into Open Client.
 *
 * Returns:
 *     CS_SUCCEED
 *
 * Side Effects:
 *     None
 */
static CS_RETCODE
#ifdef NT4
__stdcall
#endif
syb_servermsg_cb(context, connection, srvmsg)
CS_CONTEXT    *context;
CS_CONNECTION    *connection;
CS_SERVERMSG    *srvmsg;
{
#ifdef SYNCHRONOUS_MESSAGES
    fprintf(stderr, "\nServer message:\n");
    fprintf(stderr, "Message number: %ld, Severity %ld, ",
        srvmsg->msgnumber, srvmsg->severity);
    fprintf(stderr, "State %ld, Line %ld",
        srvmsg->state, srvmsg->line);
    
    if (srvmsg->svrnlen > 0)
    {
        fprintf(stderr, "\nServer '%s'", srvmsg->svrname);
    }
    
    if (srvmsg->proclen > 0)
    {
        fprintf(stderr, " Procedure '%s'", srvmsg->proc);
    }

    fprintf(stderr, "\nMessage String: %s", srvmsg->text);
#else
#ifdef WANT_SEVERITY
    curr_con->msgp += sprintf(curr_con->msgp,"%ld:%ld:%ld:%ld:%s:%s:%s",
                    srvmsg->msgnumber,
                    srvmsg->severity,
                    srvmsg->state,
                    srvmsg->line,
                    (srvmsg->svrnlen > 0)? srvmsg->svrname:"",
                    (srvmsg->proclen > 0)? srvmsg->proc:"",
                    srvmsg->text);
#else
    strcpy(curr_con->msgp, srvmsg->text);
    curr_con->msgp += strlen(srvmsg->text);
#endif
#endif

    return CS_SUCCEED;
}
/********************************************************************
 * Initialise the Sybase client library.
 */
static CS_RETCODE
syb_init(context)
CS_CONTEXT    **context;
{
CS_RETCODE    retcode;
CS_INT        netio_type = CS_SYNC_IO;

/*
 * Get a context handle to use.
 */
    if ((retcode = cs_ctx_alloc(CS_VERSION_110, context)) != CS_SUCCEED)
    {
        scarper(__FILE__,__LINE__,"syb_init: cs_ctx_alloc() failed");
        return retcode;
    }
/*
 * Initialize Open Client.
 */
    if ((retcode = ct_init(*context, CS_VERSION_110)) != CS_SUCCEED)
    {
        scarper(__FILE__,__LINE__,"syb_init: ct_init() failed");
        cs_ctx_drop(*context);
        *context = NULL;
        return retcode;
    }
/*
 * Install client and server message handlers.
 */
    if (retcode == CS_SUCCEED)
    {
        if ((retcode = ct_callback(*context, NULL, CS_SET, CS_CLIENTMSG_CB,
                    (CS_VOID *)syb_clientmsg_cb)) != CS_SUCCEED)
        {
            scarper(__FILE__,__LINE__,
                   "syb_init: ct_callback(clientmsg) failed");
        }
    }
    if (retcode == CS_SUCCEED)
    {
        if ((retcode = ct_callback(*context, NULL, CS_SET, CS_SERVERMSG_CB,
                (CS_VOID *)syb_servermsg_cb)) != CS_SUCCEED)
        {
            scarper(__FILE__,__LINE__,
                  "syb_init: ct_callback(servermsg) failed");
        }
    }

/* 
 * This is an synchronous example so set the input/output type
 * to synchronous (This is the default setting, but show an
 * example anyway).
 */
    if (retcode == CS_SUCCEED)
    {
        if ((retcode = ct_config(*context, CS_SET, CS_NETIO, &netio_type, 
                        CS_UNUSED, NULL)) != CS_SUCCEED)
        {
            scarper(__FILE__,__LINE__,"syb_init: ct_config(netio) failed");
        }
    }
    if (retcode != CS_SUCCEED)
    {
        ct_exit(*context, CS_FORCE_EXIT);
        cs_ctx_drop(*context);
        *context = NULL;
    }
    return retcode;
}
/*
 * ct_debug stuff. Call this function just before any call to OC-Lib that
 * is returning failure.
 */
void syb_debug_enable(context)
CS_CONTEXT    *context;
{
    if (ct_debug(context, NULL, CS_SET_FLAG, CS_DBG_API_STATES,
                NULL, CS_UNUSED) != CS_SUCCEED)
    {
        scarper(__FILE__,__LINE__,"syb_debug_enable: ct_debug() failed");
    }
    return;
}
/*
 * syb_execute_cmd()
 *
 * Type of function:   
 *      example program utility api
 *
 * Purpose:
 *      This routine sends a language command to the server. It expects no
 *    rows or parameters to be returned from the server.
 *
 * Parameters:   
 *      connection       - Pointer to CS_COMMAND structure.
 *      cmdbuf          - The buffer containing the command.
 *
 * Return:
 *      Result of functions called in CT-Lib
 */

static CS_RETCODE
syb_execute_cmd(con, cmdbuf)
struct sess_con *con;
CS_CHAR         *cmdbuf;
{
CS_INT      restype;
CS_RETCODE      query_code;

    curr_con = con;
    if ((con->ret_status =ct_command(con->cmd, CS_LANG_CMD, cmdbuf, CS_NULLTERM,
            CS_UNUSED)) != CS_SUCCEED)
    {
        scarper(__FILE__,__LINE__,"syb_execute_cmd: ct_command() failed");
        return con->ret_status;
    }

    if ((con->ret_status = ct_send(con->cmd)) != CS_SUCCEED)
    {
        scarper(__FILE__,__LINE__,"syb_execute_cmd: ct_send() failed");
        return con->ret_status;
    }

/*
 * Examine the results coming back. If any errors are seen, the query
 * result code (which we will return from this function) will be
 * set to FAIL.
 */
    query_code = CS_SUCCEED;
    while ((con->ret_status = ct_results(con->cmd, &restype)) == CS_SUCCEED)
    {
        switch((int)restype)
        {
        case CS_CMD_SUCCEED:
        case CS_CMD_DONE:
            break;

        case CS_CMD_FAIL:
            query_code = CS_FAIL;
            break;

        case CS_STATUS_RESULT:
            con->ret_status = ct_cancel(NULL, con->cmd, CS_CANCEL_CURRENT);
            if (con->ret_status != CS_SUCCEED)
            {
                scarper(__FILE__,__LINE__,"syb_execute_cmd: ct_cancel() failed");
                query_code = CS_FAIL;
            }
            break;

        default:
/*
 * Unexpected result type.
 */
            query_code = CS_FAIL;
            break;
        }
        if (query_code == CS_FAIL)
        {
/*
 * Terminate results processing and break out of
 * the results loop
 */
            con->ret_status = ct_cancel(NULL, con->cmd, CS_CANCEL_ALL);
            if (con->ret_status != CS_SUCCEED)
            {
                scarper(__FILE__,__LINE__,"syb_execute_cmd: ct_cancel() failed");
            }
            break;
        }
    }
    return query_code;
}
/*********************************************
 * Attach to the Database.
 */
int dbms_connect(x)
struct sess_con * x;
{
char * user;
char * pwd;
char * sv;
char * db;
char buf[128];

    x->msgp = x->message_block;
    curr_con = x;
    if (syb_init(&(x->context)) != CS_SUCCEED)
        return 0;
    if ((x->ret_status = ct_con_alloc(x->context, &(x->connection)))
                   != CS_SUCCEED)
    {
        scarper(__FILE__,__LINE__,"ct_con_alloc failed");
        return x->ret_status;
    }
/*
 * Split up the user name, pwd and database name
 */
    if ((user = nextasc(x->uid_pwd,'/','\\')) != (char *) NULL)
        user = strdup(user);
    else
        user = strdup("");
    if ((pwd = nextasc(NULL, '/','\\')) != (char *) NULL)
        pwd = strdup(pwd);
    else
        pwd = strdup("");
    if ((sv = nextasc(NULL, '/','\\')) != (char *) NULL)
        sv = strdup(sv);
    else
        sv = strdup("");
    if ((db = nextasc(NULL, '/','\\')) != (char *) NULL)
        db = strdup(db);
    else
        db = strdup("");
#ifdef DEBUG
    fprintf(stderr,"user:%s pwd:%s server:%s db:%s\n",user,pwd,sv,db);
#endif
/*    
 * If a user is defined, set the CS_USERNAME property.
 */
    if (x->ret_status == CS_SUCCEED && user != (char *) NULL)
    {
        if ((x->ret_status = ct_con_props(x->connection,
                CS_SET, CS_USERNAME, 
                user, CS_NULLTERM, NULL)) != CS_SUCCEED)
            scarper(__FILE__,__LINE__,"ct_con_props(user) failed");
    }
/*    
 * If a pwd is defined, set the CS_PASSWORD property.
 */
    if (x->ret_status == CS_SUCCEED && pwd != (char *) NULL)
    {
        if ((x->ret_status = ct_con_props(x->connection,
                CS_SET, CS_PASSWORD, 
                pwd, CS_NULLTERM, NULL)) != CS_SUCCEED)
            scarper(__FILE__,__LINE__,"ct_con_props(pwd) failed");
    }
/*    
 * Set the CS_APPNAME property.
 */
    if (x->ret_status == CS_SUCCEED && x->appname != (char *) NULL)
    {
        if ((x->ret_status = ct_con_props(x->connection,
                CS_SET, CS_APPNAME, 
                x->appname, CS_NULLTERM, NULL)) != CS_SUCCEED)
            scarper(__FILE__,__LINE__,"ct_con_props(appname) failed");
    }
/*    
 * Open a Server connection.
 */
    if (x->ret_status == CS_SUCCEED)
    {
        if ((x->ret_status = ct_connect(x->connection, sv,
        ((sv == (char *) NULL) ? 0 : CS_NULLTERM))) != CS_SUCCEED)
            scarper(__FILE__,__LINE__,"ct_connect failed");
    }
    if (x->ret_status != CS_SUCCEED)
    {
        ct_con_drop(x->connection);
        x->connection = (char *) NULL;
    }
    if (ct_cmd_alloc(x->connection, &(x->cmd)) != CS_SUCCEED)
    {
        scarper(__FILE__,__LINE__,"dbms_connect: ct_cmd_alloc() failed");
        free(user);
        free(pwd);
        free(sv);
        free(db);
        return 0;
    }
    sprintf(buf,"use %s\n",db);
    free(user);
    free(pwd);
    free(sv);
    free(db);
    return syb_execute_cmd(x,buf);
}
/***********************************************************************
 * Describe the select list variables
 */
int dbms_desc(d,cnt,bv)
struct dyn_con * d;
int cnt;
struct bv *bv;
{
CS_DATAFMT f;

    curr_con = d->con;
    if ((d->ret_status = ct_describe(d->cmd, (cnt + 1), &f)) != CS_SUCCEED)
    {
        d->con->ret_status = d->ret_status;
        scarper(__FILE__,__LINE__,"dbms_desc: ct_describe() failed");
        return 0;
    }
    else
    {
#ifdef DEBUG
        syb_datafmt_dump(&f);
#endif
        memcpy(bv->bname,f.name,f.namelen);
        bv->bname[f.namelen] = '\0';
        bv->blen = f.namelen;
        d->locale = f.locale;
        bv->dsize =  f.maxlength;
        bv->dbsize =  f.maxlength;
        bv->dbtype =  f.datatype;
        if ((f.datatype == CS_VARCHAR_TYPE || f.datatype == CS_CHAR_TYPE)
#ifdef SMALL
           && f.maxlength < 255
#endif
           )
        {
            bv->dbsize++;
            bv->dsize++;
        }
        bv->prec  =  f.precision;
        bv->scale  =  f.scale;
        bv->nullok  =  f.status;
        return 1;
    }
}
/*
 * Now execute the define itself
 */
int dbms_define(d,cnt,v,l,t,i,o,r)
struct dyn_con * d;
int cnt;
char * v;
CS_INT l;
CS_INT t;
CS_SMALLINT *i;
CS_INT *o;
CS_INT *r;
{
CS_DATAFMT f;
int ind=cnt - 1;

    curr_con = d->con;
    memset((char *) &f,0,sizeof(f));
    f.namelen = d->sdp->C[ind];
    strncpy(f.name,d->sdp->S[ind],f.namelen);
    f.datatype =  t;
    if (t == CS_VARCHAR_TYPE || t == CS_FLOAT_TYPE)
        f.format =  CS_FMT_UNUSED;
    else
        f.format =  CS_FMT_NULLTERM;
    f.precision =  d->sdp->P[ind];
    f.scale =  d->sdp->A[ind];
    f.maxlength = l;
    f.count = d->sdp->arr;
    *o = l;
#ifdef DEBUG
    syb_datafmt_dump(&f);
#endif
    if ((d->ret_status = ct_bind(d->cmd, cnt, &f, v, o, i)) != CS_SUCCEED)
    {
        d->con->ret_status = d->ret_status;
        scarper(__FILE__,__LINE__,"dbms_define: ct_bind() failed");
        return 0;
    }
    else
        return 1;
}
/************************************************************************
 * Handle an array fetch with the dynamic variables
 */
int dbms_fetch(dyn)
struct dyn_con *dyn;
{
CS_INT rows_read;

    curr_con = dyn->con;
    if (dyn->sdp == (E2SQLDA *) NULL)
        return 0;
    if ((dyn->ret_status = ct_fetch(dyn->cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED,
            &rows_read)) == CS_SUCCEED || (dyn->ret_status == CS_ROW_FAIL)
            || (dyn->ret_status == CS_END_DATA)) 
    {
/*
 * Increment our row count by the number of rows just fetched.
 */
        if (dyn->ret_status == CS_SUCCEED) 
            dyn->so_far = dyn->so_far + rows_read;
        else

/*
 * Check if we hit a recoverable error.
 */
        if (dyn->ret_status == CS_ROW_FAIL)
            fprintf(stderr, "Error on row %d\n", dyn->so_far);
        return 1;
    }
    else
    {
        dyn->to_do = 0;
        return 0;
    }
}
void dbms_commit(sess)
struct sess_con * sess;
{
    curr_con = sess;
    syb_execute_cmd(sess,"commit\n");
    return;
}
void dbms_roll(sess)
struct sess_con * sess;
{
    curr_con = sess;
    syb_execute_cmd(sess,"rollback\n");
    return;
}
