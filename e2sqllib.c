/************************************************************************
 * e2sqllib.c - Common support routines for tabdiff and sqldrive.
 *
 * These routines are supposed to be database-independent, but they
 * aren't entirely; knowledge of ORACLE datatypes is scattered about.
 */
static char * sccs_id =  "@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1993\n";
char * strdup();
#ifdef SYBASE
#define BD_SIZE 1
#define SD_SIZE 1
#else
#ifdef SQLITE3
#define BD_SIZE 1
#define SD_SIZE 1
#else
#define BD_SIZE 2
#define SD_SIZE 10
#endif
#endif
static int bd_size = BD_SIZE;
static int sd_size = SD_SIZE;
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
#ifdef SOLAR
#define __inline
#endif
#ifdef PTX
#define __inline
#endif
#ifdef HP7
#define __inline
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
#include "e2conv.h"
#include "tabdiff.h"
static int char_long = 80;
/************************************************************************
 * Set the length for long columns
 */
void set_long(l)
int l;
{
    if (l > 0)
        char_long = l;
    return;
}
static struct dict_con * cur_dict;
/************************************************************************
 * Return the current dictionary; return its previous value
 */
struct dict_con * get_cur_dict()
{
    return cur_dict;
}
/************************************************************************
 * Set the current dictionary; return its previous value
 */
struct dict_con * set_cur_dict(dict)
struct dict_con * dict;
{
struct dict_con * prev_dict = cur_dict;
    cur_dict = dict;
    return prev_dict;
}
/************************************************************************
 * Set the values for bind descriptor array sizes.
 */
void set_def_binds(b,s)
int b;
int s;
{
#ifndef SYBASE
    if (b > 0)
        bd_size = b;
#endif
    if (s > 0)
        sd_size = s;
    return;
}
int get_sd_size()
{
    return sd_size;
}
/************************************************************************
 * Get a list of the columns for a table, and store using the select
 * descriptor.
 */
void get_cols(dyn,tab)
struct dyn_con * dyn;
char * tab;
{
    char buf[128];
    (void) sprintf(buf,"select * from %s",tab);
    dyn->statement = buf;
    prep_dml(dyn);
    exec_dml(dyn);
    if (dyn->sdp == (E2SQLDA *) NULL)
        desc_sel(dyn);
    dyn->statement = (char *) NULL;
    return;
}
/****************************************************************************
 * Allocate a cursor and parse a SQL statement
 */
void curse_parse(con,dyn,curs,stmt)
struct sess_con * con;
struct dyn_con ** dyn;
int curs;
char * stmt;
{
    if ((*dyn = dyn_init(con, curs)) != (struct dyn_con *) NULL)
    {
        (*dyn)->statement = stmt;
        prep_dml(*dyn);
    }
    return;
}
/************************************************************************
 * Statement Preparation
 */
void prep_dml(dyn)
struct dyn_con * dyn;
{
/*
 * Deferred flag is true, database behaviour is native
 */
#ifdef DEBUG
    fprintf(stderr, "prep_dml(%lx) = %s\n", (long) dyn, dyn->statement);
    fflush(stderr);
#endif
    dyn_reinit(dyn);
    if (!dbms_parse(dyn))
                 /* Parse the entered statement  */
    {
        fprintf(stderr,"%s\n",dyn->statement);
        scarper(__FILE__,__LINE__,"prepare failed");
    }
#ifdef SYBASE
    if (strncasecmp(dyn->statement,"create ",7))
#else
    if (strncasecmp(dyn->statement,"explain",7))
#endif
        desc_bind(dyn);
#ifdef DONT_KNOW
    dyn->chars_sent += strlen(dyn->statement); /* Length of SQL Statements            */
    for (x = dyn->statement, i = 8; i; x++)
    {
        if (i != 8 ||
            ( *x != ' ' && *x != '\n' && *x != '\r' && *x != '\t'))
           i--;
        if (islower(*x))
            *x = _toupper(*x);
    }
    x -= 8;
    if (!strncmp(x,"SELECT",6) && isspace((int) *(x + 6)))
        dyn->is_sel = 1;
#endif
    return;
}
/************************************************************************
 * DML Statement Execution
 */
void exec_dml(dyn)
struct dyn_con * dyn;
{
char buf[4096];
int i;
#ifdef DEBUG
    fprintf(stderr, "exec_dml(%lx) = %s\n", (long) dyn, dyn->statement);
    fflush(stderr);
#endif
#ifdef SYBASE
    if (dyn->need_dynamic)
        dbms_fix_types(dyn);
    else
#endif
    if (dyn->reb_flag > 0)
    {
    register E2SQLDA * rd = dyn->bdp;

        dbms_parse(dyn);      /* Must re-parse to change the data type; but must
                                 also bind the others.   */
/*
 * Now execute the bind itself
 */
        for (i = 0; i < rd->F; i++)
        {
#ifdef SQLITE3
            if (rd->C[i] == 1)
            {
            short int *fo = (short int *) rd->O[i];
            int cnt;

                for (cnt = 0; cnt < rd->arr; cnt++)
                    *fo++ = (short int) (i+1);
            }
            if (rd->T[i] == SQLITE3_TEXT)
                rd->L[i] = strlen(rd->V[i]);
#else
            if (!dbms_bind(dyn,
                rd->S[i],     /* The variable name                         */
                rd->C[i],     /* The length of the variable name           */  
                rd->V[i],     /* The address of the data area              */
                rd->L[i],     /* The data value length                     */
                rd->P[i],     /* The precision                             */
                rd->A[i],     /* The scale                                 */
                rd->T[i],     /* The ORACLE Data Type                      */
                rd->I[i],     /* The indicator variables                   */
                rd->O[i],     /* The output lengths                        */
                rd->R[i]      /* The return codes                          */
#ifdef OR9
               ,rd->bindhp[i]
#endif
                        )) 
            {
                fprintf(stderr,"%s\n",dyn->statement);
                scarper(__FILE__,__LINE__,"variable rebind failed");
            }
#endif
        }
    }
    dyn->reb_flag = 0;
    if (!dbms_exec(dyn))
    {
        fprintf(stderr,"The following statement failed:\n%s\nData:\n",
                dyn->statement);
        if (dyn->bdp != (E2SQLDA *) NULL)
        {
            type_print(stderr,dyn->bdp);
            desc_print(stderr,(FILE *) NULL,
                       dyn->bdp,dyn->cur_ind, (char *) NULL);
        }
        sprintf(buf, "rows_sent: %d so_far:%d cur_ind: %d to_do: %d\n",
             dyn->rows_sent, dyn->so_far, dyn->cur_ind, dyn->to_do);
        scarper(__FILE__,__LINE__,buf);
    }
#ifdef OR9
/*    if (dyn->is_sel && dyn->sdp == (E2SQLDA *) NULL)   */
    if (dyn->is_sel)
        desc_sel(dyn);
#endif
    if (dyn->so_far == 0)
        dyn->so_far = dyn->rows_sent;
    dyn->cur_ind = 0;
    return;
}
#ifdef INGRES
int dynhash(s1, modulo)
char * s1;
int modulo;
{
    return string_hh( ( (struct dyn_con *) s1)->hash_key, modulo);
}
int dyncomp(s1,s2)
char * s1;
char * s2;
{
    return strcmp(((struct dyn_con *) s1)->hash_key,
                 ( (struct dyn_con *) s2)->hash_key);
}
#endif
/**********************************************************************
 * Initialise a dynamic statement control block. If the thing already
 * exists, dispose of it.
 */
struct dyn_con * dyn_init(s,n)
struct sess_con * s;
int n;
{
struct dyn_con * d;
#ifdef DEBUG
    fprintf(stderr, "dyn_init(%lx,%d)\n", (long) s, n);
    fflush(stderr);
#endif
    if ((d = dyn_find(s, n)) != (struct dyn_con *) NULL)
        dyn_kill(d);
    if ((d = (struct dyn_con *) malloc(sizeof(*d)))
           == (struct dyn_con *) NULL)
        return d;
    memset((char *) d, 0, sizeof(*d));
    d->con = s;
    d->is_sel = 0;
    if (!dbms_open(d))
    {
        scarper(__FILE__,__LINE__,"Cursor open failed");
        return (struct dyn_con *) NULL; 
    }
    d->cnum = n;
    d->reb_flag = -1;
    insert(s->ht, (char *) n, (char *) d);     /* Allocate the hash entry */
#ifdef INGRES
    d->head_bv = NULL;
    d->tail_bv = NULL;
#endif
    d->anchor = NULL;
    return d;
}
/************************************************************************
 * Destroy a dynamic statement control block
 */
