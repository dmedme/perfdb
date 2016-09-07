 /************************************************************************
 * e2or9lib.c - ORACLE support routines for e2sqllib.c
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
int r;
short int s;

    dyn->ret_status = 0;
    r = OCIStmtPrepare(dyn->stmthp, 
            dyn->con->errhp, dyn->statement,
                   (ub4)strlen((char *)dyn->statement),
                      (ub4) OCI_NTV_SYNTAX, (ub4) OCI_DEFAULT);
    if (r)
    (void) OCIErrorGet((dvoid *) dyn->con->errhp, (ub4) 1, (text *) NULL,
             &(dyn->ret_status),
                (text *) NULL, (ub4) 0, (ub4) OCI_HTYPE_ERROR);
    dyn->con->ret_status = dyn->ret_status;
#ifdef DEBUG
    fprintf(stderr, "dbms_parse(%s)\n", dyn->statement);
    dbms_error(dyn->con);
#endif
    if (r != OCI_SUCCESS && r != OCI_SUCCESS_WITH_INFO)
        return 0;                        /* Syntax error */
/*
 * Find out whether the statement is a select
 */
    (void) OCIAttrGet((dvoid *) dyn->stmthp,
                         OCI_HTYPE_STMT,
                         (dvoid *)&(s), (ub2 *)0,
                         OCI_ATTR_STMT_TYPE, dyn->con->errhp);
#ifdef DEBUG
    fprintf(stderr, "Oracle function type:%d\n", s);
#endif
    if (s == OCI_STMT_SELECT)
        dyn->is_sel = s;
    else
        dyn->is_sel = 0;
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
    dyn->ret_status = 0;
    dyn->con->ret_status = 0;
    r = OCIStmtExecute(dyn->con->svchp, dyn->stmthp, dyn->con->errhp,
                       (ub4) 
        (dyn->is_sel)? 0 : ret_arr_size, (ub4) 0,
                       (CONST OCISnapshot*) 0, (OCISnapshot*) 0,
                       (ub4) OCI_DEFAULT);
    if (r)
    {
        (void) OCIErrorGet((dvoid *) dyn->con->errhp, (ub4) 1, (text *) NULL,
             &(dyn->ret_status),
                (text *) NULL, (ub4) 0, (ub4) OCI_HTYPE_ERROR);
        dyn->con->ret_status = dyn->ret_status;
    }
#ifdef DEBUG
    fprintf(stderr, "dbms_exec(%s) r=%d\n", dyn->statement, r);
    dbms_error(dyn->con);
#endif
    if (r != OCI_SUCCESS && r != OCI_SUCCESS_WITH_INFO)
        return 0;
    r = OCIAttrGet((dvoid *) dyn->stmthp,
                         OCI_HTYPE_STMT,
                         (dvoid *)&(dyn->rows_sent), (ub4 *)0,
                         OCI_ATTR_ROW_COUNT, dyn->con->errhp);
#ifdef DEBUG
    fprintf(stderr, "dbms_exec(%s) r=%d rows=%d status=%d con->status=%d\n",
            dyn->statement, r, dyn->rows_sent, dyn->ret_status,
            dyn->con->ret_status);
#endif
    return 1;
}
void dbms_error(con)
struct sess_con * con;
{
char buf[4096];
sb4   errcode = 0;

    (void) OCIErrorGet((dvoid *) con->errhp, (ub4) 1, (text *) NULL, &errcode,
                buf, (ub4) sizeof(buf), (ub4) OCI_HTYPE_ERROR);
    (void) fprintf(stderr,"ORACLE Error Code %d\n%.*s", errcode,
             ((errcode == 0) ? 0 : sizeof(buf)), buf);
    if (errcode != 0)
    {
        OCIHandleFree((dvoid *) con->errhp, (ub4) OCI_HTYPE_ERROR);
/*
 * Allocate a new error handle
 */
        OCIHandleAlloc((dvoid *) con->envhp, (dvoid **) &(con->errhp),
                     (ub4) OCI_HTYPE_ERROR, (size_t) 0, (dvoid **) 0);
    }
    return;
}
void dbms_roll(con)
struct sess_con * con;
{
    OCITransRollback(con->svchp, con->errhp, OCI_DEFAULT);
    return;
}
int dbms_disconnect(con)
struct sess_con * con;
{
    OCILogoff(con->svchp, con->errhp);
    return 1;
}
/**********************************************************************
 * Initialise a dynamic statement control block. If the thing already
 * exists, dispose of it.
 */
