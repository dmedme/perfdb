/************************************************************************
 * e2sqlitelib.c - SQLITE support routines for e2sqllib.c
 *
 * The idea is that we can plug in anything here.
 */
static char * sccs_id =  "@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1993\n";
#ifndef MINGW32
#include <sys/param.h>
#endif
#include <sys/types.h>
#ifndef LCC
#ifndef VCC2003
#include <sys/file.h>
#endif
#endif
#ifdef V32
#include <time.h>
#else
#ifndef LCC
#ifndef VCC2003
#include <sys/time.h>
#endif
#endif
#endif
#ifdef SEQ
#include <fcntl.h>
#include <time.h>
#else
#ifdef ULTRIX
#include <fcntl.h>
#else
#ifdef AIX
#include <fcntl.h>
#else
#ifndef LCC
#ifndef VCC2003
#include <sys/fcntl.h>
#endif
#endif
#endif
#endif
#endif
#include <stdio.h>
#include <stdlib.h>
#ifndef LCC
#ifndef VCC2003
#include <unistd.h>
#endif
#endif
#include <string.h>
#include <ctype.h>

#ifdef AIX
#include <memory.h>
#endif
#include "tabdiff.h"
/************************************************************************
 * Statement Preparation
 */
int dbms_parse(dyn)
struct dyn_con * dyn;
{
char *xp = NULL;

    if (dyn->stmthp != NULL)
        dbms_close(dyn);
    dyn->ret_status = sqlite3_prepare_v2(dyn->con->srvhp,
            dyn->statement,
                   strlen((char *)dyn->statement), &dyn->stmthp, &xp);
    dyn->con->ret_status = dyn->ret_status;
    if ((dyn->sel_cols = sqlite3_column_count(dyn->stmthp)) != 0)
        dyn->is_sel = 1;
    else
        dyn->is_sel = 0;
#ifdef DEBUG
    fprintf(stderr, "dbms_parse(%s)\n", dyn->statement);
    dbms_error(dyn->con);
#endif
    if (dyn->ret_status != SQLITE_OK)
        return 0;                        /* Syntax error */
/*
 * Find out whether the statement is a select
 */
    return 1;
}
/************************************************************************
 * DML Statement Execution
 */
int dbms_exec(dyn)
struct dyn_con * dyn;
{
int ret_arr_size = ((dyn->cur_ind) ? (dyn->cur_ind) : 1);
int r;

#ifdef DEBUG
    fprintf(stderr, "dbms_exec(%s) is_sel:%d\n", dyn->statement, dyn->is_sel);
    fflush(stderr);
#endif
    r = sqlite3_step(dyn->stmthp);
    dyn->ret_status = r;
    dyn->con->ret_status = r;
#ifdef DEBUG
    fprintf(stderr, "dbms_exec(%s) r=%d\n", dyn->statement, r);
    dbms_error(dyn->con);
#endif
    if (r != SQLITE_DONE && r != SQLITE_ROW && r != SQLITE_OK)
        return 0;
#ifdef DEBUG
    fprintf(stderr, "dbms_exec(%s) r=%d rows=%d status=%d con->status=%d\n",
            dyn->statement, r, dyn->rows_sent, dyn->ret_status,
            dyn->con->ret_status);
#endif
    if (r == SQLITE_DONE)
        sqlite3_reset(dyn->stmthp);
    return 1;
}
void dbms_error(con)
struct sess_con * con;
{
    if (con->srvhp != NULL)
    {
        con->ret_status = sqlite3_extended_errcode(con->srvhp);
        (void) fprintf(stderr,"SQLITE Error: %d %s\n", con->ret_status,
                      sqlite3_errmsg(con->srvhp));
    }
    else
        scarper(__FILE__,__LINE__,
                "dbms_error() called with no valid database connection");
    return;
}
static int dbms_batch(con, ptxt)
struct sess_con * con;
char * ptxt;
{
char * errp = NULL;

    con->ret_status = sqlite3_exec(con->srvhp, ptxt, NULL, NULL, &errp);
    if (errp != NULL) 
    {
        (void) fprintf(stderr,"SQLITE Statement:%s\n Error: %d %s\n", ptxt,
                       con->ret_status, errp);
        sqlite3_free(errp);
        return 0;
    }
    return 1;
}
void dbms_roll(con)
struct sess_con * con;
{
    (void) dbms_batch(con,"rollback;");
    return;
}
int dbms_disconnect(con)
struct sess_con * con;
{
    if (con->srvhp != NULL)
    {
        sqlite3_close(con->srvhp);
        con->srvhp = NULL;
    }
    return 1;
}
/**********************************************************************
 * Initialise a dynamic statement control block. If the thing already
 * exists, dispose of it.
 */