void dyn_kill(d)
struct dyn_con * d;
{
#ifdef DEBUG
    fprintf(stderr, "dyn_kill(%lx) = %d\n", (long) d, d->cnum);
    dyn_dump(d);
    fflush(stderr);
#endif
    if (!dbms_close(d))
        scarper(__FILE__,__LINE__,"Cursor close failed");
    dyn_reinit(d);
    hremove(d->con->ht,(char *) d->cnum);
#ifdef INGRES
    zap_stat_hash(d);
#endif
    free(d);                     /* Free the structure                   */
    return;
}
void bv_dump(bv)
struct bv * bv;
{
    fprintf(stderr,"BV:%-*.*s\n\
dbsize:%d dbtype:%d dsize:%d prec:%d scale:%d nullok:%d\n",
           bv->blen, bv->blen, bv->bname, bv->dbsize, bv->dbtype, bv->dsize,
                bv->prec, bv->scale, bv->nullok);
    fflush(stderr);
    return;
}
#ifdef INGRES
static int do_bv(anchor,bname,blen, ret_bv)
struct bv ** anchor;
char * bname;
int blen;
struct bv ** ret_bv;
{
struct bv * bv = *anchor, * last_bv = (struct bv *) NULL;

    if (*anchor == (struct bv *) NULL)
    {
        last_bv = (struct bv *) malloc(sizeof(struct bv));
        *anchor = last_bv;
    }
    else
    {
        last_bv->next = (struct bv *) malloc(sizeof(struct bv));
        last_bv = last_bv->next;
    }
    last_bv->next = (struct bv *) NULL;
    last_bv->bname = bname;
    last_bv->blen = blen;
#ifdef DEBUG
    printf("%*.*s\n",last_bv->blen,last_bv->blen,last_bv->bname);
#endif
    *ret_bv = last_bv;
    return 1;
}
/********************************************************************
 * Process the Bind variables
 */
void desc_bind(d)
struct dyn_con * d;
{
/*
 * Scan the statement for ~ markers. Any found, stash them. Having found
 * them all, allocate the structure to hold them, and execute the bind
 * functions.
 */
struct bv * anchor = (struct bv *) NULL;
struct bv * this_bv;
struct bv * found_bv;
register char * x, *x1;
int len;
int cnt =0;
char ** v;
char ** s;
short int **i;
int *l;
short int *c, *t;
short int *p, *a;
unsigned short int **r, **o;
int row_len;                /* Width that needs to be allocated */
/*
 * Begin; find out how many distinct bind variables there are.
 */
#ifdef DEBUG
    fprintf(stderr, "desc_bind(%lx)\n", (long) d);
    fflush(stderr);
#endif
    if (d->head_bv != NULL)
    {
        if (d->queryType == IIAPI_QT_DEF_REPEAT_QUERY)
        {
            memcpy((char *) &(d->repti1), d->head_bv->val, sizeof(II_INT4));
            memcpy((char *) &(d->repti2),
                       d->head_bv->next->val, sizeof(II_INT4));
            memcpy((char *) &(d->reptid),
                       d->head_bv->next->next->val, 64);
            this_bv = d->head_bv->next->next->next;
        }
        else
            this_bv = d->head_bv;
        for (anchor = this_bv, row_len = 0, cnt = 0;
                this_bv != (struct bv *) NULL;
                     this_bv = this_bv->next)
        {
            cnt++;
            row_len += 1 + this_bv->dsize;
            if (row_len >2048)
                row_len -= this_bv->dsize;
        } 
    }
    else
    for (x = d->statement, row_len = 0; *x != '\0';)
    {
        switch(*x)
        {
        case '~':
            if (*(x+1) != 'V' || *(x + 2) != ' ')
            {
                x ++;
                break;
            }
/*
 * We should have the details of these columns somewhere, but I haven't
 * written the code to do this yet.
 */
            len = 3;
            if (do_bv(&anchor,x, len, &this_bv))
            {
                this_bv->dbtype = ORA_VARCHAR2;
                this_bv->dsize = char_long;
                this_bv->dbsize = char_long;
                row_len += this_bv->dsize;
                cnt++;
            }
            x++;
            break;
        case '\'':
            for (;;)
            {
                x++;
                if (*x == '\0')
                    break; 
                if (*x == '\'')
                {
                    x++;
                    if (*x != '\'')
                        break;
                }
            }
            break;
        case '"':
            for (;;)
            {
                x++;
                if (*x == '\0')
                    break; 
                if (*x == '"')
                {
                    x++;
                    break;
                }
            }
            break;
        default:
            x++;
            break;
        }
    }
    sqlclu(d->bdp);
    if (cnt == 0)
    {
        d->bdp = (E2SQLDA *) NULL;
        return;
    }
    else
/*
 * Now allocate the items that can be allocated now
 */
        d->bdp = sqlald(cnt, bd_size, 1 + row_len/cnt);
/*
 *  Loop through the bind variables, storing them in association with
 *  the SQLDA
 */
    for (c = d->bdp->C,
         r = d->bdp->R,
         o = d->bdp->O,
         v = d->bdp->V,
         s = d->bdp->S,
         i = d->bdp->I,
         t = d->bdp->T,
         l = d->bdp->L,
         p = d->bdp->P,
         a = d->bdp->A,
#ifdef OR9
         b = d->bdp->bindhp,
#endif
         x = d->bdp->base,
         this_bv = anchor;
             this_bv != (struct bv *) NULL;
#ifdef OR9
                 b++,
#endif
                 c++, r++, o++, v++, s++, i++, t++, l++, p++, a++)
    {
/*
 * First, deal with the stored variable name. It is not null terminated,
 * since it is embedded in the SQL statement.
 */
    short int *fo = (short int *) *o;

        *s = this_bv->bname;
        *c = this_bv->blen;
        *t = this_bv->dbtype;
        *l = this_bv->vlen;
        *p = this_bv->prec;
        *a = this_bv->scale;
        *v = x;
#ifdef DEBUG
        bv_dump(this_bv);
#endif
        if (d->head_bv != NULL && this_bv->val != (char *) NULL)
            memcpy(x, this_bv->val, this_bv->vlen);
        for (cnt = 0; cnt < d->bdp->arr; cnt++)
            *fo++ = (short int) this_bv->dsize;
        x = x + (this_bv->dsize)*bd_size;
        anchor = this_bv;
        this_bv = this_bv->next;
        if (d->head_bv == NULL)
            free(anchor);
        if (x > (d->bdp->bound + 1))
        {
            fprintf(stderr, "Logic Error: Overrun in desc_bind: %s\n",
               (d->statement == NULL) ? "No statement" : d->statement);
            fflush(stderr);
            return;
        }
    }
    return;
}
#else
/********************************************************************
 * Process the Bind variables
 */
