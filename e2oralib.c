 /************************************************************************
 * e2oralib.c - ORACLE support routines for e2sqllib.c
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
#ifdef E2
int __ldmax;               /* Attempt to resolve ORACLE funny */
#endif

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
int r;
/*
 * Deferred flag is true, database behaviour is native
 */
    r = oparse(&(dyn->cda), dyn->statement, -1, 1, 1);
                 /* Parse the entered statement  */
    dyn->ret_status = dyn->cda.rc;
    dyn->con->lda.rc = dyn->cda.rc;
    dyn->con->ret_status = dyn->cda.rc;
    if (r)
        return 0;                        /* Syntax error */
    else
        return 1;
}
/************************************************************************
 * DML Statement Execution
 */
int dbms_exec(dyn)
struct dyn_con * dyn;
{
int ret_arr_size = (dyn->cur_ind) ? (dyn->cur_ind) : 1;
int r;

    r = oexn(&(dyn->cda),ret_arr_size,0);
    dyn->ret_status = dyn->cda.rc;
    dyn->con->ret_status = dyn->cda.rc;
    dyn->con->lda.rc = dyn->cda.rc;
    if (r)
    {
        dyn->rows_sent += dyn->cda.rpc;
        return 0;
    }
    dyn->rows_sent += ret_arr_size;
/*
 * Function Type Values (as found by experiment)
 * 0  - Create Table
 * 3  - Insert
 * 4  - Select
 * 5  - Update
 * 8  - Drop Table
 * 9  - Delete
 * 34 - PL/SQL block
 * 54 - Commit
 * 55 - Rollback
 */
#ifdef DEBUG
    fprintf(stderr, "Oracle function type:%d\n", dyn->cda.ft);
#endif
    if (dyn->cda.ft == 4)
        dyn->is_sel = 1;
    else
        dyn->is_sel = 0;
    return 1;
}
void dbms_error(con)
struct sess_con * con;
{
char buf[4096];
    (void) fprintf(stderr,"ORACLE Error Code %d\n", con->lda.rc);
    oerhms(&(con->lda), con->lda.rc, buf, sizeof(buf));
    (void) fprintf(stderr,"%s\n", buf);
    return;
}
void dbms_roll(con)
struct sess_con * con;
{
    orol(&(con->lda));
    return;
}
int dbms_disconnect(con)
struct sess_con * con;
{
    ologof(&(con->lda));
    return 1;
}
/**********************************************************************
 * Initialise a dynamic statement control block. If the thing already
 * exists, dispose of it.
 */
int dbms_open(d)
struct dyn_con * d;
{
int r = oopen(&(d->cda),&(d->con->lda),(char *) NULL,-1,-1,(char *) NULL,-1);

    d->con->lda.rc = d->cda.rc;
    d->ret_status = d->cda.rc;
    d->con->ret_status = d->cda.rc;
    if (r)
        return 0;                        /* Syntax error */
    else
        return 1;
}
/************************************************************************
 * Destroy a dynamic statement control block
 */
int dbms_close(d)
struct dyn_con * d;
{
int r = oclose(&(d->cda));

    if (r)
        return 0;
    else
        return 1;
}
/********************************************************************
 * Process the Bind variables
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
/*
 * Now execute the bind itself
 */
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
    if ((isdigit(*(s+1)) && obndrn(&(d->cda),
            atoi(s+1),   /* The Variable Number */
            v,           /* The address of the data area              */
            l,           /* The data value length                     */
            t,           /* The ORACLE Data Type                      */
            -1,          /* Packed decimal scale (n/a)                */
            i,           /* The indicator variables                   */
            (unsigned char *) 0,
            -1,
            -1))
    || (!isdigit(*(s+1)) && obndra(&(d->cda),
            s,           /* The variable name                         */
            c,           /* The length of the variable name           */  
            v,           /* The address of the data area              */
            l,           /* The data value length                     */
            t,           /* The ORACLE Data Type                      */
            -1,          /* Packed decimal scale (n/a)                */
            i,           /* The indicator variables                   */
            o,           /* The output lengths                        */
            r,           /* The return codes                          */
            0,            /* Maximum PL/SQL Array size (0 for scalar)  */
            (int *) 0,    /* The returned PL/SQL array size            */
            (unsigned char *) 0,
                          /* COBOL only                                */
            -1,           /* COBOL only                                */
            -1)))         /* COBOL only                                */
    {
    (void) fprintf(stderr,
         "Bind Name %*.*s Type:%d  Length:%d Routine:%d:%d\n%s\n",
                   c,c,s,t,l, isdigit(*(s+1)), atoi(s+1), d->statement);
        d->con->lda.rc = d->cda.rc;
        d->ret_status = d->cda.rc;
        d->con->ret_status = d->cda.rc;
        return 0;
    }
    else
    {
        d->con->lda.rc = d->cda.rc;
        d->ret_status = d->cda.rc;
        d->con->ret_status = d->cda.rc;
        return 1;
    }
}
/*********************************************
 * Attach to the Database. Return a pointer to a structure holding
 * the login data area. Blank the input user/password. 
 */
int dbms_connect(x)
struct sess_con * x;
{
/*
 * Process the command line arguments
 */
char buf[128];
#ifdef DEBUG
    fprintf(stderr, "Connecting %s ....\n", x->uid_pwd);
    fflush(stderr);
#endif
#ifdef OLOG_WORKS
    if (olog(&(x->lda),&(x->hda[0]), x->uid_pwd,(short) -1, (char *) NULL,
           (short) -1, (char *) NULL,(short) -1, (long) OCI_LM_DEF))
#else
    if (orlon(&(x->lda),&(x->hda[0]), x->uid_pwd, -1, (char *) NULL, -1, 0))
#endif
    {
        sprintf(buf, "Login %s Failed, ORACLE Error: %d",
                x->uid_pwd, x->lda.rc);
        scarper(__FILE__, __LINE__, buf);
        return 0;
    }
    else
    {
#ifdef DEBUG
        fputs("Connected.\n", stderr);;
        fflush(stderr);
#endif
        return 1;
    }
}
/***********************************************************************
 * Describe the select list variables. Why is the error return convention
 * different to all the other calls?
 */