int dbms_open(d)
struct dyn_con * d;
{
    if (d->stmthp != NULL)
        dbms_close(d);
    return 1;
}
/************************************************************************
 * Destroy a dynamic statement control block
 */
int dbms_close(d)
struct dyn_con * d;
{
    if (d->stmthp != NULL)
    {
        sqlite3_finalize(d->stmthp);
        d->stmthp = NULL;
    }
    return 1;
}
/********************************************************************
 * Process the Bind variables. The otherwise unused o variable is
 * used to pass in a sequence for '?' placeholders. Otherwise
 * the variable number is to be passed in via s and c.
 */
int dbms_bind(d, s, c, v, l, a, p, t, i, o, r)
struct dyn_con * d;
char * s;
short int c;
char * v;
int l;
short int p;
short int a;
short int t;
short int *i;
unsigned short int *o;
unsigned short int *r;
{
int ret = 0;
double dbl = 0.0;
sqlite3_int64 li = 0;
int parmi;
char * nm;
/*
 * Now execute the bind itself
 */
    d->ret_status = SQLITE_OK;
#ifdef DEBUG
    fflush(stdout);
    (void) fprintf(stderr,"Bind Pointers %x %x %x %x %x %x\n",
          (int) d,
          (int) s,
          (int) v,
          (int)i,
          (int)o,
          (int)r);
    fflush(stderr);
    (void) fprintf(stderr,"Bind Name %*.*s  Type :%d  Length :%d\n",
                   c,c,s,t,l);
    fflush(stderr);
#endif
    if (*s == '?' && c == 1)
        parmi = (int) *o;
    else
    if (isdigit(*(s+1)))
        parmi = atoi(s + 1);
    else
    {
        nm = (char *) malloc(c + 1);
        memcpy(nm,s,c);
        nm[c] = 0;
        parmi = sqlite3_bind_parameter_index(d->stmthp, nm);
        free(nm);
    }
    if (i != NULL && *i != 0)
        t = SQLITE_NULL;
    switch(t)
    {
    case SQLITE_INTEGER:
        if (l < sizeof(sqlite3_int64))
        {
            memcpy((char *) &ret, v, (l > sizeof(int))? (sizeof(int)): l);
            d->ret_status = sqlite3_bind_int(d->stmthp, parmi, ret);
        }
        else
        {
            memcpy((char *) &li, v, sizeof(sqlite3_int64));
            d->ret_status = sqlite3_bind_int64(d->stmthp, parmi, li);
        }
        break;
    case SQLITE_FLOAT:
        memcpy((char *) &dbl, v, (l > sizeof(double))? (sizeof(double)): l);
        d->ret_status = sqlite3_bind_double(d->stmthp, parmi, ret);
        break;
    case SQLITE_BLOB:
        d->ret_status = sqlite3_bind_blob(d->stmthp, parmi, v, l, NULL);
        break;
    case SQLITE3_TEXT:
        if (l > 0)
        {
            d->ret_status = sqlite3_bind_text(d->stmthp, parmi, v, l, NULL);
            break;
        }
    case SQLITE_NULL:
    default:
        d->ret_status = sqlite3_bind_null(d->stmthp, parmi);
        break;
    }
    d->con->ret_status = d->ret_status;
    if (d->ret_status == SQLITE_OK)
        return 1;
    else
        return 0;
}
/*********************************************
 * Attach to the Database. Return a pointer to a structure holding
 * the login data area. Blank the input user/password. 
 */
int dbms_connect(x)
struct sess_con * x;
{
    fputs("Connecting ....\n", stderr);
    fflush(stderr);
    if (sqlite3_initialize() == SQLITE_OK)
    {
        if (sqlite3_open_v2(x->uid_pwd, &(x->srvhp),
             SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE, NULL) == SQLITE_OK)
            return 1;
        else
        {
            dbms_error(x);
            scarper(__FILE__, __LINE__, "sqlite3_open_v2() has failed");
        }
    }
    else
    {
        scarper(__FILE__, __LINE__, "sqlite3_initialize() has failed");
    }
    return 0;
}
/***********************************************************************
 * Describe the select list variables. SQLITE does not notionally return an
 * array of the same type. Rather, each column can be of a different type in
 * different rows.
 *
 * Thus, dbms_desc() must be called for every row, not just the before data
 * is copied in to the program's memory.
 *
 * However, our programs still need to know column names before executing a
 * fetch. So we have an additional variation for the post-fetch retrieves.
 */