void desc_bind(d)
struct dyn_con * d;
{
/*
 * Scan the statement for :, @ or ? markers. Any found, stash them. Having found
 * them all, allocate the structure to hold them, and execute the bind
 * functions, using obndra().
 */
struct bv * anchor = (struct bv *) NULL;
struct bv * this_bv;
struct bv * found_bv;
#ifdef OR9
char **b;
#endif
register char * x, *x1;
int len;
int cnt =0;
char ** v;
char ** s;
#ifdef SYBASE
CS_SMALLINT **i;
CS_INT *l;
CS_INT  *c, *t;
CS_INT  *p, *a;
CS_INT  **r, **o;
#else
short int **i;
int *l;
short int *c, *t;
short int *p, *a;
unsigned short int **r, **o;
#endif
int row_len;                /* Width that needs to be allocated */
/*
 * Begin; find out how many distinct bind variables there are.
 */
#ifdef DEBUG
    fprintf(stderr, "desc_bind(%lx)\n", (long) d);
    fflush(stderr);
#endif
#ifdef SYBASE
    d->need_dynamic = 0;
    if (!strncasecmp(d->statement,"select",6)
     || !strncasecmp(d->statement,"update",6)
     || !strncasecmp(d->statement,"delete",6)
     || !strncasecmp(d->statement,"declare",7))
#endif
    for (x = d->statement, row_len = 0; *x != '\0';)
    {
        switch(*x)
        {
#ifdef SYBASE
        case '@':
            if (*(x+1) == '@')
            {
                x += 2;     /* You cannot set global parameters  */
                break;
            }
#else
        case ':':
        case '?':
#endif
            for (x1 = x + 1;
                *x1 == '_' || *x1 == '$'|| *x1 == '#' || isalnum(*x1);
                         x1++);
/*
 * We should ignore indicator variables, apparently, but do not.
 */
            len = x1 - x;
            if (
#ifndef SQLITE3
                len > 1 &&
#endif
                do_bv(&anchor,x, len, &this_bv))
            {
                if (cur_dict == (struct dict_con *) NULL
                 ||((found_bv = find_bv(cur_dict,x,len)) == (struct bv *) NULL))
                {
#ifdef SYBASE
/*
 * Sybase does not support implicit type conversion from numbers to characters.
 * If we execute a statement, even for explain plan purposes, with
 * incorrect variable types, we get a useless error message, which does not
 * identify the variables in error. In this case, what we have to do is:
 * - construct a new statement with the variables replaced by ? placeholders
 * - prepare this statement with a special ct_dynamic() call
 * - use other special calls to describe the input and output variables.
 */

                    d->need_dynamic = 1;
                    this_bv->dbtype = CS_CHAR_TYPE;
#else
                    if (char_long < 250)
                        this_bv->dbtype = ORA_VARCHAR2;
                    else
                        this_bv->dbtype = ORA_LONG;
#endif
/*
 * Frig for Cedar spoolering system
 */
                    if (!strncmp(x,":P_RESULT",len))
                    {
                        this_bv->dbtype = ORA_LONG;
                        this_bv->dsize = 1999;
                        this_bv->dbsize = 1999;
                    }
                    else
                    {
                        this_bv->dsize = char_long;
                        this_bv->dbsize = char_long;
                    }
                    this_bv->scale = 0;
                    this_bv->prec = 0;
                    this_bv->nullok = 1;
                }
                else
                {
                    this_bv->dbtype = found_bv->dbtype;
                    this_bv->dsize = found_bv->dsize;
                    this_bv->dbsize = found_bv->dbsize;
                    this_bv->scale = found_bv->scale;
                    this_bv->prec = found_bv->prec;
                    this_bv->nullok = found_bv->nullok;
                }
                row_len += this_bv->dsize;
                cnt++;
            }
            x = x1;
            break;
        case '\'':
            for (;;)
            {
                x++;
                if (*x == '\0')
                    break; 
                if (*x == '\'')
                {
                    x++;
                    if (*x != '\'')
                        break;
                }
            }
            break;
        case '"':
            for (;;)
            {
                x++;
                if (*x == '\0')
                    break; 
                if (*x == '"')
                {
                    x++;
                    break;
                }
            }
            break;
        default:
            x++;
            break;
        }
    }
    sqlclu(d->bdp);
    if (cnt == 0)
    {
        d->bdp = (E2SQLDA *) NULL;
        return;
    }
    else
/*
 * Now allocate the items that can be allocated now
 */
        d->bdp = sqlald(cnt, bd_size, 1 + row_len/cnt);
/*
 *  Loop through the bind variables, storing them in association with
 *  the SQLDA and executing the bind function.
 */
    for (c = d->bdp->C,
         r = d->bdp->R,
         o = d->bdp->O,
         v = d->bdp->V,
         s = d->bdp->S,
         i = d->bdp->I,
         t = d->bdp->T,
         l = d->bdp->L,
         p = d->bdp->P,
         a = d->bdp->A,
#ifdef OR9
         b = d->bdp->bindhp,
#endif
         x = d->bdp->base,
         this_bv = anchor;
             this_bv != (struct bv *) NULL;
#ifdef OR9
                 b++,
#endif
                 c++, r++, o++, v++, s++, i++, t++, l++, p++, a++)
    {
/*
 * First, deal with the stored variable name. It is not null terminated,
 * since it is embedded in the SQL statement.
 */
    short int *fo = (short int *) *o;

        *s = this_bv->bname;
        *c = this_bv->blen;
#ifdef SQLITE3
        if (*c == 1)
            *fo = (short int) this_bv->seq;
#endif
        *t = this_bv->dbtype;
        *l = this_bv->dsize;
        *p = this_bv->prec;
        *a = this_bv->scale;
        *v = x;
#ifdef DEBUG
        bv_dump(this_bv);
#endif
#ifndef SQLITE3
        for (cnt = 0; cnt < d->bdp->arr; cnt++)
            *fo++ = (short int) *l;
#else
        if (*c == 1)
            for (cnt = 0; cnt < d->bdp->arr; cnt++)
                *fo++ = (short int) this_bv->seq;
#endif
        anchor = this_bv;
        this_bv = this_bv->next;
        free(anchor);
/*
 * Now execute the bind itself
 */
#ifndef SQLITE3
#ifdef SYBASE
        if (!(d->need_dynamic))
#endif
        if (!dbms_bind(d,
                *s,           /* The variable name                         */
                *c,           /* The length of the variable name           */  
                *v,           /* The address of the data area              */
                *l,           /* The data value length                     */
                *p,           /* The data precision                        */
                *a,           /* The data scale                            */
                *t,           /* The ORACLE Data Type                      */
                *i,           /* The indicator variables                   */
                *o,           /* The output lengths                        */
                *r            /* The return codes                          */
#ifdef OR9
                ,b
#endif
                  ))
            {
                fprintf(stderr,"%s\n",d->statement);
                scarper(__FILE__,__LINE__,"variable bind failed");
            }
#endif
        x = x + (*l)*bd_size;
    }
    if (x > (d->bdp->bound + 1))
    {
        fprintf(stderr, "Logic Error: Overrun in desc_bind: %s\n",
           (d->statement == NULL) ? "No statement" : d->statement);
        exit(1);
    }
    return;
}
#endif
/********************************************************************
 * Process the Bind after messing around with the sizes.
 */
void do_rebind(d)
struct dyn_con * d;
{
int cnt =0, cnt1 = 0;
char ** v;
char ** s;
#ifdef OR9
char **b;
#endif
#ifdef SYBASE
CS_SMALLINT **i;
CS_INT *l;
CS_INT *c, *t;
CS_INT *p, *a;
CS_INT **r, **o;
#else
short int **i;
int *l;
short int *c, *t;
short int *p, *a;
unsigned short int **r, **o;
#endif
/*
 *  Loop through the bind variables, storing them in association with
 *  the SQLDA and executing the bind function.
 */
#ifdef DEBUG
    fprintf(stderr, "do_rebind(%x)\n", (long) d);
    fflush(stderr);
#endif
    if (d->bdp == NULL)
        return;
    for (cnt = 0,
         c = d->bdp->C,
         r = d->bdp->R,
         o = d->bdp->O,
         v = d->bdp->V,
         s = d->bdp->S,
         i = d->bdp->I,
         t = d->bdp->T,
         p = d->bdp->P,
#ifdef OR9
         b = d->bdp->bindhp,
#endif
         a = d->bdp->A,
         l = d->bdp->L;
             cnt < d->bdp->F;
#ifdef OR9
                 b++,
#endif
                 cnt++, c++, r++, o++, v++, s++, i++, t++, l++, a++, p++)
    {
/*
 * First, deal with the stored variable name. It is not null terminated,
 * since it is embedded in the SQL statement.
 */
#ifdef SYBASE
        CS_INT *fo = (CS_INT *) *o;
        for (cnt1 = 0; cnt1 < d->bdp->arr; cnt1++)
            *fo++ = (CS_INT) *l;
#else
        short int *fo = (short int *) *o;
        for (cnt1 = 0; cnt1 < d->bdp->arr; cnt1++)
#ifdef SQLITE3
            *fo++ = (short int) (cnt + 1);
#else
            *fo++ = (short int) *l;
#endif
#endif
#ifdef SQLITE3
        *l = strlen(*v);
#endif
/*
 * Now execute the bind itself
 */
        if (!dbms_bind(d,
                *s,           /* The variable name                         */
                *c,           /* The length of the variable name           */  
                *v,           /* The address of the data area              */
                *l,           /* The data value length                     */
                *p,           /* The Data Precision                        */
                *a,           /* The Data Scale                            */
                *t,           /* The Data Type                             */
                *i,           /* The indicator variables                   */
                *o,           /* The output lengths                        */
                *r            /* The return codes                          */
#ifdef OR9
                ,b
#endif
            ))
            {
                fprintf(stderr,"%s\n",d->statement);
                scarper(__FILE__,__LINE__,"variable bind failed");
            }
    }
    return;
}
/***************************************************************************
 * Attach to the Database. Return a pointer to a structure holding
 * the login data area. Blank the input user/password. 
 */
struct sess_con * dyn_connect(con_str, appname)
char      *con_str;
char      *appname;
{
struct sess_con * x;
char * x1;
    if ((x = (struct sess_con *) calloc(sizeof(*x), 1))
             == (struct sess_con *) NULL)
        return x;
#ifdef INGRES
    x->curs_no = 1;                          /* 0 breaks the hash delete */
#endif
#ifdef SYBASE
    x->appname = strdup(appname);       
#endif
    x->uid_pwd = strdup(con_str);       
    for (x1 = con_str; *x1 != '\0'; *x1++ = ' ');
    x->ht = hash(128,long_hh,icomp);          /* Cursor Hash Table */
    x->csmacro.vt = hash(256,string_hh,strcmp);       /* Variable Hash Table */
#ifndef OSF
    cscalc_init(&(x->csmacro));               /* Initialise the expression
                                                 handler */
#endif
    if (!dbms_connect(x))
    {
        free(x->uid_pwd);
        cleanup(x->ht);
        cleanup(x->csmacro.vt);
        free((char *) x);
        return (struct sess_con *) NULL;
    }
    return x;
}
/*********************************************
 * Detach from the Database.
 */
int dyn_disconnect(x)
struct sess_con * x;
{
#ifdef DEBUG
    fprintf(stderr, "dyn_disconnect(%lx)\n", (long) x);
    fflush(stderr);
#endif
    iterate(x->ht, 0, dyn_kill); /* Zap all the cursors          */
    if (dbms_disconnect(x))          /* Exit the database            */
    {
        cleanup(x->ht);              /* Free the cursor hash table   */
        cscalc_zap(&(x->csmacro));      /* Free up the expression handler memory */
        cleanup(x->csmacro.vt);
        free(x->uid_pwd);            /* Free the connect string      */
#ifdef SYBASE
        free(x->appname);
#endif
        free((char *) x);
        return 1;
    }
    return 0;
}
/*************************************************
 * Dump out stuff from dynamic statement structure
 */