int dbms_desc(d,cnt,bv)
struct dyn_con * d;
int cnt;
struct bv *bv;
{
    if (!odescr(&(d->cda),
                    cnt,
                    &(bv->dbsize),
                    &(bv->dbtype),
                    bv->bname,
                    &(bv->blen),
                    &(bv->dsize),
                    &(bv->prec),
                    &(bv->scale),
                    &(bv->nullok)) && !(d->cda.rc))
    {
        d->con->lda.rc = d->cda.rc;
        d->ret_status = d->cda.rc;
        d->con->ret_status = d->cda.rc;
        return 0;
    }
    else
    {
        d->con->lda.rc = d->cda.rc;
        d->ret_status = d->cda.rc;
        d->con->ret_status = d->cda.rc;
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
int l;
short int t;
short int *i;
unsigned short int *o;
unsigned short int *r;
{
int ret;

#ifdef DEBUG
    fflush(stdout);
    (void) fprintf(stderr,"Define Position:%d Type:%d Length: %d\n",
                   cnt,t,l);
    fflush(stderr);
#endif
    ret = odefin(&(d->cda),
            cnt,          /* Position indicator                        */
            v,           /* The address of the data area              */
            l,           /* The data value length                     */
            t,           /* The ORACLE Data Type                      */
            -1,           /* Packed decimal scale (n/a)                */
            i,           /* The indicator variables                   */
            (char *) 0,   /* COBOL only                                */
            -1,           /* COBOL only - (n/a)                        */
            -1,           /* COBOL only - (n/a)                        */
            o,           /* The output lengths                        */
            r);          /* The return codes                          */
    d->con->lda.rc = d->cda.rc;
    d->ret_status = d->cda.rc;
    d->con->ret_status = d->cda.rc;
    if (ret)
        return 0;
    else
        return 1;
}
/************************************************************************
 * Handle an array fetch with the dynamic variables
 */
int dbms_fetch(dyn)
struct dyn_con *dyn;
{
    if (dyn->sdp == (E2SQLDA *) NULL)
        return 0;
    if (ofen(&(dyn->cda),dyn->sdp->arr) && dyn->cda.rc != 1403)
    {
        dyn->con->lda.rc = dyn->cda.rc;
        dyn->ret_status = dyn->cda.rc;
        dyn->con->ret_status = dyn->cda.rc;
        dyn->to_do = 0;
        return 0;
    }
    dyn->ret_status = dyn->cda.rc;
    dyn->con->lda.rc = dyn->cda.rc;
    dyn->con->ret_status = dyn->cda.rc;
    dyn->so_far = dyn->cda.rpc;
    return 1;
}
void dbms_commit(sess)
struct sess_con * sess;
{
    ocom(&(sess->lda));
    return;
}
/*
 * Return the ROWID in conventional format from the cursor data area
 * the following logic is for ORACLE V.7. ORACLE V.8 uses a different format
 * ROWID, that appears to use the characters in the table below for a RADIX 64
 * (ie. using a subset of the bits) representation.
 */
#ifdef ORV7
char* dbms_rowid(d)
struct dyn_con *d;
{
static char rowid[20];
    (void) sprintf(&rowid[0], "%08x.%04x.%04x",
           ( unsigned long ) d->cda.rid.rcs7,
           ( unsigned long ) d->cda.rid.rcs8,
           ( unsigned long ) d->cda.rid.rd.rcs5);
    return &rowid[0];
}
#else
/*
 * Version 8 RID from CDA
    struct {
        struct {
           ub4    rcs4;
           ub2    rcs5;
           ub1    rcs6;
        } rd;
        ub4    rcs7;
        ub2    rcs8;
    } rid;
 */
static char *chartab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
char* dbms_rowid(d)
struct dyn_con *d;
{
static char rowid[19];
char * x;
unsigned int i, j;
    rowid[18] = '\0';
    x = &rowid[17];
    for(i =  d->cda.rid.rcs8; i != 0; x--)
    {
        j = i & 0x3f;
        i = i >> 6;
        *x = chartab[j];
    }
    while (x > &rowid[14])
    {
        *x = 'A';
        x--;
    }
    for(i =  d->cda.rid.rcs7; i != 0; x--)
    {
        j = i & 0x3f;
        i = i >> 6;
        *x = chartab[j];
    }
    if (x == &rowid[8] && d->cda.rid.rd.rcs6 != 0
              && d->cda.rid.rd.rcs6 + j < 64)
        rowid[9] = chartab[j + d->cda.rid.rd.rcs6];
    else
        while (x > &rowid[8])
        {
            *x = 'A';
            x--;
        }
    for (i =  d->cda.rid.rd.rcs5; i != 0; x--)
    {
        j = i & 0x3f;
        i = i >> 6;
        *x = chartab[j];
    }
    while (x > &rowid[5])
    {
        *x = 'A';
        x--;
    }
    for (i =  d->cda.rid.rd.rcs4; i != 0; x--)
    {
        j = i & 0x3f;
        i = i >> 6;
        *x = chartab[j];
    }
    while (x >= &rowid[0])
    {
        *x = 'A';
        x--;
    }
    return &rowid[0];
}
#endif