int dbms_desc(d,cnt,bv)
struct dyn_con * d;
int cnt;
struct bv *bv;
{
unsigned char x;

    cnt--;
    if (cnt < 0 || cnt >= d->sel_cols)
        return 1;
    memset(bv, 0, sizeof(*bv));
    bv->bname = sqlite3_column_origin_name(d->stmthp, cnt);
    bv->blen = strlen(bv->bname);
    return 0;
}
int dbms_ddesc(d,cnt,bv)
struct dyn_con * d;
int cnt;
struct bv *bv;
{
unsigned char x;

    memset(bv, 0, sizeof(*bv));
    bv->bname = sqlite3_column_name(d->stmthp, cnt);
    bv->blen = strlen(bv->bname);
    bv->dbtype = sqlite3_column_type(d->stmthp, cnt);
    bv->dsize = sqlite3_column_bytes(d->stmthp, cnt);
    bv->dbsize = bv->dsize;
    return 1;
}
/*
 * Define, I suspect, is an ORACLE-ism; it refers to the allocation of storage
 * to receive select results. SQLITE doesn't work like this, because it appears
 * that the columns aren't typed; you have to inspect each column of each row.
 * Each column of each row needs to be characterised independently.
 *
 * The problem is that the E2 programs assume that the select list is available
 * after the parse. There is limited support for this is SQLITE (it doesn't do
 * anything sensible for derived columns) but for our purposes (constructing
 * insert statements that correspond to select * from tables, for instance) it
 * is enough.
 *
 * So something is constructed; we get rid of it again before the real selects.
 */
int dbms_define()
{
    return 1;
}
/************************************************************************
 * Handle an array fetch. With SQLITE, there is no array fetch; there is
 * only a statement step, followed by calls column by column to read the data.
 */
int dbms_fetch(dyn)
struct dyn_con *dyn;
{
char cbuf[256];
struct bv bv, * anchor, *last_bv;
int  cnt, row_len, flen;
char *x, *j;
char * x1;
char ** v;
char ** s;
short int **i;
unsigned short int **r, **o;
int *l;
short int *c, *t, *ot, *p, *a, *u;
double dbl;
int ret;