void dyn_dump(d)
struct dyn_con * d;
{
#ifdef INGRES
    fprintf(stderr, "%x|%x|%d|%s|%d|%d|%s|%x|%s\n",
        (long) d->con,
        (long) d,
        d->cnum,
        d->hash_key,
        d->repti1,
        d->repti2,
        d->reptid,
        d->reptHandle,
        (d->statement != NULL) ?d->statement : "(None)");
#else
    fprintf(stderr, "%x|%x|%d|%s\n",
        (long) d->con,
        (long) d,
        d->cnum,
        (d->statement != NULL) ?d->statement : "(None)");
#endif
    return;
}
/**********************************************************************
 * Add another ORACLE type to the designated block
 *
 * This changes the bound data type if the data type is not appropriate.
 */
void add_bind(dyn, tok_id, len, ptr)
struct dyn_con * dyn;
enum tok_id tok_id;
int len;
char * ptr;
{
short int i,r;
register E2SQLDA * rd;

    rd= dyn->bdp;
    if (dyn->fld_ind >= rd->N)
        dyn->fld_ind = 0;
    r = dyn->cur_ind;   /* Record Number */
    dyn->fields_sent++;
    i = dyn->fld_ind;
#ifdef DEBUG
    printf("add_bind(r: %d i: %d tok_id: %d len: %d rd->T[i]: %d\n",
                  r, i, tok_id, len, rd->T[i]);
    fflush(stdout);
#endif
    if ((tok_id == FNUMBER && rd->T[i] != ORA_FLOAT)
          || (tok_id == E2FLONG && rd->T[i] != ORA_INTEGER)
#ifndef SYBASE
          || (tok_id == FDATE && rd->T[i] != ORA_DATE)
#endif
          || (tok_id == FRAW && rd->T[i] != ORA_LONG_RAW)
          || (tok_id == FIELD && rd->L[i] < len))
    {
        if (tok_id == FNUMBER)
        {
            rd->T[i] = ORA_FLOAT;
            rd->L[i] = sizeof(double);
        }
        else
        if (tok_id == E2FLONG)
        {
            rd->T[i] = ORA_INTEGER;
            rd->L[i] = sizeof(long);
        }
#ifndef SYBASE
        else
        if (tok_id == FDATE)
        {
            rd->T[i] = ORA_DATE;
            rd->L[i] = 7;
        }
#endif
        else
/*
 * String or whatever is too long, and/or it is raw data.
 */
        {
            if (tok_id == FRAW)
                rd->T[i] = ORA_LONG_RAW;
            if ( rd->L[i] < len)
            {
            char * x =  (char *) malloc((len+1) * rd->arr);
            char * y1, * y2;
            int j;

                for (j = 0, y1 = rd->V[i], y2 = x ; j < r ; j++)
                {
                    memcpy(y2, y1, rd->L[i]);
                    y1 += rd->L[i];
                    y2 += len + 1;
                }
                rd->L[i] = len + 1;
                rd->V[i] = x;
                if (len > 249 && rd->T[i] != ORA_LONG_RAW)
                    rd->T[i] = ORA_LONG;
                dyn_note(dyn, x);
            }
        }
#ifdef DEBUG
    printf("Changed: add_bind(r: %d i: %d tok_id: %d len: %d rd->T[i]: %d\n",
                  r, i, tok_id, len, rd->T[i]);
    fflush(stdout);
#endif
        if (dyn->reb_flag == -1)
        {
#ifdef SQLITE3
            if (rd->C[i] == 1)
            {
            short int *fo = (short int *) rd->O[i];
            int cnt;

                for (cnt = 0; cnt < rd->arr; cnt++)
                    *fo++ = (short int) (i+1);
            }
#else
            if (!dbms_bind(dyn,
                rd->S[i],     /* The variable name                         */
                rd->C[i],     /* The length of the variable name           */  
                rd->V[i],     /* The address of the data area              */
                rd->L[i],     /* The data value length                     */
                rd->P[i],     /* The data value precision                  */
                rd->A[i],     /* The data value scale                      */
                rd->T[i],     /* The ORACLE Data Type                      */
                rd->I[i],     /* The indicator variables                   */
                rd->O[i],     /* The output lengths                        */
                rd->R[i]      /* The return codes                          */
#ifdef OR9
                ,rd->bindhp[i]
#endif
                ))
            {
                fprintf(stderr,"%s\n",dyn->statement);
                scarper(__FILE__,__LINE__,"variable rebind failed");
            }
#endif
        }
        else
            dyn->reb_flag = 1;
    }
    if (len == 0)
    {       /* NULL Value */
        *(rd->I[i] + r) = -1;  /* Set indicator to NULL */
        *(rd->V[i] + r*rd->L[i]) = '\0';
    }
    else
    {
        *(rd->I[i] + r) = 0;
#ifndef SYBASE
        if (tok_id == FDATE)
        {
/*
 * the ptr points to a double 'seconds since 1970'
 */
            dyn->chars_sent += 7;
            *(rd->O[i] + r) = 7;
            memcpy(rd->V[i] + r*rd->L[i],retordate(*((double *)ptr)), 7);
        }
        else
#endif
        {
            memcpy(rd->V[i] + r*rd->L[i], ptr, len);
            dyn->chars_sent += len;
            *(rd->O[i] + r) = len;
            *(rd->V[i] + r*rd->L[i] + len) = '\0';
        }
    }
    dyn->fld_ind++;
#ifdef DEBUG
    for (i = 0; i < rd->F; i++)
    (void) fprintf(stderr,"Bind Pointers %x %x %x %x %x %x\n",
          (int) dyn,
          (int) rd->S[i],
          (int) rd->V[i],
          (int) rd->I[i],
          (int) rd->O[i],
          (int) rd->R[i]);
    fflush(stderr);
#endif
    return;
}
/**********************************************************************
 * Add another ORACLE type to the designated block
 */
void add_native(dyn,tok_id)
struct dyn_con * dyn;
enum tok_id tok_id;
{
double d;
int i;

    if (tok_id == FNUMBER || tok_id == FDATE)
    {
        d = strtod(tbuf,(char **) NULL);
        add_bind(dyn, tok_id, sizeof(double), (char *) &d);
    }
    else
    if (tok_id == E2FLONG)
    {
        i = atoi(tbuf);
        add_bind(dyn,tok_id,sizeof(long), &i);
    }
    else
    {
        if (tok_id != FRAW)
            tlen = strlen(tbuf);
        add_bind(dyn, tok_id, tlen, tbuf);
    }
    return;
}
/***********************************************************************
 * Describe the select list variables into sdp, expanding if
 * necessary. Set desc. size to number of columns found.
 *
 * Set up the type information (since we will want this regardless
 * of whether or not we plan to process the data from the select).
 *
 * Adjust the returned data types, so that everything comes back as
 * a string, except for the date, which we adjust at time of select, by
 * expanding the ORACLE representation, and raw stuff, which comes
 * back as is, and which we convert to hexadecimal for output.
 */