int dbms_open(d)
struct dyn_con * d;
{
    if (OCIHandleAlloc((dvoid *) d->con->envhp, (dvoid **) &(d->stmthp),
                     (ub4)OCI_HTYPE_STMT, (CONST size_t) 0, (dvoid **) 0))
        return 0;
    else
        return 1;
}
/************************************************************************
 * Destroy a dynamic statement control block
 */
int dbms_close(d)
struct dyn_con * d;
{

    if (OCIHandleFree((dvoid *) d->stmthp, (ub4) OCI_HTYPE_STMT))
        return 0;
    else
        return 1;
}
/********************************************************************
 * Process the Bind variables
 */
int dbms_bind(d, s, c, v, l, a, p, t, i, o, r, b)
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
OCIBind *b;
{
int ret;
/*
 * Now execute the bind itself
 */
    d->ret_status = 0;
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
    if ((isdigit(*(s+1)) &&
         OCIBindByPos(d->stmthp, &b, d->con->errhp, 
            (ub4) atoi(s+1),   /* The Variable Number */
            (dvoid *) v,       /* The address of the data area              */
            (sb4) l,           /* The data value length                     */
            (ub2) t,           /* The ORACLE Data Type                      */
            (dvoid *) i,       /* The indicator variables                   */
            (ub2 *) o,         /* The output lengths                        */
            (ub2 *) r,         /* The return codes                          */
            (ub4) 0,           /* Maximum PL/SQL Array size (0 for scalar)  */
            (ub4 *) 0,    /* The returned PL/SQL array size            */
            (ub4) OCI_DEFAULT))
    || (!isdigit(*(s+1)) &&
        OCIBindByName(d->stmthp, &b, d->con->errhp, 
            (text *) s,          /* The variable name                         */
            (sb4) c,           /* The length of the variable name           */  
            (dvoid *) v,       /* The address of the data area              */
            (sb4) l,           /* The data value length                     */
            (ub2) t,           /* The ORACLE Data Type                      */
            (dvoid *) i,       /* The indicator variables                   */
            (ub2 *) o,           /* The output lengths                        */
            (ub2 *) r,           /* The return codes                          */
            (ub4) 0,            /* Maximum PL/SQL Array size (0 for scalar)  */
            (ub4 *) 0,    /* The returned PL/SQL array size            */
            (ub4) OCI_DEFAULT)))
    {
        (void) OCIErrorGet((dvoid *) d->con->errhp, (ub4) 1, (text *) NULL,
             &(d->ret_status),
                (text *) NULL, (ub4) 0, (ub4) OCI_HTYPE_ERROR);
        d->con->ret_status = d->ret_status;
        (void) fprintf(stderr,
         "Error: %d Bind Name %*.*s Type:%d  Length:%d Routine:%d:%d\n%s\n",
                 d->ret_status,
                   c,c,s,t,l, isdigit(*(s+1)), atoi(s+1), d->statement);
        return 0;
    }
    else
    {
        d->con->ret_status = d->ret_status;
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
char buf[512];
char * xp = (char *) NULL;
text *cstring = (text *) x->uid_pwd;
char * user;
char * pwd;
char * sv;

    fprintf(stderr, "Connecting %s ....\n", x->uid_pwd);
    fflush(stderr);
/*
 * Split up the user name, pwd and database name
 */
    if ((user = nextasc(x->uid_pwd,'/','\\')) != (char *) NULL)
        user = strdup(user);
    else
        user = strdup("");
    if ((pwd = nextasc(NULL, '@','\\')) != (char *) NULL)
        pwd = strdup(pwd);
    else
        pwd = strdup("");
    if ((sv = nextasc(NULL, '@','\\')) != (char *) NULL)
        sv = strdup(sv);
    else
        sv = strdup("");
    if (OCIInitialize((ub4) OCI_OBJECT, (dvoid *)0,
                    (dvoid * (*)(dvoid *, size_t)) 0,
                    (dvoid * (*)(dvoid *, dvoid *, size_t))0,
                    (void (*)(dvoid *, dvoid *)) 0 ))
        xp = "FAILED: OCIInitialize()\n";
    else
/*
 * Inititialize the OCI Environment
 */
    if (OCIEnvInit((OCIEnv **) &(x->envhp), (ub4) OCI_DEFAULT,
                 (size_t) 0, (dvoid **) 0 ))
        xp = "FAILED: OCIEnvInit()\n";
    else
/*
 * Allocate a service handle
 */
    if (OCIHandleAlloc((dvoid *) x->envhp, (dvoid **) &(x->svchp),
                     (ub4) OCI_HTYPE_SVCCTX, (size_t) 0, (dvoid **) 0))
        xp = "FAILED: OCIHandleAlloc() on svchp\n";
    else
/*
 * Allocate an error handle
 */
    if (OCIHandleAlloc((dvoid *) x->envhp, (dvoid **) &(x->errhp),
                     (ub4) OCI_HTYPE_ERROR, (size_t) 0, (dvoid **) 0))
        xp = "FAILED: OCIHandleAlloc() on errhp\n";
    else
/*
 * Allocate a server handle
 */
    if (OCIHandleAlloc((dvoid *) x->envhp, (dvoid **) &(x->srvhp),
                     (ub4) OCI_HTYPE_SERVER, (size_t) 0, (dvoid **) 0))
        xp = "FAILED: OCIHandleAlloc() on srvhp\n";
    else
/*
 * Allocate a authentication handle
 */
    if (OCIHandleAlloc((dvoid *) x->envhp, (dvoid **) &(x->authp),
                     (ub4) OCI_HTYPE_SESSION, (size_t) 0, (dvoid **) 0))
        xp = "FAILED: OCIHandleAlloc() on authp\n";
/*
 * Attach the server
 */
    else
    if (OCIServerAttach(x->srvhp, x->errhp, (text *) sv,
                     (sb4) strlen(sv), (ub4) OCI_DEFAULT))
        xp = "FAILED: OCIServerAttach()\n";
    else
/*
 * Set the server handle in the service handle
 */
    if (OCIAttrSet((dvoid *) x->svchp, (ub4) OCI_HTYPE_SVCCTX,
                 (dvoid *) x->srvhp, (ub4) 0, (ub4) OCI_ATTR_SERVER, 
                             x->errhp))
        xp = "FAILED: OCIAttrSet() server attribute\n";
    else
/*
 * Set attributes in the authentication handle 
 */
    if (OCIAttrSet((dvoid *) x->authp, (ub4) OCI_HTYPE_SESSION,
                 (dvoid *) user, (ub4) strlen(user),
                 (ub4) OCI_ATTR_USERNAME, x->errhp))
        xp = "FAILED: OCIAttrSet() userid\n";
    else
    if (OCIAttrSet((dvoid *) x->authp, (ub4) OCI_HTYPE_SESSION,
                 (dvoid *) pwd, (ub4) strlen(pwd),
                 (ub4) OCI_ATTR_PASSWORD, x->errhp))
        xp = "FAILED: OCIAttrSet() passwd\n";
/*
 *  else
 *  if (OCISessionBegin(x->svchp, x->errhp, x->authp, (ub4) OCI_CRED_EXT,
 *                                        (ub4) OCI_DEFAULT))
 */
    else
    if (OCISessionBegin(x->svchp, x->errhp, x->authp, (ub4) OCI_CRED_RDBMS,
                                          (ub4) OCI_DEFAULT))
        xp = "FAILED: OCISessionBegin()\n";
    else
    if (OCIAttrSet((dvoid *) x->svchp, (ub4) OCI_HTYPE_SVCCTX,
                 (dvoid *) x->authp, (ub4) 0, (ub4) OCI_ATTR_SESSION, 
                             x->errhp))
        xp = "FAILED: OCIAttrSet() session attribute\n";
    else
    {
#ifdef DEBUG
        fputs("Connected.\n", stderr);;
        fflush(stderr);
#endif
        free(user);
        free(pwd);
        free(sv);
        return 1;
    }
    sprintf(buf, "Login %s/%s@%s Failed at %s", user,pwd,sv, xp);
    dbms_error(x);
    scarper(__FILE__, __LINE__, buf);
    free(user);
    free(pwd);
    free(sv);
    return 0;
}
/***********************************************************************
 * Describe the select list variables
 */
int dbms_desc(d,cnt,bv)
struct dyn_con * d;
int cnt;
struct bv *bv;
{
OCIParam * pp;
unsigned char x;

    d->ret_status = 0;
    if (!OCIParamGet((dvoid *) d->stmthp, OCI_HTYPE_STMT,
           d->con->errhp, (dvoid **) &pp, cnt))
    { 
    short int l;
        (void) OCIAttrGet(pp, OCI_DTYPE_PARAM, &l, 0,
                        OCI_ATTR_DATA_SIZE, d->con->errhp); 
        bv->dsize = l;
        bv->dbsize = l;
        (void) OCIAttrGet(pp, OCI_DTYPE_PARAM, &(bv->dbtype), 0,
                        OCI_ATTR_DATA_TYPE, d->con->errhp);
        if (bv->dbtype == SQLT_NTY)
        {
            (void) OCIAttrGet(pp, OCI_DTYPE_PARAM, &(bv->tname), &bv->tlen,
                        OCI_ATTR_TYPE_NAME, d->con->errhp);
            (void) OCIAttrGet(pp, OCI_DTYPE_PARAM, &(bv->sname), &bv->slen,
                        OCI_ATTR_SCHEMA_NAME, d->con->errhp);
        }
        (void) OCIAttrGet(pp, OCI_DTYPE_PARAM, &(bv->bname), &bv->blen,
                        OCI_ATTR_NAME, d->con->errhp);
        (void) OCIAttrGet(pp, OCI_DTYPE_PARAM, &(bv->prec), 0,
                        OCI_ATTR_PRECISION, d->con->errhp);
        (void) OCIAttrGet(pp, OCI_DTYPE_PARAM, &x, 0,
                        OCI_ATTR_SCALE, d->con->errhp);
        bv->scale = x;
        (void) OCIAttrGet(pp, OCI_DTYPE_PARAM, &x, 0,
                        OCI_ATTR_IS_NULL, d->con->errhp);
        bv->nullok = x;
        d->con->ret_status = d->ret_status;
#ifdef DEBUG
        fprintf(stderr, "desc:%.*s type:%u size:%u\n", bv->blen, bv->bname,
                                                       bv->dbtype, bv->dbsize);
#endif
        return 1;
    }
    else
    {
        (void) OCIErrorGet((dvoid *) d->con->errhp, (ub4) 1, (text *) NULL,
             &(d->ret_status),
                (text *) NULL, (ub4) 0, (ub4) OCI_HTYPE_ERROR);
        d->con->ret_status = d->ret_status;
        dbms_error(d->con);
        return 0;
    }
}
/*
 * Dummy Callback to skip output data we don't know how to handle
 */
sb4 cdf_fetch_buffer(ctx, defnp, iter, bufpp, alenpp, piecep, indpp, rcpp)
dvoid *ctx;
OCIDefine *defnp;
ub4 iter;
dvoid **bufpp;
ub4 **alenpp;
ub1 *piecep;  
dvoid **indpp;
ub2 **rcpp;
{
static char buf[128];
static int len = 128;

    *bufpp = buf;
    *alenpp = &len;
    *piecep = OCI_ONE_PIECE;
    return OCI_CONTINUE;
}
/*
 * Now execute the define itself
 */
int dbms_define(d, cnt, v, l, t, i, o, r, b, n, nl, s, sl)
struct dyn_con * d;
int cnt;
char * v;
int l;
short int t;
short int *i;
unsigned short int *o;
unsigned short int *r;
OCIDefine ** b;
char *n;
int nl;
char *s;
int sl;
{
int ret;

    d->ret_status = 0;
#ifdef DEBUG
    fflush(stdout);
    (void) fprintf(stderr,"Define Position:%d Type:%d Length: %d Handlep:%x\n",
                   cnt,t,l, (long) b);
    fflush(stderr);
#endif
    if (t == SQLT_CLOB || t == SQLT_BLOB)
    {
#ifdef LOCATORS_WORK
        (void) OCIDescriptorAlloc((dvoid *) d->con->envhp, (dvoid **) v,
                           (ub4) OCI_DTYPE_LOB, (size_t) 0, (dvoid **) 0);
        if (*((char **) v) != NULL)
        {
            dyn_descr_note(d, *((char **) v), OCIDescriptorFree, OCI_DTYPE_LOB);
            l = sizeof(v);
        }
#else
    if (t == SQLT_CLOB)
        t = ORA_LONG;
    else
        t = ORA_LONG_RAW;
#endif
    }
    ret = OCIDefineByPos(d->stmthp, b, d->con->errhp,
            (ub4) cnt,   /* Position indicator                        */
            (dvoid *) v, /* The address of the data area              */
            (sb4) l,     /* The data value length                     */
            (ub2) t,     /* The ORACLE Data Type                      */
            (dvoid *) i, /* The indicator variables                   */
            (ub2 *) o,   /* The output lengths                        */
            (ub2 *) r,   /* The return codes                          */
       (t == SQLT_CLOB ||
        t == SQLT_BLOB ||
        t == SQLT_NTY)? OCI_DYNAMIC_FETCH : OCI_DEFAULT);
    if (ret)
        (void) OCIErrorGet((dvoid *) d->con->errhp, (ub4) 1, (text *) NULL,
             &(d->ret_status),
                (text *) NULL, (ub4) 0, (ub4) OCI_HTYPE_ERROR);
    d->con->ret_status = d->ret_status;
    if (ret)
    {
        return 0;
    }
    else
    if (t == SQLT_CLOB || t == SQLT_BLOB || t == SQLT_NTY)
    {
        if (t == SQLT_NTY)
        {
        OCIType *tdo;

            ret = OCITypeByName ( d->con->envhp, d->con->errhp,
                          d->con->svchp, 
                          (text *) s,
                          (ub4) sl,
                          (text* ) n,
                          (ub4) nl,
                          NULL,
                          (ub4) 0,
                          OCI_DURATION_SESSION,
                          OCI_TYPEGET_ALL,
                          (OCIType **) &tdo );
            if (ret)
            (void) OCIErrorGet((dvoid *) d->con->errhp, (ub4) 1, (text *) NULL,
                 &(d->ret_status),
                    (text *) NULL, (ub4) 0, (ub4) OCI_HTYPE_ERROR);
            d->con->ret_status = d->ret_status;
            if (ret)
                return 0;
            ret = OCIDefineObject(*b, d->con->errhp, tdo, NULL,
                                      NULL, NULL, NULL);
            if (ret)
            (void) OCIErrorGet((dvoid *) d->con->errhp, (ub4) 1, (text *) NULL,
                 &(d->ret_status),
                    (text *) NULL, (ub4) 0, (ub4) OCI_HTYPE_ERROR);
            d->con->ret_status = d->ret_status;
            if (ret)
                return 0;
        }
        ret = OCIDefineDynamic(*b, d->con->errhp, 
                          NULL, (OCICallbackDefine) cdf_fetch_buffer);
        if (ret)
        (void) OCIErrorGet((dvoid *) d->con->errhp, (ub4) 1, (text *) NULL,
             &(d->ret_status),
                (text *) NULL, (ub4) 0, (ub4) OCI_HTYPE_ERROR);
        d->con->ret_status = d->ret_status;
        if (ret)
            return 0;
    }
    return 1;
}
/************************************************************************
 * Handle an array fetch with the dynamic variables
 */
int dbms_fetch(dyn)
struct dyn_con *dyn;
{
int ret;

    dyn->ret_status = 0;
    dyn->con->ret_status = 0;
    if (dyn->sdp == (E2SQLDA *) NULL)
        return 0;
    ret = OCIStmtFetch2(dyn->stmthp,
                       dyn->con->errhp, (ub4) 
                dyn->sdp->arr, (ub2) OCI_FETCH_NEXT, (sb4) 0,
                               (ub4) OCI_DEFAULT);
    if (ret)
    (void) OCIErrorGet((dvoid *) dyn->con->errhp, (ub4) 1, (text *) NULL,
             &(dyn->ret_status),
                (text *) NULL, (ub4) 0, (ub4) OCI_HTYPE_ERROR);
    dyn->con->ret_status = dyn->ret_status;
#ifdef DEBUG
    fprintf(stderr, "dbms_fetch(%s)\n", dyn->statement);
    dbms_error(dyn->con);
#endif
    if (ret != OCI_SUCCESS
     && ret != OCI_SUCCESS_WITH_INFO
     && dyn->ret_status != 1403
     && dyn->ret_status != 1002
     && dyn->ret_status != 0)
        return 0;
    (void) OCIAttrGet((dvoid *) dyn->stmthp,
                         OCI_HTYPE_STMT,
                         (dvoid *)&(dyn->rows_sent), (ub4 *)0,
                         OCI_ATTR_ROW_COUNT, dyn->con->errhp);
    dyn->so_far = dyn->rows_sent;
    return 1;
}
void dbms_commit(sess)
struct sess_con * sess;
{
    OCITransCommit(sess->svchp, sess->errhp, OCI_DEFAULT);
    return;
}
/*
 * Return the ROWID in conventional format from the cursor data area
 * the following logic is for ORACLE V.7. ORACLE V.8 uses a different format
 * ROWID, that appears to use the characters in the table below for a RADIX 64
 * (ie. using a subset of the bits) representation.
 */
char* dbms_rowid(d)
struct dyn_con *d;
{
OCIRowid * rowp;

    (void) OCIDescriptorAlloc((dvoid *) d->con->envhp, (dvoid **) &(rowp),
                           (ub4) OCI_DTYPE_ROWID, (size_t) 0, (dvoid **) 0);
    if (rowp != NULL)
        dyn_descr_note(d, rowp, OCIDescriptorFree, OCI_DTYPE_ROWID);
    (void) OCIAttrGet (d->stmthp,
                    OCI_HTYPE_STMT,
                    rowp,                      /* get the current rowid */
                    0,
                    OCI_ATTR_ROWID,
                    d->con->errhp);
    return (char *) rowp;
}