    anchor = (struct bv *) NULL;
    memset((char *) &bv, 0, sizeof(bv));
    bv.bname = &cbuf[0];
    bv.blen = sizeof(cbuf);
/*
 * Loop through the select list items, transferring the data to the buffer.
 */
    for (cnt = 0,
         row_len = 0;
             cnt < dyn->sel_cols && dbms_ddesc(dyn,cnt,&bv);
                 cnt++)
    {
        switch((int) bv.dbtype)
        {
        case SQLITE_NULL:
            bv.dsize = 0;
            break;
        case SQLITE_BLOB:
        case SQLITE3_TEXT:
            bv.dsize = bv.dbsize + 1; /* Space for the NUL */
            break;
        case SQLITE_INTEGER:
        case SQLITE_FLOAT:
        default:
            bv.dsize = bv.dbsize;
            break;
        }
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
        memcpy(last_bv->bname, bv.bname, bv.blen);
        *(last_bv->bname + bv.blen) = '\0';
#ifdef DEBUG
        bv_dump(last_bv);
#endif
        bv.blen = sizeof(cbuf);
    }
/*
 * Allocate sufficient space for the select data structures
 */
    sqlclu(dyn->sdp);
    dyn->sdp = sqlald(cnt, 1,1  + row_len/cnt);                    
#ifdef DEBUG
    fprintf(stderr, "scram_cnt:%d d->sdt(%x)\n", (long) d->sdt, scram_cnt);
    fflush(stderr);
#endif
    if ( !(dyn->sdt) )
    {                          /* Haven't allocated (*sdt)[] yet. */
        dyn->sdt = (short *) calloc(dyn->sdp->N, sizeof(short));
        if (scram_cnt)
            dyn->scram_flags = (char *) calloc(dyn->sdp->N, sizeof(char));
    }
    else
    if (dyn->sdtl < dyn->sdp->N )
    {                          /* Need to reallocate saved type array. */
        dyn->sdt = (short *) realloc(dyn->sdt, sizeof(short) * dyn->sdp->N);
        if (scram_cnt && (dyn->scram_flags != (char *) NULL))
            dyn->scram_flags = (char *) realloc(dyn->scram_flags,
                                              sizeof(char)*dyn->sdp->N);
    }
    dyn->sdtl = cnt;
/*
 *  Loop through the select variables, storing them in association with
 *  the SQLDA
 */
    for (cnt = 0,
         j = dyn->scram_flags,
         ot = dyn->sdt,
         x = dyn->sdp->base,
         c = dyn->sdp->C,
         r = dyn->sdp->R,
         o = dyn->sdp->O,
         v = dyn->sdp->V,
         s = dyn->sdp->S,
         i = dyn->sdp->I,
         t = dyn->sdp->T,
         l = dyn->sdp->L,
         p = dyn->sdp->P,
         a = dyn->sdp->A,
         u = dyn->sdp->U,
         last_bv = anchor;

             last_bv != (struct bv *) NULL;

              x = x + (*l)*(dyn->sdp->arr),
              cnt++, ot++, c++, r++, o++, v++, s++, i++, t++, l++, j++)
    {
#ifdef DEBUG
        fprintf(stderr, "dyn->sdp->bound:%x x:%x last_bv:%x\n", (long) dyn->sdp->bound,(long) x, (long) last_bv);
        fprintf(stderr, 
"j:%x ot:%x c:%x  r:%x  o:%x  v:%x  s:%x  i:%x  t:%x  l:%x  p:%x  a:%x  u:%x\n", 
         (long) j, (long) ot, (long) c, (long) r, (long) o, (long) v, (long) s,
          (long) i, (long) t, (long) l, (long) p, (long) a, (long) u );
        fflush(stderr);
#endif
        if (x > (dyn->sdp->bound + 1))
        {
            fprintf(stderr,"%s\n",dyn->statement);
            scarper(__FILE__,__LINE__,"Select descriptor over-flow!");
            exit(1);
        }
/*
 * First, deal with the stored variable name.
 */
        *s = last_bv->bname;
        *c = last_bv->blen;
/*
 * Now the other values returned from the describe()
 */
        *ot = last_bv->dbtype;
        *l =  last_bv->dsize;
        *p = last_bv->prec;
        *a = last_bv->scale;
        *u = last_bv->nullok;
/*
 * Now reallocate the variable pointer to account for the true length
 */        
        *v = x;
        if (scram_cnt)
            *j = 0;               /* Clear Scramble Flag */
        *t = last_bv->dbtype;
/*
 * Now copy the data.
 */
#ifdef DEBUG
        bv_dump(last_bv);
#endif
        switch((int) last_bv->dbtype)
        {
        case SQLITE_NULL:
            **i = -1;
            break;
        case SQLITE_INTEGER:
        case SQLITE_FLOAT:
            **i = 0;
            dbl = sqlite3_column_double(dyn->stmthp, cnt);
            memcpy(x,&dbl,sizeof(dbl));
            **o = sizeof(double);
            break;
        case SQLITE_BLOB:
        case SQLITE3_TEXT:
        default:
            if (last_bv->dsize == 0)
                **i = -1;
            else
            {
                **i = 0;
                x1 = sqlite3_column_text(dyn->stmthp, cnt);
                if (x1 == NULL)
                {
                    scarper(__FILE__,__LINE__,"sqlite3_column_text() returned NULL!?");
                    return 0;
                }
                memcpy(x,x1,last_bv->dsize); /* Includes the trailing NUL */
            }
            **o =last_bv->dsize - 1; /* Excludes the trailing NUL */
            break;
        }
        *r = 0;
        anchor = last_bv;
        last_bv = last_bv->next;
        free(anchor);
    }
    if (x > (dyn->sdp->bound + 1))
    {
        fprintf(stderr,"%s\n",dyn->statement);
        scarper(__FILE__,__LINE__,"Select descriptor over-flow!");
        exit(1);
    }
#ifdef DEBUG
    type_print(stderr,dyn->sdp);
#endif
    ret = sqlite3_step(dyn->stmthp);
    dyn->ret_status = ret;
    dyn->con->ret_status = ret;
    if (ret == SQLITE_OK || ret == SQLITE_DONE || ret == SQLITE_ROW)
    {
        dyn->so_far++;
        if (r == SQLITE_DONE)
            sqlite3_reset(dyn->stmthp);
        return 1;
    }
    else
        return 0;
}
void dbms_commit(sess)
struct sess_con * sess;
{
    (void) dbms_batch(con,"commit;");
    return;
}