void desc_sel(d)
struct dyn_con * d;
{
char cbuf[256];
struct bv bv, * anchor, *last_bv;
int  cnt, row_len, flen;
char *x, *j;
char ** v;
char ** s;
#ifdef OR9
char **b;
#endif
#ifdef SYBASE
CS_SMALLINT **i;
CS_INT **r, **o;
CS_INT *l;
CS_INT *c, *t, *ot, *p, *a, *u;
CS_INT num_cols;
#else
int num_cols;
short int **i;
unsigned short int **r, **o;
int *l;
short int *c, *t, *ot, *p, *a, *u;
#endif

#ifdef DEBUG
    fprintf(stderr, "desc_sel(%x)\n", (long) d);
    fflush(stderr);
#endif
    anchor = (struct bv *) NULL;
    memset((char *) &bv, 0, sizeof(bv));
    bv.bname = &cbuf[0];
    bv.blen = sizeof(cbuf);
/*
 * Loop through the select list items, finding how many of them there will
 * be, and how much space we need to allocate.
 */
#ifdef SYBASE
    for (cnt = 0,
         row_len = 0,
         ct_res_info(d->cmd,CS_NUMDATA,&num_cols, CS_UNUSED, NULL);
             cnt < num_cols && dbms_desc(d,cnt,&bv);
                 cnt++)
#else
#ifdef OR9
    if (OCIAttrGet ((dvoid *) d->stmthp, (ub4)OCI_HTYPE_STMT, (dvoid *) 
                  &num_cols, (ub4 *) 0, (ub4)OCI_ATTR_PARAM_COUNT,
                   d->con->errhp))
    {
        dbms_error(d->con);
        return;
    }
    for (cnt = 1,
         row_len = 0;
          cnt <= num_cols && dbms_desc(d,cnt,&bv); cnt++)
#else
    for (cnt = 1, row_len = 0; !dbms_desc(d,cnt,&bv); cnt++)
#endif
#endif
    {
        switch((int) bv.dbtype)
        {
#ifdef SYBASE
        case CS_DECIMAL_TYPE:
        case CS_IMAGE_TYPE:
            flen = 2 * bv.dsize;
            bv.dsize = flen;
            break;
        case CS_TINYINT_TYPE:
        case CS_SMALLINT_TYPE:
        case CS_USHORT_TYPE:
        case CS_INT_TYPE:
        case CS_MONEY_TYPE:
        case CS_MONEY4_TYPE:
        case CS_REAL_TYPE:
        case CS_FLOAT_TYPE:
        case CS_NUMERIC_TYPE:
            flen = 8;
            bv.dsize = flen;
            break;
        case CS_BIT_TYPE:
            flen = 8 * bv.dsize + 1;
            bv.dsize = flen;
            break;
        case CS_DATETIME_TYPE:
        case CS_DATETIME4_TYPE:
            flen = 30;
            bv.dsize = flen;
            break;
#else
#ifndef SQLITE3
#ifndef INGRES
        case ORA_DATE:
            flen = 21;
            bv.dsize = flen;
            break;
        case ORA_RAW_MLSLABEL:
        case ORA_LONG_VARRAW:
#ifndef DIY
/*
 * Since I wrote this, ORACLE have greatly expanded the range of possible codes.
 * The ones that cause difficulties are dropped in here, pending a re-write for
 * ORACLE 9 or 10.
 */
        case SQLT_CLOB:  /* character lob */
        case SQLT_BLOB:  /* binary lob */
#ifdef LOCATORS_WORK
            flen = 8;    /* Largest imaginable pointer */
            bv.dsize = flen;
            break;
#endif
        case SQLT_CUR: /* cursor  type */
        case SQLT_RDD: /* rowid descriptor */
        case SQLT_NTY: /* named object type */
        case 109: /* cartridge defined type? */
        case SQLT_REF: /* ref type */
        case SQLT_BFILEE: /* binary file lob */
        case SQLT_CFILEE: /* character file lob */
        case SQLT_RSET:   /* result set type */
        case SQLT_NCO:   /* named collection type (varray or nested table) */
#endif
#endif
        case ORA_RAW:
        case ORA_VARRAW:
        case ORA_LONG_RAW:
            flen = 2 * bv.dsize + 1;
            bv.dsize = flen;
            break;
#endif
#endif
        default:
            if (!(flen = ((bv.dsize > bv.dbsize) ? bv.dsize : bv.dbsize)))
            {
                flen = char_long;
                bv.dsize = flen;
            }
            flen++;
            bv.dsize = flen;
            break;
        }
        row_len += flen;
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
#ifdef SYBASE
    if (cnt != num_cols)
    {
        fprintf(stderr,"%s\n",d->statement);
        fprintf(stderr,"cnt:%d num_cols:%d\n",cnt,num_cols);
        scarper(__FILE__,__LINE__,"Describe() failed");
    }
#else
#ifdef OR9
    if (cnt <= num_cols)
    {
        fprintf(stderr,"%s\n",d->statement);
        fprintf(stderr,"cnt:%d num_cols:%d\n",cnt,num_cols);
        scarper(__FILE__,__LINE__,"Describe() failed");
    }
#endif
    cnt--;    /* Now an accurate count of the number of select list items */
#endif
    if (!cnt)
    {
        if (d->ret_status
#ifdef SYBASE
                             != CS_SUCCEED
#else
#ifdef OR9
                != 0 && d->ret_status != 1403
#endif
#endif
                        )
            {
                fprintf(stderr,"%s\n",d->statement);
                scarper(__FILE__,__LINE__,"Describe() failed");
            }
        return;
    }
/*
 * Allocate sufficient space for the select data structures
 */
    sqlclu(d->sdp);
    d->sdp = sqlald(cnt, sd_size,1  + row_len/cnt);                    
#ifdef DEBUG
    fprintf(stderr, "scram_cnt:%d d->sdt(%x)\n", (long) d->sdt, scram_cnt);
    fflush(stderr);
#endif
    if ( !(d->sdt) )
    {                          /* Haven't allocated (*sdt)[] yet. */
#ifdef SYBASE
        d->sdt = (CS_INT *) calloc(d->sdp->N, sizeof(CS_INT));
#else
        d->sdt = (short *) calloc(d->sdp->N, sizeof(short));
#endif
        if (scram_cnt)
            d->scram_flags = (char *) calloc(d->sdp->N, sizeof(char));
    }
    else
    if (d->sdtl < d->sdp->N )
    {                          /* Need to reallocate saved type array. */
#ifdef SYBASE
        d->sdt = (CS_INT *) realloc(d->sdt, sizeof(CS_INT) * d->sdp->N);
#else
        d->sdt = (short *) realloc(d->sdt, sizeof(short) * d->sdp->N);
#endif
        if (scram_cnt && (d->scram_flags != (char *) NULL))
            d->scram_flags = (char *) realloc(d->scram_flags,
                                              sizeof(char)*d->sdp->N);
    }
    d->sdtl = cnt;
/*
 *  Loop through the select variables, storing them in association with
 *  the SQLDA and executing the define function.
 */
    for (cnt = 1,
         j = d->scram_flags,
         ot = d->sdt,
         x = d->sdp->base,
         c = d->sdp->C,
         r = d->sdp->R,
         o = d->sdp->O,
         v = d->sdp->V,
         s = d->sdp->S,
         i = d->sdp->I,
         t = d->sdp->T,
         l = d->sdp->L,
         p = d->sdp->P,
         a = d->sdp->A,
         u = d->sdp->U,
#ifdef OR9
         b = (OCIBind **) d->sdp->bindhp,
#endif
         last_bv = anchor;

             last_bv != (struct bv *) NULL;

              x = x + (*l)*(d->sdp->arr),
#ifdef OR9
              b++,
#endif
              cnt++, ot++, c++, r++, o++, v++, s++, i++, t++, l++, j++)
    {
#ifdef DEBUG
        fprintf(stderr, "d->sdp->bound:%x x:%x last_bv:%x\n", (long) d->sdp->bound,(long) x, (long) last_bv);
        fprintf(stderr, 
"j:%x ot:%x c:%x  r:%x  o:%x  v:%x  s:%x  i:%x  t:%x  l:%x  p:%x  a:%x  u:%x\n", 
         (long) j, (long) ot, (long) c, (long) r, (long) o, (long) v, (long) s,
          (long) i, (long) t, (long) l, (long) p, (long) a, (long) u );
        fflush(stderr);
#endif
        if (x > (d->sdp->bound + 1))
        {
            fprintf(stderr,"%s\n",d->statement);
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

        switch (last_bv->dbtype)
        {
#ifdef SYBASE
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
            *t = CS_FLOAT_TYPE;
            break;
#else
#ifndef SQLITE3
#ifndef INGRES
        case ORA_DATE:
            *t = ORA_DATE;
            break;
        case ORA_RAW_MLSLABEL:
#endif
        case ORA_RAW:
        case ORA_VARRAW:
            *t = ORA_RAW;
            break;
        case ORA_LONG_RAW:
#ifndef INGRES
        case ORA_LONG_VARRAW:
#endif
            *t = ORA_LONG_RAW;
            break;
        case ORA_LONG:
#endif
        case ORA_NUMBER:
        case ORA_INTEGER:
#ifndef INGRES
#ifndef DIY
#ifndef SQLITE3
        case SQLT_CUR: /* cursor  type */
        case SQLT_RDD: /* rowid descriptor */
        case SQLT_NTY: /* named object type */
        case SQLT_CLOB:  /* character lob */
        case SQLT_BLOB:  /* binary lob */
        case SQLT_BFILEE: /* binary file lob */
        case SQLT_CFILEE: /* character file lob */
        case 109: /* cartridge defined type? */
        case SQLT_REF: /* ref type */
        case SQLT_RSET:   /* result set type */
        case SQLT_NCO:   /* named collection type (varray or nested table) */
#endif
#endif
#endif
            *t = last_bv->dbtype;
            break;
        default:  /* Anything vaguely character */
#endif
        {
        int k;
        char buf[ESTARTSIZE];
/*
 * Problems with extra zero bytes
 */
#ifdef SYBASE
            (*l)++;
            *t = ORA_CHAR;
#else
            *t = ORA_VARCHAR2;
#endif
/*
 * Check for scramble
 */
            if (scram_cnt)
            {
#ifdef DEBUG
                fflush(stdout);
                fputs("Scramble enabled?\n", stderr);
                fflush(stderr);
#endif
                (void) sprintf(buf,"%.*s,\n",*c,*s);
                for (k = 0; k < scram_cnt; k++)
                    if (re_exec(buf,scram_cand[k]) > 0)
                    {
#ifdef DEBUG
                        fprintf(stderr, "Scramble matched %d %-.*s!\n", 
                                   k, *c, *s);
                        fflush(stderr);
#endif
                        *j = 1;
                        break;
                    }
            }
        }
        }
/*
 * Now execute the define itself. Define, I suspect, is an ORACLE-ism; it refers
 * to the allocation of storage to receive select results.
 */
#ifdef DEBUG
        bv_dump(last_bv);
#endif
        if (!dbms_define(d,
                cnt,          /* Position indicator                        */
                *v,           /* The address of the data area              */
                *l,           /* The data value length                     */
                *t,           /* The ORACLE Data Type                      */
                *i,           /* The indicator variables                   */
                *o,           /* The output lengths                        */
                *r            /* The return codes                          */
#ifdef OR9
                ,b
                ,last_bv->tname
                ,last_bv->tlen
                ,last_bv->sname
                ,last_bv->slen
#endif
            ))
        {
            fprintf(stderr,"%s\n",d->statement);
            scarper(__FILE__,__LINE__,"select define failed");
        }
#ifdef DEBUG
        fprintf(stderr, "%.*s:%u:%u\n",*c,*s,*ot,*t);
#endif
        anchor = last_bv;
        last_bv = last_bv->next;
        free(anchor);
    }
    if (x > (d->sdp->bound + 1))
    {
        fprintf(stderr,"%s\n",d->statement);
        scarper(__FILE__,__LINE__,"Select descriptor over-flow!");
        exit(1);
    }
#ifdef DEBUG
    type_print(stderr,d->sdp);
#endif
    d->so_far = 0;
    d->cur_ind = 0;
    d->to_do = 0;
    return;
}
#ifdef SQLITE3
static int should_hit(d)
struct dyn_con * d;
{
char * x = d->statement;

    if (x != NULL)
    {
        while(isspace((int) *x))
            x++;
        if (!strncasecmp(x, "insert",6)
         || !strncasecmp(x, "update",6)
         || !strncasecmp(x, "delete",6))
            return 1;
    }
    return 0;
}
#endif
/*******************************************************************
 * Execute a dynamic statement
 */
enum tok_id dyn_exec(fp,d)
FILE * fp;
struct dyn_con * d;
{
enum tok_id tok_id;
#ifdef SQLITE3
int hit_flag = should_hit(d);
#endif

#ifdef DEBUG
    fprintf(stderr, "dyn_exec(%x, %x)\n", (long) fp, (long) d);
    fflush(stderr);
#endif
/*
 * If there are bind variables, process them
 */
#ifndef INGRES
    if (d->bdp != (E2SQLDA *) NULL && d->bdp->F != 0)
    {
        tok_id = get_tok(fp);
/*
 * Provide a null record if no values have been provided
 */
        if (tok_id != FIELD && tok_id != FNUMBER && tok_id != E2FLONG
          && tok_id != FDATE && tok_id != FRAW)
        {
        char * tmp = strdup(tbuf);
#ifdef DEBUG
            (void) fprintf(stderr,"No data found for this statement: %s\n",
                                  d->statement);
#endif
            d->cur_ind = 0;
            *tbuf='\0';
            for ( d->fld_ind = 0; d->fld_ind <  d->bdp->F; d->fld_ind++)
                 add_field(d);
            d->cur_ind = 1;
            strcpy(tbuf,tmp);
            free(tmp);
        }
        else
        {
/*
 * If there are both bdp and sdp, adjust the bdp if they all match
 */
#ifdef SYBASE
            d->need_dynamic = 0;
#endif
            if (d->sdp != (E2SQLDA *) NULL
              &&  d->bdp->F == d->sdp->F
              &&  d->sb_map == (short int *) NULL
              && cur_dict == (struct dict_con *) NULL)
                ini_bind_vars(d);
#ifdef DEBUG
            else
            {
                 (void) fprintf(stderr,
                   "d->sdp:%x d->bdp->F:%d d->sdp->F:%d d->sb_map:%x\n",
                    (unsigned long) d->sdp,
                    ((unsigned long) d->bdp == 0)? 0:
                    (unsigned long) d->bdp->F,
                    ((unsigned long) d->sdp == 0) ?0:
                    (unsigned long) d->sdp->F,
                    (unsigned long) d->sb_map);
                 fflush(stderr);
            }
#endif
            d->cur_ind = 0;
            d->fld_ind = 0;
            while (tok_id != PEOF && tok_id != SQL && tok_id != COMMAND)
            {
                switch(tok_id)
                {
                case FNUMBER:
                case E2FLONG:
                case FDATE:
                case FRAW:
                case FIELD:
                    if (d->fld_ind >=  d->bdp->F)
                    {
                        fprintf(stderr,"%s:\nOffset: %d\n",d->statement,
                         ftell(fp));
                        scarper(__FILE__, __LINE__, 
                                 "Syntax error in input, Extra FIELD");
                    }
                    if ( d->sb_map != (short int *) NULL)
                        add_field(d);
                    else
                        add_native(d,tok_id);
                    break;
                case EOR:
                    d->cur_ind++;
                    if (d->cur_ind == d->bdp->arr)
                    {
                        exec_dml(d);
#ifdef SQLITE3
                        if (hit_flag && d->rows < 1)
                        {
                            fprintf(stderr,"%s:\nOffset: %d\n",d->statement,
                                     ftell(fp));
                            scarper(__FILE__, __LINE__, 
                                 "No rows affected; record changed by other user?");
                        }
#endif
                    }
                    d->fld_ind = 0;
                    break;
                default:
                    {
                        fprintf(stderr,"%s|==> %s\nOffset: %d token: %d\n",
                                d->statement, tbuf, ftell(fp), tok_id);
                        scarper(__FILE__, __LINE__,  "Syntax error in file");
                    }
                }
                tok_id = get_tok(fp);
            }
        }
    }
    else
#endif
    {
       d->cur_ind = 1;
       tok_id = get_tok(fp);
    }
    if (d->cur_ind)
    {
        exec_dml(d);
#ifdef SQLITE3
        if (hit_flag && d->rows < 1)
        {
            fprintf(stderr,"%s:\nOffset: %d\n",d->statement,
                                     ftell(fp));
            scarper(__FILE__, __LINE__, 
                                 "No rows affected; record changed by other user?");
        }
#endif
    }
    if (d->is_sel && d->sdp == (E2SQLDA *) NULL)
        desc_sel(d);
    return tok_id;
}
/************************************************************************
 * Handle array fetches with the dynamic variables
 */
void dyn_fetch(fp1,fp2,dyn, output_function)
FILE * fp1;
FILE * fp2;
struct dyn_con *dyn;
void (*output_function)();
{
#ifdef DEBUG
    fprintf(stderr, "dyn_fetch(%x,%x,%x,%x)\n",
                    (long) fp1,(long) fp2, (long) dyn,
                    (long) output_function);
    fflush(stderr);
#endif
    if (!dyn->is_sel)
        return;                    /* Do not fetch a non-select */
    if (dyn->sdp == (E2SQLDA *) NULL)
        desc_sel(dyn);
#ifdef OR9
    dyn->ret_status = 0;
    dyn->to_do = 0;
    dyn->so_far = 0;
#endif
    for (;;)
    {
        if (dyn->to_do == 0)
        {
#ifdef SQLITE3
            if (dyn->ret_status != SQLITE_ROW)
#else
#ifdef SYBASE
            if (dyn->ret_status != CS_SUCCEED && dyn->ret_status != CS_ROW_FAIL)
#else
            if (dyn->ret_status)
#endif
#endif
                return ;
/*
 * If any of the select are CHAR fields, these need to be preset to spaces
 */
            fflush(fp1);
            dyn->to_do = dyn->so_far;
            if (!dbms_fetch(dyn))
            {
                fprintf(stderr,"%s\n", dyn->statement);
                scarper(__FILE__,__LINE__,"Fetch failed");
                dyn->to_do = 0;
#ifdef SYBASE
                dyn->ret_status = CS_FAIL;
#else
                dyn->ret_status = 1;
#endif
                continue;
            }
            else
                dyn->cur_ind = 0;
            dyn->to_do = dyn->so_far - dyn->to_do;
        }
        if (dyn->to_do > 0)
        {
            if (output_function != NULL)
                (*output_function)(fp1,fp2,dyn);
            dyn->to_do = 0;
        }
    }
}
/************************************************************************
 * Handle result sets. Sybase injects an extra layer, hence this function
 * is empty for ORACLE.
 */
void res_process(fp1,fp2,dyn, output_function, all_flag)
FILE * fp1;
FILE * fp2;
struct dyn_con *dyn;
void (*output_function)();
int all_flag;
{
#ifdef DEBUG
    fprintf(stderr, "res_process(%x,%x,%x,%x,%d)\n",
                    (long) fp1,(long) fp2, (long) dyn,
                    (long) output_function, all_flag);
    fflush(stderr);
#endif
#ifdef SYBASE
/*
 * Process the results of the command.
 */
    if (dyn->to_do > 0)
    {
        if (output_function != NULL)
            (*output_function)(fp1,fp2,dyn);
        dyn->to_do = 0;
    }
    (void) dbms_res_process(fp1,fp2,dyn, output_function, all_flag);
#else
/*    if (all_flag)   */
    if (all_flag && dyn->is_sel)
        dyn_fetch(fp1, fp2, dyn, output_function);
    else
    if (dyn->is_sel)
    {
    int base = dyn->so_far;

#ifdef OR9
        dyn->ret_status = 0;
#endif
        if (!dbms_fetch(dyn))
        {
            fprintf(stderr,"%s\n",dyn->statement);
            scarper(__FILE__,__LINE__,"Fetch failed");
            dyn->to_do = 0;
            dyn->ret_status = 1;
        }
        else
            dyn->cur_ind = 0;
        dyn->to_do = dyn->so_far - base;
        if (output_function != NULL)
            (*output_function)(fp1, fp2, dyn);
#ifdef INGRES
        dbms_stat_close(dyn);
#endif
        return;
    }
#ifdef INGRES
    dbms_stat_close(dyn);
#endif
#endif
    return;
}
/*
 * Fish out a single column from a block, updating the length, and the
 * column found.
 */
__inline int col_render(dyn, dp, irow, icol, len_ptr, f_ptr)
struct dyn_con *dyn;
E2SQLDA *dp;
int irow;
int icol;
int *len_ptr;
char **f_ptr;
{
short int *ip = (dp->I[icol] + irow);

    if (*ip < 0)                       /* Value is null           */
    {
        *(f_ptr)  = "";
        *(len_ptr)  = 0;
    }
    else
    {
#ifndef SYBASE
        if ((dp->T[icol] != ORA_NUMBER)
         && (dp->T[icol] != ORA_DATE))
#endif
        {
            *f_ptr  = dp->V[icol] + irow * dp->L[icol];
            *len_ptr = (int) (*(dp->O[icol] + irow));
        }
#ifndef SYBASE
        else
        if (dp->T[icol] == ORA_DATE)
        {
            *f_ptr = (char *) malloc(21);
            ordate( *f_ptr, dp->V[icol] + irow * dp->L[icol]);
            *len_ptr = 21;
            dyn_note(dyn,*f_ptr);
        }
        else
        if (dp->T[icol] == ORA_NUMBER)
        {
            *f_ptr = (char *) malloc(128);
            *len_ptr = ora_num( dp->V[icol] + irow * dp->L[icol], *f_ptr,
                 *(dp->O[icol] + irow));
            if (*len_ptr > 128)
            {
                scarper(__FILE__,__LINE__, "Failed to convert ORACLE number");
                exit(1);
            }
            dyn_note(dyn,*f_ptr);
        }
#endif
    }
    return 1;
}
/*
 * Allows a value in an array to be patched. Note that at this point, there
 * is no housekeeping to control updates. This will come. The input value is
 * always a string. This is converted if the type necessitates.
 */
#ifndef VCC2003
__inline
#endif
int col_patch(dp, irow, icol, len, f_ptr)
E2SQLDA *dp;
int irow;
int icol;
int len;
char *f_ptr;
{
    if (len == 0)
    {       /* NULL Value */
        *(dp->I[icol] + irow) = -1;  /* Set indicator to NULL */
        *(dp->V[icol] + irow*dp->L[icol]) = '\0';
        *(dp->O[icol] + irow) = 0;
    }
    else
    {
        *(dp->I[icol] + irow) = 0;  /* Set indicator to NOT NULL */
        switch ((int) dp->T[icol])
        {
#ifndef SYBASE
#ifndef INGRES
#ifndef SQLITE3
        double d;
        char *x;
        case ORA_DATE:
            *(dp->O[icol] + irow) = 7;
            (void) date_val(f_ptr, "DD MON YYYY HH24:MI:SS", &x, &d);
            memcpy(dp->V[icol] + irow*dp->L[icol], retordate(d), 7);
            break;
        case ORA_VARNUM:
        case ORA_NUMBER:
            *(dp->O[icol] + irow) = (short int) retora_num(
                                 dp->V[icol] + irow*dp->L[icol], f_ptr,
                                   dp->L[icol]);
            break;
        case ORA_RAW_MLSLABEL:
        case ORA_LONG_VARRAW:
#endif
#endif
#endif
#ifndef SQLITE3
        case ORA_RAW:
        case ORA_VARRAW:
        case ORA_LONG_RAW:
            (void) hexout( dp->V[icol] + irow*dp->L[icol], f_ptr, dp->L[icol]);
            *(dp->O[icol] + irow) = (short int) (len/2 + (len & 1));
            break;
#endif
        case ORA_INTEGER:
#ifndef INGRES
#ifndef SQLITE3
        case ORA_UNSIGNED:
#endif
#endif
            (void) iout( dp->V[icol] + irow*dp->L[icol], f_ptr, dp->L[icol]);
            *(dp->O[icol] + irow) = dp->L[icol];
            break;
#ifndef SQLITE3
        case ORA_PACKED:
            (void) pout( dp->V[icol] + irow*dp->L[icol], f_ptr, dp->L[icol]);
            *(dp->O[icol] + irow) = len/2 + (len & 1);
            break;
#endif
        case ORA_FLOAT:
            if (dp->L[icol] == sizeof(float))
            {
                (void) dout( dp->V[icol] + irow*dp->L[icol], f_ptr, 
                              sizeof(float));
                *(dp->O[icol] + irow) = (short int) sizeof(float);
            }
            else
            {
                (void) dout( dp->V[icol] + irow*dp->L[icol], f_ptr,
                              sizeof(double));
                *(dp->O[icol] + irow) = (short int) sizeof(double);
            }
            break;
/*
 * Treat everything else as a character string.
 */
        default:
            if (len > dp->L[icol])
                len = dp->L[icol];
            memcpy(dp->V[icol] + irow*dp->L[icol], f_ptr, len);
            *(dp->O[icol] + irow) = len;
            break;
        }
    }
    return 1;
}
/*
 * Allows a value in an array to be patched. This version does not check or
 * convert data types.
 */
__inline int bin_patch(dp, irow, icol, len, f_ptr)
E2SQLDA *dp;
int irow;
int icol;
int len;
char *f_ptr;
{
    if (len == 0)
    {       /* NULL Value */
        *(dp->I[icol] + irow) = -1;  /* Set indicator to NULL */
        *(dp->V[icol] + irow*dp->L[icol]) = '\0';
        *(dp->O[icol] + irow) = 0;
    }
    else
    {
        *(dp->I[icol] + irow) = 0;  /* Set indicator to NOT NULL */
        if (len > dp->L[icol])
            len = dp->L[icol];
        memcpy(dp->V[icol] + irow*dp->L[icol], f_ptr, len);
        *(dp->O[icol] + irow) = len;
    }
    return 1;
}
/**********************************************************************
 * Add another array element to the designated display block.
 */
int disp_add(d, len, ptr)
struct disp_con * d;
int len;
char * ptr;
{
int ret;

    ret = bin_patch(d->bdp, d->cur_ind, d->fld_ind, len, ptr);
#ifdef DEBUG
    fprintf(stderr, "disp_add() %d:%d=%*.*s\n", d->fld_ind, d->to_do,
                len, len, ptr);
#endif
    d->fld_ind++;
    if (d->fld_ind >= d->bdp->N)
    {
        d->fld_ind = 0;
        d->cur_ind++;
        d->to_do++;
        if (d->cur_ind >= d->bdp->arr)
            return 0;
    }
    return ret;
}
/************************************************************************
 * Return an array of field pointers and lengths for a row
 *
 * The fields are in the buffer; no movement or type conversion takes
 * place.
 *
 * Return 0 if somthing goes wrong, otherwise return 1.
 */
int dyn_loc_raw(dyn, len_ptr,f_ptr)
struct dyn_con *dyn;
int *len_ptr;
char **f_ptr;
{
int i;
E2SQLDA *dp;

#ifdef DEBUG
    fprintf(stderr, "dyn_locate(%x,%x,%x)\n",
                    (long) dyn,(long) len_ptr, (long) f_ptr);
    fflush(stderr);
#endif
    if (dyn->sdp == (E2SQLDA *) NULL)
        desc_sel(dyn);
    if ((dp = dyn->sdp) == (E2SQLDA *) NULL)
        return 0;
    if (dyn->to_do == 0 || dyn->cur_ind >= dyn->to_do)
    {
        if (dyn->ret_status
#ifdef SYBASE
                != CS_SUCCEED
#else
#ifdef SQLITE3
                != SQLITE_ROW
#else
#ifdef OR9
                != 0 && dyn->ret_status != 1403 && dyn->ret_status != 1002
#endif
#endif
#endif
                )
            return 0;
        dyn->to_do = dyn->so_far;
        if (!dbms_fetch(dyn))
        {
            fprintf(stderr,"%s\n",dyn->statement);
            scarper(__FILE__,__LINE__,"Fetch failed");
            dyn->to_do = 0;
            dyn->ret_status = 1;
            return 0;
        }
        else
            dyn->cur_ind = 0;
        dyn->to_do = dyn->so_far - dyn->to_do;
    }
    if (dyn->to_do <= 0)
        return 0;
    for (i=0; i < dp->N; i++)
    {                        /* For each field */
        if (*(dp->I[i] + dyn->cur_ind) < 0)
        {                  /* Value is null           */
            *(f_ptr)  = "";
            *(len_ptr)  = 0;
        }
        else
        {
            *f_ptr  = dp->V[i] + (dyn->cur_ind) * dp->L[i];
            *len_ptr = (int) (*(dp->O[i] + dyn->cur_ind));
        }
        f_ptr++;
        len_ptr++;
    }
    dyn->cur_ind++;
    return i;
}
/************************************************************************
 * Return an array of field pointers and lengths for a row
 *
 * Return 0 if somthing goes wrong, otherwise return 1.
 */
int dyn_locate(dyn, len_ptr,f_ptr)
struct dyn_con *dyn;
int *len_ptr;
char **f_ptr;
{
int i;
E2SQLDA *dp;

#ifdef DEBUG
    fprintf(stderr, "dyn_locate(%x,%x,%x)\n",
                    (long) dyn,(long) len_ptr, (long) f_ptr);
    fflush(stderr);
#endif
    if (dyn->sdp == (E2SQLDA *) NULL)
        desc_sel(dyn);
    if ((dp = dyn->sdp) == (E2SQLDA *) NULL)
        return 0;
    if (dyn->to_do == 0 || dyn->cur_ind >= dyn->to_do)
    {
        if (dyn->ret_status
#ifdef SQLITE3
                != SQLITE_ROW
#else
#ifdef SYBASE
                != CS_SUCCEED
#else
#ifdef OR9
                != 0 && dyn->ret_status != 1403 && dyn->ret_status != 1002
#endif
#endif
#endif
                )
            return 0;
        dyn->to_do = dyn->so_far;
        if (!dbms_fetch(dyn))
        {
            fprintf(stderr,"%s\n",dyn->statement);
            scarper(__FILE__,__LINE__,"Fetch failed");
            dyn->to_do = 0;
            dyn->ret_status = 1;
            return 0;
        }
        else
            dyn->cur_ind = 0;
        dyn->to_do = dyn->so_far - dyn->to_do;
    }
    if (dyn->to_do <= 0)
        return 0;
    for (i=0; i < dp->N; i++)
    {                        /* For each field */
        if (!col_render(dyn, dp, dyn->cur_ind, i, len_ptr, f_ptr))
            return 0;
        f_ptr++;
        len_ptr++;
    }
    dyn->cur_ind++;
    return i;
}
/**********************************************************************
 *   ini_bind_vars : Re-allocate Variable space to Bind Descriptor.
 **********************************************************************
 * This code is called for the tabdiff case, where the bind variables
 * are given their types from the target table. It is possible that
 * insufficient variable space has been allocated, in which case a
 * reallocation will take place.
 */
void ini_bind_vars(d)
struct dyn_con * d;
{
short int i;
short int j;
short int * x1, * x2;
struct sb_pair {
    short int bind;
    short int sel;
} * sb_work, *sb_cur, *sb_comp, sb_save;
E2SQLDA * resize;
int needed;
char *x;

#ifdef DEBUG
    fprintf(stderr, "ini_bind_vars(%x)\n", (long) d);
    fflush(stderr);
#endif
/*
 *   Allocate space for the select/bind mapping. This will be
 *   sizeof( int) * number of select items +
 *   sizeof( int) * number of bind items, since the format will
 *   be (count of ptrs for a select followed by those pointers),
 *     (count of ptrs for a select followed by those pointers), ...
 */
    if (d->sb_map != (short int *) NULL)
        free(d->sb_map);
    if ((d->sb_map = (short int *) calloc((d->bdp->N + d->sdp->N),
             sizeof(short int))) == (short int *) NULL)
        scarper(__FILE__,__LINE__,"select/bind map allocation failed");
/*
 *   Allocate space for a work array that will have pointers in
 *   to corresponding bind and select items; this will be sorted
 *   to give the control array.
 */
    if ((sb_work = (struct sb_pair *) calloc(d->bdp->N,
                    sizeof(struct sb_pair))) == (struct sb_pair *) NULL)
        scarper(__FILE__,__LINE__,"select/bind work allocation failed");
/*
 *   Loop - Step through the bind vars. For each one.
 */
    for (needed = 0, i = 0, x = d->bdp->base, sb_cur = sb_work;
            i < d->bdp->N;
                x = x + (d->bdp->L[i])*d->bdp->arr, i++)
    {
/*
 *   -   Find it in the select list. It is an error if it is not
 *       there.
 */
         for (j = 0; j < d->sdp->N; j++)
             if (!strncmp((d->bdp->S[i] + 1),d->sdp->S[j],d->sdp->C[j])
                && d->sdp->C[j] + 1 == d->bdp->C[i])
                 break;
         if (j >= d->sdp->N)
         {
#ifdef TABDIFF_ONLY
             fprintf(stderr, "Bind references non-existent column:%*s:\n",
                      d->bdp->C[i],d->bdp->S[i]);
#endif
             free(sb_work);
             free(d->sb_map);
             d->sb_map = (short int *) NULL;
             return;
         }
/*
 *   -   Add the address pair to the work array.
 */
         sb_cur->bind = i;
         sb_cur->sel = j;
         sb_cur++;
/*
 *   -   Allocate the indicator and variable arrays for this bind
 *       descriptor
 */
#ifdef SYBASE
        switch (d->sdp->T[j])
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
            d->bdp->T[i] = CS_FLOAT_TYPE;
            d->bdp->L[i] = 8;
            needed += 8;
            break;
        default:
            d->bdp->T[i] = ORA_CHAR;
            d->bdp->L[i] = d->sdp->L[j];
            needed += d->sdp->L[j];
            break;
        }
#else
#ifndef SQLITE3
        if (d->sdp->T[j] == ORA_DATE)
        {
            needed += 21;
            d->bdp->L[i] = 21;
        }
        else
        if ( d->sdp->T[i] == ORA_RAW
          || d->sdp->T[i] == ORA_VARRAW
#ifndef INGRES
          || d->sdp->T[i] == ORA_RAW_MLSLABEL
          || d->sdp->T[i] == ORA_LONG_VARRAW
#endif
          || d->sdp->T[i] == ORA_LONG_RAW )
        {
            d->bdp->L[i] = 2*d->sdp->L[j];
            needed += 2*d->sdp->L[j];
        }
        else
#endif
        {
            d->bdp->L[i] = d->sdp->L[j];
            needed += d->sdp->L[j];
        }
        d->bdp->T[i] = ORA_VARCHAR2;
                           /* All bind variables are null terminated CHAR */
#endif
        d->bdp->V[i] = x;
    }
    if (d->bdp->V[0] != d->bdp->base)
    {
        fputs( "Logic Error: Variables do not point at base\n", stderr);
        exit(1);
    }
/*
 * If there was insufficient space allocated, allocate enough, and repeat
 * some of the logic.
 */
    if (x > (d->bdp->bound + 1))
    {
        resize = sqlald(d->bdp->N, d->bdp->arr, 1 + needed/(d->bdp->N));
        for ( i = 0, x = resize->base;
                i < resize->N;
                    x = x + (resize->L[i])*resize->arr, i++)
        {
            resize->S[i] = d->bdp->S[i];
            resize->C[i] = d->bdp->C[i];
            resize->L[i] = d->bdp->L[i];
            resize->T[i] = d->bdp->T[i];
            resize->V[i] = x;
        }
        sqlclu(d->bdp);
        d->bdp = resize;
    }
#ifndef OSF
    else
    if (d->bdp->bound != (x - 1))
    {
        d->bdp->bound = x - 1;
/*
 * Recover surplus space. Unbelievably, OSF will move something it is
 * shrinking. What a waste of machine cycles.
 */
        resize = (E2SQLDA *) realloc(d->bdp, x - (char *) d->bdp);
        if ( resize != d->bdp )
        {
            fputs( "Logic Error: realloc() moved structure\n", stderr);
            exit(1);
        }
    }
#endif
    if (d->bdp->V[0] != d->bdp->base)
    {
        fputs( "Logic Error: Variables do not point at base\n", stderr);
        exit(1);
    }
/*
 *   Sort the array into select descriptor order.
 */
    for (sb_cur = sb_work; sb_cur < sb_work + d->bdp->N -1; sb_cur ++)
        for (sb_comp = sb_work + d->bdp->N -1; sb_cur < sb_comp; sb_comp --)
        {
            if (sb_cur->sel > sb_comp->sel)
            {
                sb_save = *sb_comp;
                *sb_comp = *sb_cur; 
                *sb_cur = sb_save; 
            }
        }
/*
 *   Populate the select/bind mapping, that will drive add_field()
 */
    for (sb_cur = sb_work,
         x2 = d->sb_map,
         j = -1;
             sb_cur < sb_work + d->bdp->N;
                 sb_cur++)
    {
        if (j != sb_cur->sel)
        {
            if (j != -1)
                *x1 = ((x2 - x1) - 1);
            x1 = x2;
            x2++;
            j = sb_cur->sel;
        }
        *x2++ = sb_cur->bind;
    }
    *x1 = ((x2 - x1) - 1);
    free(sb_work);
#ifdef SYBASE
    if (!dbms_parse(d))
                 /* Parse the entered statement  */
    {
        fprintf(stderr,"%s\n",d->statement);
        scarper(__FILE__,__LINE__,"rebind() prepare failed");
    }
#endif
    do_rebind(d);
    return;
}    /* end ini_bind_vars */
void sort_out(sess)
struct sess_con * sess;
{
    dbms_commit(sess);
    return;
}
