/************************************************************************
 * e2srclib.c - Common support routines for tabdiff and sqldrive input
 * files.
 *
 * These routines are supposed to be database-independent, but they
 * aren't entirely; knowledge of ORACLE datatypes is scattered about.
 */
static char * sccs_id =  "@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1993\n";
#ifdef E2
int __fnonstd_used=1;
#endif
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
#include "e2conv.h"
#include "tabdiff.h"
#ifndef LINUX
char * strdup();
#endif
int strcasecmp();
/*
 * Scramble control - Only work on clear text.
 */
static char scram_arr[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,
20,21,22,23,24,25,26,27,28,29,30,31,32, /* Leave Control characters alone */
37,42,47,46,41,36,35,40,45,44,39,34,33,38,43,
                                        /* Scramble some punctuation */
51,54,57,56,53,50,49,52,55,48,          /* Scramble the numbers */
59,64,63,58,60,62,61,                   /* Scramble some more punctuation */
70,75,80,85,90,89,84,79,74,69,68,73,78,83,88,87,82,77,72,67,66,71,76,
81,86,65,                               /* Scramble the Upper case letters */
93,96,95,92,91,94,                      /* Scramble some more punctuation */
84,79,74,69,68,73,78,83,88,87,82,77,72,67,66,71,76,81,86,65,70,75,
80,85,90,89,                        /* Map the Lower case letters to Upper */
 123,124,123,126,127 /* Do not map the '}' so we can safely use it as a sep. */
};
char * scramble( to_scram, scram_len)
char * to_scram;
int scram_len;
{
static char buf[WORKSPACE];
   register char * rscram, *rout;
   for (rscram = to_scram, rout = buf;
           scram_len-- > 0 && *rscram != 0;
               rscram++, rout++)
       if (*rscram > 127)
           return (char *) NULL;
       else
           *rout = *(scram_arr + *rscram);
   if (scram_len > 0)
       *rout = 0;
   return buf;
}
/**********************************************************************
 * Search for a dynamic statement control block.
 */
struct dyn_con * dyn_find(s,n)
struct sess_con * s;
int n;
{
HIPT * h;
#ifdef DEBUG
    fprintf(stderr, "dyn_find(%x,%d)\n", (int) s, n);
    fflush(stderr);
#endif
    if ((h = lookup(s->ht, (char *) n)) != (HIPT *) NULL)
        return ((struct dyn_con *) (h->body));
    else
        return (struct dyn_con *) NULL;
}
void dyn_forget(d)
struct dyn_con * d;
{
#ifdef DEBUG
    fprintf(stderr, "dyn_forget:%x\n", d);
#endif
    mem_forget(d->anchor);
    d->anchor = (struct mem_con *) NULL;
    return;
}
/*
 * Ready a statement for re-execution
 */
void dyn_reset(dyn)
struct dyn_con * dyn;
{
#ifdef DEBUG
    fprintf(stderr, "dyn_reset(%lx)\n", (long) dyn);
    fflush(stderr);
#endif
    dyn->ret_status = 0;
    dyn->so_far = 0;
    dyn->cur_ind = 0;
    dyn->to_do = 0;
#ifndef INGRES
    dyn_forget(dyn);
#endif
#ifdef SYBASE
    dyn->con->msgp = &(dyn->con->message_block[0]);
#endif
    return;
}
/************************************************************************
 * Discard stuff that does not relate to parses.
 */
void dyn_reinit(d)
struct dyn_con * d;
{
#ifdef DEBUG
    fflush(stdout);
    fprintf(stderr, "dyn_reinit(%x)\n", (int) d);
    fflush(stderr);
#endif
    if (d->sb_map != (short int *) NULL)
    {
        free(d->sb_map);         /* The database column/bind mapping     */
        d->sb_map = (short int *) NULL;
    }
#ifdef SYBASE
    if (d->sdt != (CS_INT *) NULL)
    {
        free(d->sdt);            /* Saved select descriptors             */
        d->sdt = (CS_INT *) NULL;
    }
#else
    if (d->sdt != (short *) NULL)
    {
        free(d->sdt);            /* Saved select descriptors             */
        d->sdt = (short *) NULL;
    }
#endif
    if (d->scram_flags != (char *) NULL)
    {
        free(d->scram_flags);    /* Scramble flags for the select stuff  */
        d->scram_flags = (char *) NULL;
    }
    if (d->bdp != (E2SQLDA *) 0)
    {
        sqlclu(d->bdp);          /* Free the bind variables descriptor   */
        d->bdp = (E2SQLDA *) 0;
    }
    if (d->sdp != (E2SQLDA *) 0)
    {
    int i = d->sdp->N;
    char **x = d->sdp->S;
#ifndef INGRES
        while (i-- > 0)
           free(*x++);           /* Select descriptors have malloc()'ed names */
#endif
        sqlclu(d->sdp);          /* Free the select list descriptor      */
        d->sdp = (E2SQLDA *) 0;
    }
    dyn_reset(d);
    d->is_sel = 0;
    d->sdtl = 0;
    return;
}
/*
 * Record the allocation of some memory so that it can be free()ed when
 * a cursor is binned
 */
void dyn_note(d,p)
struct dyn_con * d;
char *p;
{
#ifdef DEBUG
    fprintf(stderr, "dyn_note:%x\n", d);
#endif
    d->anchor = mem_note(d->anchor, p);
    return;
}
/*
 * Record the allocation of a descriptor so it can be released when
 * a cursor is binned
 */
void dyn_descr_note(d,p, special_routine, extra_arg)
struct dyn_con * d;
char *p;
void (*special_routine)();
int extra_arg;
{
#ifdef DEBUG
    fprintf(stderr, "dyn_note:%x\n", d);
#endif
    d->anchor = descr_note(d->anchor, p, special_routine, extra_arg);
    return;
}
/**************************************************************************
 * Convert Binary to Hexadecimal.
 */
static void hextochar(in, out, len)
char *in;
char * out;
int len;
{
    for (; len; len--, in++)
    { 
        *out = (char) (((((int ) *in) & 0xf0) >> 4) + 48);
        if (*out > '9')
           *out += (char) 7;
        out++;
        *out = (unsigned char) ((((int ) *in) & 0x0f) + 48);
        if (*out > '9')
           *out += (unsigned char) 7;
        out++;
    }
    return;
}
/*******************************************************************
 * Routine to stuff embedded quotes in strings
 */
short int quote_stuff(qs,s,n)
char * qs;
char * s;
int n;
{
register char  * qp, *sp;
    for (qp = qs, sp = s; n && *sp != '\0'; n--, sp++, qp++)
    {
        *qp = *sp;
        if (*sp == '\'')
           *(++qp) = *sp;
    }
    *qp = '\0';
    return (short int) (qp - qs);
}
/***************************************************************
 * Write out a field
 */
short int out_field(fp1,fp2,s,len,pad,l)
FILE * fp1;
FILE * fp2;
char * s;
short int len;
short int pad;
short int l;
{
    if (l > 80 - pad - len)
    {
        if (fp1 != (FILE *) NULL)
            fputc((int) '\n', fp1);
        if (fp2 != (FILE *) NULL)
            fputc((int) '\n', fp2);
        l = 0;
    }
    if (fp1 != (FILE *) NULL)
    {
#ifdef DEBUG
        if (memchr(s,0,len) !=  NULL)
        {
            fflush(stdout);
            fputs( "Logic Error: strings should not have nulls\n", stderr);
            fflush(stderr);
        }
#endif
        if (pad > 1)
            fputc((int) '\'', fp1);
        if (len > 0)
            fwrite(s,sizeof(char), len,fp1);
        if (pad > 1)
            fputc((int) '\'', fp1);
    }
    if (fp2 != (FILE *) NULL)
    {
        if (pad > 1)
            fputc((int) '\'', fp2);
        if (len > 0)
            fwrite(s,sizeof(char), len,fp2);
        if (pad > 1)
            fputc((int) '\'', fp2);
    }
    if (pad == 3 || pad == 1)
    {
        if (fp1 != (FILE *) NULL)
            fputc((int) ',', fp1);
        if (fp2 != (FILE *) NULL)
            fputc((int) ',', fp2);
        l += len + pad;
    }
    else
    {
        if (fp1 != (FILE *) NULL)
            fputc((int) '\n', fp1);
        if (fp2 != (FILE *) NULL)
            fputc((int) '\n', fp2);
    }
    return l;
}
/*
 * Output a row from a descriptor block
 */
void row_print(fp1,fp2,dp, irow, scram_flags, col_flags)
FILE * fp1;
FILE * fp2;
E2SQLDA * dp;
int irow;
char * scram_flags;
unsigned int * col_flags;
{
int i, k, l, m;
short *ip;
char buf[512];

    for (i = 0, l = 0; i < dp->N; i++)
    {                        /* For each field */
    short int pad;
    char *y, *y2, *y3;

        if (col_flags != (unsigned int *) NULL && !E2BIT_TEST(col_flags, i))
            continue;
        y = dp->V[i] + irow * dp->L[i];
        k = *(dp->O[i] + irow);
/*
 * Note that for the last item there is no trailing comma.
 */
        if (i < (dp->N - 1))
            pad = 3;
        else
            pad = 2;
        ip = (dp->I[i] + irow);
        if (*ip < 0)       /* returned value is null           */
            l = out_field(fp1,fp2,"",(short int) 0,pad,l);
        else
        switch (dp->T[i])
        {
        double d;
        long u;
#ifndef SQLITE3
#ifndef SYBASE
#ifndef INGRES
        case ORA_VARNUM:
        case ORA_NUMBER:
            pad -= 2;
            k = ora_num(y,buf,k);
            l = out_field(fp1,fp2,buf,k,pad,l);
            break;
        case ORA_DATE:
            ordate( buf, y);
            l = out_field(fp1,fp2,buf,(short int)20,pad,l);
            break;
        case ORA_RAW_MLSLABEL:
        case ORA_LONG_VARRAW:
        case SQLT_CUR: /* cursor  type */
        case SQLT_RDD: /* rowid descriptor */
        case SQLT_NTY: /* named object type */
        case 109: /* cartridge defined type? */
        case SQLT_REF: /* ref type */
        case SQLT_CLOB:  /* character lob */
        case SQLT_BLOB:  /* binary lob */
        case SQLT_BFILEE: /* binary file lob */
        case SQLT_CFILEE: /* character file lob */
        case SQLT_RSET:   /* result set type */
        case SQLT_NCO:   /* named collection type (varray or nested table) */
#endif
#endif
        case ORA_RAW:
        case ORA_VARRAW:
        case ORA_LONG_RAW:
            k = 2*k;
            if (k > sizeof(buf))
                y2 = (char *) malloc(k);
            else
                y2 = buf;
            hextochar(y, y2 ,k/2);
            l = out_field(fp1, fp2, y2, k, pad, l);
            if (y2 != buf)
                free(y2);
            break;
#endif
        case ORA_INTEGER:
            pad -= 2;
            memcpy((char *) &u,y,k);
            k = sprintf(buf,"%d",u);
            l = out_field(fp1,fp2,buf,k,pad,l);
            break;
        case ORA_FLOAT:
            pad -= 2;
            memcpy((char *) &d,y,k);
            k = sprintf(buf,"%.23g",d);
            l = out_field(fp1,fp2,buf,k,pad,l);
            break;
#ifndef SQLITE3
#ifndef INGRES
        case ORA_UNSIGNED:
            pad -= 2;
            memcpy((char *) &u,y,k);
            k = sprintf(buf,"%u",u);
            l = out_field(fp1,fp2,buf,k,pad,l);
            break;
#endif
        case ORA_PACKED:
            pad -= 2;
            strcpy(buf,pin(y,k));
            l = out_field(fp1,fp2,buf,strlen(buf),pad,l);
            break;
#endif
        default:
/*
 * Treat everything else as a character string; stuff the quote marks,
 * and write out in quotes
 */
            m = k;
            y3 = y;
            *(y + k) = '\0';
            if ( scram_flags != (char *) NULL &&
                  scram_flags[i] && (y2 = scramble(y,k)))
                memcpy(y,y2,k);   /* Scramble if requested */
            if (k  > sizeof(buf)/2 - 1)
                y2 = (char *) malloc( 2*k + 1);
            else
                y2 = buf;
            k = quote_stuff(y2,y3,k);
            if (!k && m)
            {
                k = 1;
                buf[0] = ' ';
            }
            l = out_field(fp1,fp2,y2,k,pad,l);
            if (y2 != buf && y2 != y3)
                free(y2);
            break;
        }
    }
    return;
}
/*
 * Count escapes
 */
static int count_esc(buf, len, esc_chr, col_flags)
char * buf;
int len;
char esc_chr;
unsigned int * col_flags;
{
int  esc_cnt;

    for ( esc_cnt = 0; len > 0; len--, buf++)
        if (*buf == esc_chr)
            esc_cnt++;
    return esc_cnt;
}
/*
 * Generate a delimited string from an SQLDA row. No scrambling or filtering.
 */
char * row_str(dp, irow, delim, esc_chr, col_flags)
E2SQLDA * dp;
int irow;
char delim;
char esc_chr;
unsigned int * col_flags;
{
int i, k, l, m;
short *ip;
char buf[512];
char * ret;
char * outp;
/*
 * To begin with, work out how much space we need
 */
    for (i = 0, l = 0; i < dp->N; i++)
    {                        /* For each field */
    char *y;

        if (col_flags != (unsigned int *) NULL && !E2BIT_TEST(col_flags, i))
            continue;
        y = dp->V[i] + irow * dp->L[i]; /* Where the data is */
        k = *(dp->O[i] + irow);         /* Data length */
/*
 * Note that for the last item there is no trailing delimiter.
 */
        ip = (dp->I[i] + irow);
        if (*ip < 0)       /* returned value is null           */
            l++;
        else
        switch (dp->T[i])
        {
        double d;
        long u;
#ifndef SQLITE3
#ifndef SYBASE
#ifndef INGRES
        case ORA_VARNUM:
        case ORA_NUMBER:
            k = ora_num(y,buf,k);
            l += (k + 1);
            break;
        case ORA_DATE:
            l += 21;
            break;
        case ORA_RAW_MLSLABEL:
        case ORA_LONG_VARRAW:
        case SQLT_CUR: /* cursor  type */
        case SQLT_RDD: /* rowid descriptor */
        case SQLT_NTY: /* named object type */
        case 109: /* cartridge defined type? */
        case SQLT_REF: /* ref type */
        case SQLT_CLOB:  /* character lob */
        case SQLT_BLOB:  /* binary lob */
        case SQLT_BFILEE: /* binary file lob */
        case SQLT_CFILEE: /* character file lob */
        case SQLT_RSET:   /* result set type */
        case SQLT_NCO:   /* named collection type (varray or nested table) */
#endif
#endif
        case ORA_RAW:
        case ORA_VARRAW:
        case ORA_LONG_RAW:
            l += 2*k + 1;
            break;
#endif
        case ORA_INTEGER:
            memcpy((char *) &u,y,k);
            k = sprintf(buf,"%d",u);
            l += k + 1;
            break;
        case ORA_FLOAT:
            memcpy((char *) &d,y,k);
            k = sprintf(buf,"%.23g",d);
            l += k + 1;
            break;
#ifndef SQLITE3
#ifndef INGRES
        case ORA_UNSIGNED:
            memcpy((char *) &u,y,k);
            k = sprintf(buf,"%u",u);
            l += k + 1;
            break;
#endif
        case ORA_PACKED:
            strcpy(buf,pin(y,k));
            l +=strlen(buf) + 1;
            break;
#endif
        default:
/*
 * Treat everything else as a character string
 */
            l += k + 1 + count_esc(y, k, '\\');
            break;
        }
    }
    if ((ret = (char *) malloc(l + 1)) == NULL)
        return ret;    
/*
 * Now copy the data in to position
 */
    for (i = 0, outp = ret; i < dp->N; i++)
    {                        /* For each field */
    char *y;

        if (col_flags != (unsigned int *) NULL && !E2BIT_TEST(col_flags, i))
            continue;
        y = dp->V[i] + irow * dp->L[i]; /* Where the data is */
        k = *(dp->O[i] + irow);         /* Data length */
/*
 * Note that for the last item there is no trailing delimiter.
 */
        ip = (dp->I[i] + irow);
        if (*ip < 0)       /* returned value is null           */
        {
            *outp = delim;
            outp++;
        }
        else
        switch (dp->T[i])
        {
        double d;
        long u;
#ifndef SQLITE3
#ifndef SYBASE
#ifndef INGRES
        case ORA_VARNUM:
        case ORA_NUMBER:
            k = ora_num(y, outp, k);
            outp += k;
            *outp = delim;
            outp++;
            break;
        case ORA_DATE:
            ordate( outp, y);
            outp += 20;
            *outp = delim;
            outp++;
            break;
        case ORA_RAW_MLSLABEL:
        case ORA_LONG_VARRAW:
        case SQLT_CUR: /* cursor  type */
        case SQLT_RDD: /* rowid descriptor */
        case SQLT_NTY: /* named object type */
        case 109: /* cartridge defined type? */
        case SQLT_REF: /* ref type */
        case SQLT_CLOB:  /* character lob */
        case SQLT_BLOB:  /* binary lob */
        case SQLT_BFILEE: /* binary file lob */
        case SQLT_CFILEE: /* character file lob */
        case SQLT_RSET:   /* result set type */
        case SQLT_NCO:   /* named collection type (varray or nested table) */
#endif
#endif
        case ORA_RAW:
        case ORA_VARRAW:
        case ORA_LONG_RAW:
            k += k;
            hextochar(y, outp ,k >> 1);
            outp += k;
            *outp = delim;
            outp++;
            break;
#endif
        case ORA_INTEGER:
            memcpy((char *) &u,y,k);
            outp += sprintf(outp,"%d",u);
            *outp = delim;
            outp++;
            break;
        case ORA_FLOAT:
            memcpy((char *) &d,y,k);
            outp += sprintf(outp,"%.23g",d);
            *outp = delim;
            outp++;
            break;
#ifndef SQLITE3
#ifndef INGRES
        case ORA_UNSIGNED:
            memcpy((char *) &u,y,k);
            outp += sprintf(outp,"%u",u);
            *outp = delim;
            outp++;
            break;
#endif
        case ORA_PACKED:
            strcpy(outp, pin(y,k));
            outp +=strlen(outp);
            *outp = delim;
            outp++;
            break;
#endif
        default:
/*
 * Treat everything else as a character string
 */
            while(k > 0 && *y != '\0')
            {
                *outp++ = *y;
                if (*y == esc_chr)
                    *outp++ = *y;
                y++;
                k--;
            }
            *outp = delim;
            outp++;
            break;
        }
    }
    *outp = '\0';
    outp--;
    *outp ='\n';
    return ret;
}
void desc_print(fp1,fp2,dp, to_do, scram_flags)
FILE * fp1;
FILE * fp2;
E2SQLDA * dp;
int to_do;
char * scram_flags;
{
int j;

    for (j = 0; j < to_do; j++)    /* For each retrieved row */
        row_print(fp1,fp2,dp, j, scram_flags, (unsigned int *) NULL);
    return;
}
void type_print(fp,dp)
FILE *fp;
E2SQLDA * dp;
{
char * x;
int i;
    for (i = 0; i < dp->N; i++)
    {                        /* For each field */
        switch ((int) dp->T[i])
        {
#ifndef SQLITE3
#ifndef SYBASE
#ifndef INGRES
        case ORA_RAW_MLSLABEL:
            x = "ORA_MLSLABEL";
            break;
        case ORA_DATE:
            x = "ORA_DATE";
            break;
        case ORA_NUMBER:
            x = "ORA_NUMBER";
            break;
        case ORA_LONG_VARRAW:
            x = "ORA_LONG_VARRAW";
            break;
        case ORA_VARNUM:
            x = "ORA_VARNUM";
            break;
        case ORA_VARCHAR2:
            x = "ORA_VARCHAR2";
            break;
#endif
#endif
        case ORA_RAW:
            x = "ORA_RAW";
            break;
        case ORA_VARRAW:
            x = "ORA_VARRAW";
            break;
        case ORA_LONG_RAW:
            x = "ORA_LONG_RAW";
            break;
#endif
        case ORA_INTEGER:
            x = "ORA_INTEGER";
            break;
        case ORA_FLOAT:
            x = "ORA_FLOAT";
            break;
#ifndef SQLITE3
#ifndef INGRES
        case ORA_UNSIGNED:
            x = "ORA_UNSIGNED";
            break;
#endif
        case ORA_PACKED:
            x = "ORA_STRING";
            break;
        case ORA_CHAR:
            x = "ORA_STRING";
            break;
#endif
        case ORA_VARCHAR:
            x = "ORA_VARCHAR";
            break;
        default:
            x = "OTHER";
            break;
        }
        fprintf(fp, " %s:%d len:%d",x, dp->T[i],dp->L[i]);
    }
    fputc((int) '\n',fp);
    fflush(fp);
    return;
}
/******************************************************************
 * Dump out the results of a select in a formatted manner. This is done so
 * that it is easy to get back the data, for re-insertion.
 * - All single quotes are stuffed
 * - Fields are quoted and comma-delimited
 * - New lines are inserted when the width would go over 80, or if it is
 *   now more than 80.
 * - Records are separated by a line with a / by itself
 */
void form_print(fp1,fp2,d)
FILE *fp1;
FILE *fp2;
struct dyn_con * d;
{
    desc_print(fp1,fp2,d->sdp,d->to_do, d->scram_flags);
    return;
}
/******************************************************************
 * Dump out the results of a select in a flat manner.
 */
void flat_print(fp1,fp2,d)
FILE *fp1;
FILE *fp2;
struct dyn_con * d;
{
int j;
char * p;

    if (d->sdp == NULL)
        return;
    for (j = 0; j < d->to_do; j++)    /* For each retrieved row */
    {
        p = row_str(d->sdp, j, '}', '\\', NULL);
        if (fp1 != NULL)
            fputs(p, fp1); 
        if (fp2 != NULL)
            fputs(p, fp2); 
        free(p);
    }
    return;
}
void bind_print(fp1,d)
FILE *fp1;
struct dyn_con * d;
{
    printf("Bind Print: columns N: %d F:%d arr:%d\n",
             d->bdp->N, d->bdp->F, d->bdp->arr);
#ifdef DEBUG
    fflush(fp1);
#endif
    type_print(fp1,d->bdp);
#ifdef DEBUG
    fflush(fp1);
#endif
    desc_print(fp1,(FILE *) NULL,d->bdp,1, d->scram_flags);
#ifdef DEBUG
    fflush(fp1);
#endif
    return;
}
/*
 * Ferret away a value from a descriptor. Note that this could be a native
 * ORACLE number, not necessarily a string.
 */
int save_value(w, dp, row, col)
char ** w;
E2SQLDA * dp;
int row;
int col;
{
int len;
short int * ip;
char *y;
    ip = (dp->I[col] + row);
    if (*ip >= 0)       /* returned value is not null           */
    {
        y = dp->V[col] + row * dp->L[col];
        switch (dp->T[col])
        {
#ifndef SQLITE3
#ifndef SYBASE
#ifndef INGRES
        case ORA_LONG_VARRAW:
        case ORA_RAW_MLSLABEL:
#endif
#endif
        case ORA_RAW:
        case ORA_VARRAW:
        case ORA_LONG_RAW:
            len = (*(dp->O[col] + row));
            *w = (char *) malloc(2*len + 1);
            hextochar(y, *w, len);
            len = 2*len;
            break;
#endif
        default:
            len = (*(dp->O[col] + row));
            if ((*w = (char *) malloc(len + 1)) == (char *) NULL)
            {
                fprintf(stderr, "Logic Error: malloc refused %d\n", len);
                exit(1);
            }
            memcpy(*w, y, len);
            *(*w + len) = '\0';
        }
    }
    else
    {
        len = 0;
        *w =  strdup("");
    }
    return len;
}
/*******************************************************************
 * Discard the stuff that comes back in the case of sqldrive
 */
void col_disc(fp1,fp2,d)
FILE *fp1;
FILE *fp2;
struct dyn_con * d;
{
E2SQLDA *dp;
short int i,j;
short     *ip;

    dp = d->sdp;
/*
 * Update traffic statistics
 */
    d->rows_read += d->to_do;
    d->fields_read += dp->N * d->to_do;
    d->chars_read += dp->N * d->to_do;    /* Allow one character for each
                                             NULL indicator */
    for (j = 0; j < d->to_do; j++)    /* For each retrieved row */
    {
        for (i = 0; i < dp->N; i++)
        {                        /* For each field */
            ip = (dp->I[i] + j);
            if (*ip >= 0)       /* returned value is not null           */
            {
                switch ( dp->T[i]  )
                {
#ifndef SQLITE3
#ifndef SYBASE
#ifndef INGRES
                case ORA_DATE:
                    d->chars_read += 7;
                    break;
#endif
#endif
#endif
                default:
/*
 * Everything else is a character string
 */
                    d->chars_read +=  (*(dp->O[i] + j));
                    break;
                }
            }
        }
    }
    return;
}
/*******************************************************************
 * Dump out the results of a select with a column prompt before each value
 * - Records are separated by a blank line
 */
void col_dump(fp1,fp2,d)
FILE *fp1;
FILE *fp2;
struct dyn_con * d;
{
char buf[256];
E2SQLDA *dp;
short int    i,j, k;
short     *ip;

    dp = d->sdp;
    for (j = 0; j < d->to_do; j++)    /* For each retrieved row */
    {
        if (fp1 != (FILE *) NULL)
            putc((int) '\n', fp1);
        if (fp2 != (FILE *) NULL)
            putc((int) '\n', fp2);
        for (i = 0; i < dp->N; i++)
        {                        /* For each field */
            ip = (dp->I[i] + j);
            if (*ip >= 0)       /* returned value is not null           */
            {
static char *pad="                                ";
                char *y, *y2;
                if (fp1 != (FILE *) NULL)
                {
                    fwrite(dp->S[i],sizeof(char), dp->C[i],fp1);
                    fwrite(pad,sizeof(char),32 - dp->C[i], fp1);
                    putc((int) ':', fp1);
                    putc((int) ' ', fp1);
                }
                if (fp2 != (FILE *) NULL)
                {
                    fwrite(dp->S[i],sizeof(char), dp->C[i],fp2);
                    fwrite(pad,sizeof(char),32 - dp->C[i], fp2);
                    putc((int) ':', fp2);
                    putc((int) ' ', fp2);
                }
                y = dp->V[i] + j * dp->L[i];
                k = *(dp->O[i] + j);
                switch ( dp->T[i] )
                {
                double dbl;
                long u;
#ifndef SQLITE3
#ifndef SYBASE
#ifndef INGRES
                case ORA_DATE:
                    ordate( buf, y);
                    if (fp1 != (FILE *) NULL)
                        fwrite(buf,sizeof(char), 20,fp1);
                    if (fp2 != (FILE *) NULL)
                        fwrite(buf,sizeof(char), 20,fp2);
                    break;
                case ORA_VARNUM:
                case ORA_NUMBER:
                    ora_num(y,buf,k);
                    if (fp1 != (FILE *) NULL)
                        fputs(buf,fp1);
                    if (fp2 != (FILE *) NULL)
                        fputs(buf,fp2);
                    break;
                case ORA_RAW_MLSLABEL:
                case ORA_LONG_VARRAW:
#endif
#endif
                case ORA_RAW:
                case ORA_VARRAW:
                case ORA_LONG_RAW:
                    k = 2*k;
                    if (k  > sizeof(buf))
                        y2 = (char *) malloc( k);
                    else
                        y2 = buf;
                    hextochar(y,y2,k/2);
                    if (fp1 != (FILE *) NULL)
                        fwrite(y2,sizeof(char), k,fp1);
                    if (fp2 != (FILE *) NULL)
                        fwrite(y2,sizeof(char), k,fp2);
                    if (y2 != buf)
                        free(y2);
                    break;
#endif
                case ORA_INTEGER:
                    memcpy((char *) &u,y,k);
                    if (fp1 != (FILE *) NULL)
                        fprintf(fp1,"%d",u);
                    if (fp2 != (FILE *) NULL)
                        fprintf(fp2,"%d",u);
                    break;
                case ORA_FLOAT:
                    memcpy((char *) &dbl,y,k);
                    if (fp1 != (FILE *) NULL)
                        fprintf(fp1,"%.23g",dbl);
                    if (fp2 != (FILE *) NULL)
                        fprintf(fp2,"%.23g",dbl);
                    break;
#ifndef SQLITE3
#ifndef INGRES
                case ORA_UNSIGNED:
                    memcpy((char *) &u,y,k);
                    if (fp1 != (FILE *) NULL)
                        fprintf(fp1,"%u",u);
                    if (fp2 != (FILE *) NULL)
                        fprintf(fp2,"%u",u);
                    break;
#endif
                case ORA_PACKED:
                    if (fp1 != (FILE *) NULL)
                        fputs(pin(y,k), fp1);
                    if (fp2 != (FILE *) NULL)
                        fputs(pin(y,k), fp2);
                    break;
#endif
                default:
/*
 * Treat everything else as a character string.
 */
                    if (fp1 != (FILE *) NULL)
                        fwrite(y,sizeof(char), k,fp1);
                    if (fp2 != (FILE *) NULL)
                        fwrite(y,sizeof(char), k,fp2);
                    break;
                }
                if (fp1 != (FILE *) NULL)
                    putc((int) '\n', fp1);
                if (fp2 != (FILE *) NULL)
                    putc((int) '\n', fp2);
            }
        }
    }
    return;
}
/*******************************************************************
 * Print out Descriptor Column Headings
 */
void col_head_print(fp,dp, col_flags)
FILE *fp;
E2SQLDA *dp;
unsigned int * col_flags;
{
int i;

    for (i = 0; i < dp->N; i++)
    {                        /* For each field */
        if (col_flags != (unsigned int *) NULL
          && !E2BIT_TEST(col_flags, i))
            continue;
        fwrite(dp->S[i],sizeof(char), dp->C[i],fp);
        if (i != (dp->N - 1))
            putc((int) ',', fp);
        else
            putc((int) '\n', fp);
    }
    return;
}
/*******************************************************************
 * Return Descriptor Column Headings as a string
 */
char * col_head_str(dp, delim, col_flags)
E2SQLDA *dp;
char delim;
unsigned int * col_flags;
{
int i;
int l;
char * ret;
char * p;

    for (i = 0, l = 0; i < dp->N; i++)
    {                        /* For each field */
        if (col_flags != (unsigned int *) NULL
          && !E2BIT_TEST(col_flags, i))
            continue;
        l += (1 + dp->C[i]);
    }
    l++;
    ret = (char *) malloc(l);
    for (i = 0, p = ret; i < dp->N; i++)
    {                        /* For each field */
        if (col_flags != (unsigned int *) NULL
          && !E2BIT_TEST(col_flags, i))
            continue;
        memcpy(p, dp->S[i], dp->C[i]);
        p +=  dp->C[i];
        *p = delim;
        p++;
    }
    *p = '\0';
    p--;
    *p = '\n';
    return ret;
}
/*
 * Search down the list of descriptors and do a case-insensitive comparison
 * If matched, return zero
 * If not matched, allocate one and increment the count
 */
int do_bv(anchor,bname,blen, ret_bv)
struct bv ** anchor;
char * bname;
int blen;
struct bv ** ret_bv;
{
struct bv * bv = *anchor, * last_bv = (struct bv *) NULL;
#ifdef SQLITE3
int seq = 1;
#endif

    while (bv != (struct bv *) NULL)
    {
    register char * x, *x1;
    int i;

       if (blen > 1 && blen == bv->blen)
       {
           for (x=bv->bname, x1=bname, i=blen; i; i--, x++, x1++) 
           {
               if (*x != *x1
                && !(isupper(*x) && islower(*x1) && toupper(*x1) == *x)
                && !(islower(*x) && isupper(*x1) && toupper(*x) == *x1))
                   break;
           }
           if (!i)
               break;
       }
       last_bv = bv;
       bv = bv->next;
#ifdef SQLITE3
       seq++;
#endif
    }
    if (bv != (struct bv *) NULL)
    {
#ifdef DEBUG
        printf("%*.*s\n",bv->blen,bv->blen,bv->bname);
#endif
        *ret_bv = bv;
        return 0;
    }
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
#ifdef SQLITE3
    last_bv->seq = seq;
#endif
#ifdef DEBUG
    printf("%*.*s\n",last_bv->blen,last_bv->blen,last_bv->bname);
#endif
    *ret_bv = last_bv;
    return 1;
}
/*
 * Vaguely ORACLE-compatible sqlald(). The intention is to preserve
 * the existing code as we switch from PRO*C to OCI. We do not bother
 * with the other parameters of the real ORACLE version since we do
 * not use them. We add the array size and unit allocation because
 * we are pre-allocating buffer space regardless (rather wastefully, perhaps).
 */
E2SQLDA * sqlald(cnt, arr, all)
int cnt;
int arr;
int all;
{
E2SQLDA * bdp;
char *x;
char ** x1;
int i, s;
#ifdef SYBASE
CS_INT ** x2;
CS_SMALLINT ** x3;
#else
unsigned short int ** x2;
short int ** x3;
#endif

#ifdef DEBUG
    fflush(stdout);
    fprintf(stderr, "Entered sqlald(%d, %d, %d)\n", cnt, arr, all);
    fflush(stderr);
    if (cnt == 0)
        printf("Bind descriptor with no variables!?\n");
#endif
    for (bdp = (E2SQLDA *) NULL; arr > 0 && bdp == (E2SQLDA *) NULL;)
    {
        if ((bdp = (E2SQLDA *) calloc(1, sizeof(struct sqlda) /* The SQLDA itself */
#ifdef SYBASE
            + cnt *(sizeof(char *)
                                    /* Pointer to array of pointers to
                                       Variable Names                      */
                + sizeof(CS_INT)
                                    /* Pointer to array of name lengths    */
                + sizeof(CS_INT)
                                    /* Pointer to array of Field Types     */
                + sizeof(CS_INT)
                                    /* Pointer to array of Field Precision */
                + sizeof(CS_INT)
                                    /* Pointer to array of Field Scale     */
                + sizeof(CS_INT)
                                    /* Pointer to array of Field Nullable  */
                + sizeof(CS_INT)
                                    /* Pointer to array of Field Lengths
                                       The meaning depends on the type;
                                       for numbers, scale is in low byte,
                                       precision in the others             */
                + sizeof(char *) + arr * all * sizeof(char)
                                    /* Pointer to array of pointers to
                                       (arrays of) Variable Values         */
                + sizeof(CS_SMALLINT *) + arr * sizeof(CS_INT)
                                    /* Pointer to array of pointers to
                                       (arrays of) NULL Variable
                                       Indicators                          */
               + sizeof(CS_INT *)+arr*sizeof(CS_INT)
                                    /* Pointer to array of pointers to
                                       (arrays of) Output Lengths          */
               + sizeof(CS_INT *)+arr*sizeof(CS_INT))
                                    /* Pointer to array of pointers to
                                       (arrays of) Variable Return Codes   */
#else
            + cnt *(sizeof(char *)
                                    /* Pointer to array of pointers to
                                       Variable Names                      */
                + sizeof(short int)
                                    /* Pointer to array of name lengths    */
                + sizeof(short int)
                                    /* Pointer to array of Field Types     */
                + sizeof(short int)
                                    /* Pointer to array of Field Precision */
                + sizeof(short int)
                                    /* Pointer to array of Field Scale     */
                + sizeof(short int)
                                    /* Pointer to array of Field Nullable  */
                + sizeof(int)
                                    /* Pointer to array of Field Lengths
                                       The meaning depends on the type;
                                       for numbers, scale is in low byte,
                                       precision in the others             */
                + sizeof(char *) + arr * all * sizeof(char)
                                    /* Pointer to array of pointers to
                                       (arrays of) Variable Values         */
                + sizeof(short int *) + arr * sizeof(short int)
                                    /* Pointer to array of pointers to
                                       (arrays of) NULL Variable
                                       Indicators                          */
               + sizeof(unsigned short int *)+arr*sizeof(unsigned short int)
                                    /* Pointer to array of pointers to
                                       (arrays of) Output Lengths          */
               + sizeof(unsigned short int *)+arr*sizeof(unsigned short int)
                                    /* Pointer to array of pointers to
                                       (arrays of) Variable Return Codes   */
#ifdef OR9
               + sizeof(unsigned short int *)+arr*sizeof(char *)
#endif
               )
#endif
     + sizeof(char *)*15             /* Alignment allowance */
     )) == (E2SQLDA *) NULL)
        arr = arr/2;
    }
    if (arr == 0)
    {
        if ( bdp != (E2SQLDA *) NULL)
            free((char *) bdp);
        return (E2SQLDA *) NULL;
    }
    bdp->N = cnt;                   /* Count of Fields Sized               */
    bdp->F = cnt;                   /* Count of Fields Found               */
    bdp->all = all;
    bdp->arr = arr;
    if (cnt)
    {
        bdp->S = (char **) (bdp + 1);
                                    /* Pointer to array of pointers to
                                       Variable Names                      */
        bdp->V = (char **) (bdp->S + cnt);
                                    /* Pointer to array of pointers to
                                       (arrays of) Variable Values         */
#ifdef SYBASE
        bdp->I = (CS_SMALLINT **) (bdp->V + cnt);
                                    /* Pointer to array of pointers to
                                       (arrays of) NULL Variable
                                       Indicators                          */
        bdp->O = (CS_INT **)(bdp->I + cnt);
                                    /* Pointer to array of pointers to
                                       (arrays of) Output Lengths          */
#else
        bdp->I = (short int **) (bdp->V + cnt);
                                    /* Pointer to array of pointers to
                                       (arrays of) NULL Variable
                                       Indicators                          */
        bdp->O = (unsigned short int **) (bdp->I + cnt);
                                    /* Pointer to array of pointers to
                                       (arrays of) Output Lengths          */
#endif
        bdp->R = bdp->O + cnt;      /* Pointer to array of pointers to
                                       (arrays of) Variable Return Codes   */
#ifdef SYBASE
        bdp->L = (CS_INT *) (bdp->R + cnt);
                                    /* Pointer to array of Field Lengths
                                       The meaning depends on the type;
                                       for numbers, scale is in low byte,
                                       precision in the others             */
        bdp->C = bdp->L + cnt;      /* Pointer to array of name lengths    */
#else
        bdp->L = (int *) (bdp->R + cnt);
                                    /* Pointer to array of Field Lengths
                                       The meaning depends on the type;
                                       for numbers, scale is in low byte,
                                       precision in the others             */
        bdp->C = (short int *) (bdp->L + cnt);
                                    /* Pointer to array of name lengths    */
#endif
        bdp->T = bdp->C + cnt;
                                    /* Pointer to array of Field Types     */
        bdp->P = bdp->T + cnt;
                                    /* Pointer to array of Field Precision */
        bdp->A = bdp->P + cnt;
                                    /* Pointer to array of Field Scale     */
#ifdef OR9
        bdp->bindhp = (char **) (bdp->A + cnt);
                                    /* Pointer to array of OCIDefine pointers */
        bdp->bindhp = (char **)( ((char *) bdp->bindhp)
                      + (((((long) bdp->bindhp ) % sizeof(char *)) == 0) ? 0 :
                      (sizeof(char *) -
                       (((long) bdp->bindhp ) % sizeof(char *)))));
                                    /* Avoid possible alignment problems */
        bdp->U = (unsigned short *) (bdp->bindhp + cnt);
                                    /* Pointer to array of Field Nullable  */
#ifdef DEBUG
        fprintf(stderr, "bindhp=%lx U=%lx\n",
                     (long) bdp->bindhp,  (long) bdp->U);
#endif
#else
        bdp->U = bdp->A + cnt;
#endif
/*
 * Initialise the output lengths and return pointers
 */
#ifdef SYBASE
        for (x2 = bdp->O,
             x = (char *) (bdp->U + cnt),
             i = 2 * cnt,
             s = arr * sizeof(CS_INT);
                 i;
                     i--,
                     x2++,
                     x += s)
            *x2 = (CS_INT *) x;
#else
        for (x2 = bdp->O,
             x = (char *) ((unsigned short int *) (bdp->U + cnt )),
             i = 2 * cnt,
             s = arr * sizeof(unsigned short int);
                 i;
                     i--,
                     x2++,
                     x += s)
            *x2 = (unsigned short int *) x;
#endif
/*
 * Initialise the null indicators
 */
#ifdef SYBASE
        for (x3 = bdp->I,
             i = cnt,
             s = arr * sizeof(CS_SMALLINT);
                 i;
                     i--,
                     x3++,
                     x += s)
            *x3 = (CS_SMALLINT *) x;
#else
        for (x3 = bdp->I,
             i = cnt,
             s = arr * sizeof(short int);
                 i;
                     i--,
                     x3++,
                     x += s)
            *x3 = (short int *) x;
#endif
/*
 * Now the space for the variable data
 */
        bdp->base = x;
        for (x1 = bdp->V,
             i = cnt,
             s = arr * all * sizeof(char);
                 i;
                     i--,
                     x1++,
                     x += s)
            *x1 = x;
        bdp->bound = x - 1;
    }
#ifdef DEBUG
    fputs("Left sqlald()\n", stderr);
    fflush(stderr);
#endif
    return bdp;
}
void sqlclu(bdp)
E2SQLDA * bdp;
{
#ifdef DEBUG
    fprintf(stderr, "sqlclu(%lx)\n", (long) bdp);
    fflush(stderr);
#endif
    if (bdp != (E2SQLDA *) NULL)
        free((char *) bdp);
    return;
}
/***************************************************************************
 * Allocate a display control structure. Will be tied to some drawing mechanism.
 */
struct disp_con * disp_new(bdp, rows)
E2SQLDA * bdp;
int rows;
{
struct disp_con * dp = (struct disp_con *) malloc(sizeof(struct disp_con));
int row_len;
int i, j;
int *lp;
char * x;

    if (dp == (struct disp_con *) NULL)
        return  (struct disp_con *) NULL;
    dp->dsel = (struct dyn_con *) NULL; /* Data source                  */
    dp->dins = (struct dyn_con *) NULL; /* What to do with new rows     */
    dp->dupd = (struct dyn_con *) NULL; /* What to do with updated rows */
    dp->ddel = (struct dyn_con *) NULL; /* What to do with deleted rows */
    dp->title = (char *) NULL;          /* Title (optional)             */
    dp->col_flags = (unsigned int *) malloc( ((bdp->F - 1)/32 + 1) *
                                        sizeof(unsigned int));
    memset((char *) dp->col_flags, 0xff,
                         ((bdp->F - 1)/32 + 1) * sizeof(unsigned int));
/*
 * Work out how long it needs to be.
 */
    for (row_len = 0, i = 0, j = bdp->N, lp = bdp->L; i < j; i++, lp++)
        row_len += *lp;
    
    dp->bdp = sqlald(bdp->N, rows, 1 + row_len/bdp->N);
                           /* Descriptor that holds current values     */
    rows = dp->bdp->arr;   /* In case we get short changed by malloc() */
    
    disp_reset(dp);
/*
 * Copy across common elements
 */
    for (i = 0, x = dp->bdp->base; i < j; i++)
    {
        dp->bdp->S[i] = strdup(bdp->S[i]);
                                    /* Pointer to array of pointers to
                                       Variable Names                      */
        dp->bdp->L[i] = bdp->L[i];
                                    /* Pointer to array of Field Lengths
                                       The meaning depends on the type;
                                       for numbers, scale is in low byte,
                                       precision in the others             */
        dp->bdp->C[i] = bdp->C[i];
                                    /* Pointer to array of name lengths    */
        dp->bdp->T[i] = bdp->T[i];
                                    /* Pointer to array of Field Types     */
        dp->bdp->P[i] = bdp->P[i];
                                    /* Pointer to array of Field Precision */
        dp->bdp->A[i] = bdp->A[i];
                                    /* Pointer to array of Field Scale     */
        dp->bdp->U[i] = bdp->U[i];
                                    /* Pointer to array of Field Nullable  */
        dp->bdp->V[i] = x;
                                    /* Pointer to array of pointers to
                                       (arrays of) Variable Values         */
        x += rows * dp->bdp->L[i];
    }
    return dp;
}
/***************************************************************************
 * Allocate a display control structure. Will be tied to some drawing mechanism.
 */
void disp_destroy(dp)
struct disp_con * dp;
{
int row_len;
int i, j;
int *lp;
char * x;

    if (dp == (struct disp_con *) NULL)
        return;
/*
 * This routine does nothing with any associated dynamic statement control
 * blocks
 */
    free(dp->col_flags);
    sqlclu(dp->bdp);
    return;
}
/***************************************************************************
 * Dump out a display control structure
 */
void disp_print(fp, dp)
FILE *fp;
struct disp_con * dp;
{
int j;

    if (dp != (struct disp_con *) NULL && dp->to_do > 0)
    {
        if ( dp->title != (char *) NULL)
        {
            fputs( dp->title, fp);
            fputc('\n', fp);
        }
        col_head_print(fp, dp->bdp, dp->col_flags);
        for (j = 0; j < dp->to_do; j++)    /* For each retrieved row */
            row_print(fp,(FILE *) NULL,dp->bdp, j, (char *) NULL,
                    dp->col_flags);
    }
    return;
}
/*
 * Clear display control variables
 */
void disp_reset(dp)
struct disp_con * dp;
{
    if (dp != (struct disp_con *) NULL)
    {
        dp->to_do = 0;         /* Number remaining for processing       */
        dp->so_far = 0;        /* Number retrieved so far               */
        dp->cur_ind = 0;       /* The current array position            */
        dp->fld_ind = 0;       /* The current field position            */
    }
    return;
}
/***************************************************************************
 * Allocate a dictionary, to remember data type details.
 */
struct dict_con * new_dict(cnt)
int cnt;                /* Anticipated number of inhabitant */
{
struct dict_con * x;

    if ((x = (struct dict_con *) malloc(sizeof(*x)))
             == (struct dict_con *) NULL)
        return x;
    x->bvt = hash(cnt,string_hh,
          (COMPFUNC) strcasecmp);    /* Bind Variable Hash Table */
    x->anchor = (struct bv *) NULL;             /* Bind Variable chain      */
    return x;
}
/*
 * We only supply the data we are interested in; no precision or scale
 * for instance
 */
void dict_add(dict, bname,dbtype,dsize)
struct dict_con * dict;
char * bname;
short dbtype;
int dsize;
{
register struct bv * x;

    if ((x = (struct bv *) malloc(sizeof(struct bv))) == (struct bv *) NULL)
        return;
    x->next = dict->anchor;
    dict->anchor = x;
    x->bname = strdup(bname);
    x->dbtype = dbtype;
    x->dsize = dsize;
    x->dbsize = dsize;
    x->blen = strlen(bname);
    x->prec = 0;
    x->scale = 0;
    x->nullok = 1;
    insert(dict->bvt,x->bname, (char *) x);
    return;
}
/*
 * Search for a bind variable in a dictionary
 */
struct bv * find_bv(dict,bname,blen)
struct dict_con * dict;
char * bname;
int blen;
{
char sname[64];
int slen = (blen > 63) ? 63 : blen;
HIPT * h;
    memcpy(sname,bname,slen);
    *(&(sname[0]) + slen) = '\0';
    if ((h = lookup(dict->bvt, &sname[0])) == (HIPT *) NULL)
        return (struct bv *) NULL;
    else
        return (struct bv *) (h->body);
}
/*
 * Free all memory allocated to a dictionary
 */
void dict_deall(dict)
struct dict_con * dict;
{
    iterate(dict->bvt, free, free);
    cleanup(dict->bvt);
    free(dict);
    return;
}
/**********************************************************************
 * Add a field to the designated block
 *
 * This drives off the select/bind descriptor mapping (which defines how
 * the input data relates to the output data) when used by tabdiff.
 */
void add_field(dyn)
struct dyn_con * dyn;
{
short int i,j,l,r;
register E2SQLDA * rd;
double d;

    rd= dyn->bdp;
    if (dyn->fld_ind == 0)
        dyn->cur_map = dyn->sb_map;
               /* reset to the beginning of the list */
    r = dyn->cur_ind;   /* Record Number */
    l = strlen(tbuf);   /* Field Length */
    dyn->chars_sent += l;
    dyn->fields_sent++;
    if (dyn->cur_map != (short int *) NULL)
    {   /* There could be many of them */
        for (j = *(dyn->cur_map++); j; j--)
        {
            i = *(dyn->cur_map++);   /* Field Number */
#ifdef DEBUG
            fprintf(stderr,
"rd->base:%x rd->bound:%x rd->I[i] + r + 1:%x\n\
rd->V[i] + (r + 1)*rd->L[i]:%x i:%d r:%d L[i]:%d\n",
                           (unsigned long)  rd->base,
                           (unsigned long)  rd->bound,
                           (unsigned long)  (rd->I[i] + r + 1),
                           (unsigned long) (rd->V[i] + (r+1)*(rd->L[i])),
                            i, r, rd->L[i]);
            fflush(stderr);
            if (((char *) (rd->I[i] + (r+1)) > rd->base))
            {
               fputs( "Logic Error: Indicators go too far\n", stderr);
               exit(1);
            }
            if ((rd->V[i] + (r + 1) *rd->L[i]) > rd->bound + 1
             || (rd->V[i] + r*rd->L[i]) < rd->base)
            {
               fputs( "Logic Error: Variables go too far\n", stderr);
               exit(1);
            }
#endif
            if (l)
            {           /* Non-zero length */
                *(rd->I[i] + r) = 0;
                if (rd->T[i] == ORA_FLOAT)
                {
                    d = strtod(tbuf, (char **) NULL);
                    memcpy(rd->V[i] + r*rd->L[i], &d, sizeof(d));
                }
                else
                    strncpy(rd->V[i] + r*rd->L[i],tbuf,rd->L[i]);
            }
            else        /* Zero length */
            {
                *(rd->I[i] + r) = -1;  /* Set indicator to NULL */
                *(rd->V[i] + r*rd->L[i]) = '\0';
            }
            *(rd->O[i] + r) = l;
        }
    }
    else
    {
        if ((i = dyn->fld_ind) < rd->N)
        {
            if (l)
            {           /* Non-zero length */
                *(rd->I[i] + r) = 0;
                strncpy(rd->V[i] + r*rd->L[i],tbuf,rd->L[i]);
            }
            else        /* Zero length */
            {
                *(rd->I[i] + r) = -1;  /* Set indicator to NULL */
                *(rd->V[i] + r*rd->L[i]) = '\0';
            }
            *(rd->O[i] + r) = l;
        }
    }
    dyn->fld_ind++;
    return;
}
/**********************************************************************
 * Handle premature EOF
 */
enum tok_id prem_eof(cur_pos, cur_tok)
char * cur_pos;
enum tok_id cur_tok;
{            /* Allow the last string to be unterminated */
    *tlook = '\0';
    *cur_pos = '\0';
    look_status = PRESENT;
    switch (cur_tok)
    {
    case FIELD:
        look_tok = EOR; 
        break;
    case SQL:
    case EOR:
        look_tok = PEOF;
        break;
    default:
        break;
    }
    return cur_tok;
}
/**********************************************************************
 * Unstuff a field into tbuf
 */
static enum tok_id unstuff_field(fp, cur_pos)
FILE * fp;
char * cur_pos;
{
int p;

    for(;;)
    {
        p = getc(fp);
        if (p == EOF)
            return (prem_eof(cur_pos,FIELD));
        else
        if (p == (int) '\'')
        {            /* Have we come to the end? */
        int pn;

             pn = getc(fp);
             if (pn != (int) '\'')
             {   /* End of String */
                *cur_pos = '\0';
                if (pn != (int) ',')
                {    /* End of record */
                    *tlook = '\0';
                    look_status = PRESENT;
                    look_tok = EOR; 
                }
                return FIELD;
            }
        }
        *cur_pos++ = (char) p;
    }
}
static enum tok_id get_number(fp, cur_pos)
FILE * fp;
char * cur_pos;
{
int p;
char * x;                      /* for where strtod() gets to */
char * save_pos = cur_pos;
double ret_num;
int int_flag;
/*
 * First pass; collect likely numeric characters
 */
    int_flag = 1;
    for (save_pos = cur_pos,
         cur_pos++,
         *cur_pos =  getc(fp);
             *cur_pos == '.' || ( *cur_pos >='0' && *cur_pos <= '9');
                 cur_pos++,
                 *cur_pos = getc(fp))
         if (*cur_pos == '.')
             int_flag = 0;
    p = (int) *cur_pos;
    (void) ungetc(*cur_pos,fp);
    *cur_pos = '\0';
/*
 * Now apply a numeric validation to it
 */
    ret_num = strtod(save_pos, &x);
/*
 * Push back any over-shoot
 */
    while (cur_pos > x)
    {
        cur_pos--;
        (void) ungetc(*cur_pos,fp);
    }
    *cur_pos = '\0';
#ifdef DEBUG
    (void) fprintf(stderr,"Found NUMBER %s\n", save_pos);
    (void) fflush(stderr);
#endif   
    if (getc(fp) != (int) ',')
    {    /* End of record */
        *tlook = '\0';
        look_status = PRESENT;
        look_tok = EOR; 
    }
    if (int_flag)
        return E2FLONG;
    else
        return FNUMBER;
}
#ifdef NOCALC
int cscalc(fp,cp, y, z)
FILE *fp;
char * cp;
char * y;    /* Actually pointers to structures, but who cares ... */
char * z;
{
    return 1;
}
#endif
/*
 * Resolve symbols
 */
int recognise_scan_spec()
{
    return -1;
}
char * find_scan_spec()
{
    return NULL;
}
/**********************************************************************
 * Evaluate an expression that incorporates variables.
 */
static enum tok_id eval_expr(fp, cur_pos, ret_len)
FILE * fp;
char * cur_pos;
int *ret_len;
{
    if (con == NULL)
    {
#ifdef DEBUG
        puts("Evaluation Impossible");
#endif
        *cur_pos = '\0';
        *ret_len = 0;
    }
    else
    {
        cscalc(fp,(char *) NULL, &con->csmacro, NULL);
        if (con->csmacro.ret_symbol.sym_value.tok_type == BTOK)
        {
            if ((*ret_len = con->csmacro.ret_symbol.sym_value.tok_len))
                memcpy(cur_pos, con->csmacro.ret_symbol.sym_value.tok_ptr, *ret_len);
            else
                *cur_pos = '\0';
        }
        else
        {
            if (con->csmacro.ret_symbol.sym_value.tok_type == NTOK
             || con->csmacro.ret_symbol.sym_value.tok_type == DTOK)
                sprintf(cur_pos, "%.13g", con->csmacro.ret_symbol.sym_value.tok_val);
            else
            {
                memcpy(cur_pos,  con->csmacro.ret_symbol.sym_value.tok_ptr,
                                 con->csmacro.ret_symbol.sym_value.tok_len);
                *(cur_pos + con->csmacro.ret_symbol.sym_value.tok_len) = '\0';
            }
            if (*cur_pos == '\0')
            {
#ifdef DEBUG
                fflush(stderr);
                puts("NULL Variable Found");
                fflush(stdout);
#endif
                *ret_len = 0;
            }
            else
            {
#ifdef DEBUG
                fflush(stderr);
                printf("Variable Found - Type %d Value:%s\n",(int)
                            con->csmacro.ret_symbol.sym_value.tok_type,cur_pos);
                fflush(stdout);
#endif
                *ret_len = strlen(cur_pos);
            }
        }
        Lfree();                 /* return Lmalloc()'ed space */
    }
    if (getc(fp) != (int) ',')
    {    /* End of record */
        *tlook = '\0';
        look_status = PRESENT;
        look_tok = EOR; 
    }
    if (*ret_len == 0 || con->csmacro.ret_symbol.sym_value.tok_type == STOK)
        return FIELD;
    else
    if ( con->csmacro.ret_symbol.sym_value.tok_type == NTOK)
        return FNUMBER;
    else
    if ( con->csmacro.ret_symbol.sym_value.tok_type == DTOK)
        return FDATE;
    else
    if ( con->csmacro.ret_symbol.sym_value.tok_type == BTOK)
        return FRAW;
    else
        return FIELD;
}
/**********************************************************************
 * Read a PATH command string into the buffer.
 * Return 1 if valid, 0 if not.
 */
int get_command(fp,cur_pos)
FILE * fp;
char * cur_pos;
{
char * ini_pos = cur_pos;

    *cur_pos = (signed char) getc(fp);
    if (*cur_pos == (signed char) EOF || *cur_pos == '\\')
        return 0;     /*ignore empty non-terminated command*/
    cur_pos++;
    for (*cur_pos = (signed char) getc(fp);
        *cur_pos != (signed char) EOF;
             cur_pos++,
             *cur_pos = (signed char) getc(fp))
                    /* advance to end of string,
                     * treating '\\' as an escape character
                     * storing it in cur_pos_buf
                     */
        if (*cur_pos == (int) '\\')
        {
            *cur_pos = (signed char) getc(fp);
            if (*cur_pos != (int) '\\')
            {
               (void) ungetc(*cur_pos,fp);
                             /* pop back this character */
               break;        /* got the lot */
            }
            /* otherwise, we have stripped an escape character */
        }   
    if (*ini_pos == 'C' || (*ini_pos == 'M' && (cur_pos - ini_pos > 4)))
        return 0;           /* Ignore comments and message directives */
    *cur_pos = '\0';        /* terminate the string */
    return 1;
}
/**********************************************************************
 * Read the next token
 */
enum tok_id get_tok(fp)
FILE * fp;
{
int p;
char * cur_pos;
/*
 * If look-ahead present, return it
 *
 * This code also processes PATH timing commands.
 * These are not returned, but are edited out, like comments.
 */
#ifdef DEBUG
   fprintf(stdout, "get_tok() called, look_status %d\n", (int) look_status);
   fflush(stdout);
#endif
restart:
    if (look_status == PRESENT)
    {
        strcpy(tbuf,tlook);
        look_status = CLEAR;
        return look_tok;
    }
    else
        cur_pos = tbuf; 
    while ((p = getc(fp)) == (int) '\n'); /* skip any blank lines */
/*
 * Scarper if all done
 */
    if ( p == EOF )
        return PEOF;
/*
 * Pick up the next token, stripping off the wrapper for fields.
 */
    else
    {
        switch(p)
        {
        case '-':
        case '.':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':                         /* apparently a number */
            *cur_pos = p;
            return get_number(fp, cur_pos);
        case (int) '\'':
        {      /* This is a FIELD; search for the end of the string
                * Strings are returned unstuffed, and without wrapping quotes
                */
            return unstuff_field(fp,cur_pos);
        }
        case (int) '[':
        {      /* This is an expression. The expression is evaluated.
                */
            return eval_expr(fp,cur_pos, &tlen);
        }
        case (int) '\\':
        {        /* Command string; start timing, end timing or delay */
             
             if (!get_command(fp, cur_pos))
             {
                 if (feof(fp))
                     return PEOF;
                 else
                     goto restart;
             }
             return COMMAND;
        }
        default:   /* an SQL statement or Command */
        {      /* Search for \n/\n */
        enum match_state { NOTHING, NEW_LINE, SLASH };
        enum match_state match_state;
        match_state = NEW_LINE;

            for(;;)
            {
                if (p == EOF)
                    return (prem_eof(cur_pos,SQL));
                switch (match_state)
                {
                case NOTHING:
                    if (p == (int) '\n')
                        match_state = NEW_LINE;
                    else
                        *cur_pos++ = (char) p;
                    break;
                case NEW_LINE:
                    if (p == (int) '/')
                        match_state = SLASH;
                    else
                    if (p == (int) '\\')
                    {        /* Command string ? */
                         if (get_command(fp, cur_pos))
                         {
                             strcpy(tlook, cur_pos);
                             look_status = PRESENT;
                             look_tok = COMMAND;
                             *cur_pos = '\0';
                             return SQL;
                         }
                         else
                         {
                             *cur_pos++ = '\n';
                             match_state = NOTHING;
                         }
                    }
                    else
                    {
                        *cur_pos++ = (char) '\n';
                        if (p != (int) '\n')
                        {
                            match_state = NOTHING;
                            *cur_pos++ = (char) p;
                        }
                    }
                    break;
                case SLASH:
                    if (p == (int) '\n')
                    {
                        *cur_pos = '\0';
                        return SQL;
                    }
                    else
                    {
                        match_state = NOTHING;
                        *cur_pos++ = (char) '\n';
                        *cur_pos++ = (char) '/';
                        *cur_pos++ = (char) p;
                    }
                    break;
                }
                p = getc(fp);
#ifdef DEBUG
                putchar(p);
                fflush(stdout);
#endif
            }
        }
        }
    }
}
/*
 * Output bind variables. 
 *
 * The input argument is a pointer to a length, which is followed
 * by the data.
 */
#ifndef SYBASE
#ifndef INGRES
short int ora_field(fp,ftype,p, offset, last_flag, top)
FILE * fp;                /* Output File                                */
int ftype;                /* ORACLE data type of the field to be output */
unsigned char * p;        /* Pointer to the field length byte           */
short int offset;         /* Current position on line                   */
int last_flag;            /* 1 if the last field, 0 otherwise           */
unsigned char * top;
{
int no_quote = 0;
int out_len;
unsigned int in_len;
float f;
unsigned char * op;
unsigned char buf[2048];
unsigned char * ret=&buf[0];

    in_len = *p;
    if ((p >= top)
     || (*p == 253 && *(p+1) == 1)
     || (*p == 253 && *(p+1) == 1)
     || ftype == 0)
    {
        out_len = 0;
        buf[0] = 0;
        if (*p == 253)
            in_len = 1;
        else
            in_len = 0;
    }
    else
    switch (ftype)
    {
#ifndef SQLITE3
    case ORA_NUMBER:              /* Up to 22 bytes long        */
    case ORA_VARNUM:              /* Internal numeric representation? */
        out_len = (int) ora_num(p,&buf[0], -1);
        no_quote = 2;
        break;
#endif
    default:
#ifdef DEBUG
        printf("Unrecognised ORACLE Data Type %d, len[1] = %d, len[2] = %d\n",
                 ftype, (int) *p, (int) *(p+1));
        (void) gen_handle(stdout, p,top,1);
        fflush(stdout);
#endif
        ftype = ORA_VARCHAR2;     /* Guess unknown as characters */
    case ORA_VARCHAR2:            /* Up to 2000 bytes long!?    */
#ifndef SQLITE3
    case ORA_ROWID:               /* 6 bytes long               */
    case ORA_MLSLABEL:            /* 4 bytes long               */
    case ORA_RAW:                 /* Up to 255 byte long        */
    case ORA_CHAR:                /* Up to 255 byte long        */
    case ORA_STRING:              /* Converts to null-terminated string */
    case ORA_CHARZ:               /* Null terminated char data  */
    case ORA_DISPLAY:             /* COBOL Display data type    */
    case ORA_RAW_MLSLABEL:        /* Up to 255 bytes long       */
#endif
/*
 * Duplicate length; skip
 */
        if (in_len == 1 && *(p + 1) == *(p + 2))
        {
            ret = p + 3;
            out_len = *(p + 1);
            in_len = out_len;
        }
        else
        if (in_len == 2 && *(p + 3) == 0xff )
        {
            in_len = counted_int(p,1);
            ret = p + 4;
            out_len = in_len;
        }
        else
        {
            if (in_len == 2 && *(p + 3) == 0xfe )
            {
                in_len = 254;
                ret = p + 4;
            }
            else
                ret = (p + 1);
            if (in_len == 254)
            {
                if ((p = memchr(ret,0,top - p)) !=  NULL)
                     out_len = (p - ret);
                else
                     p = ret;
            }
            else
            {
                if ((p = memchr(ret,0,in_len)) !=  NULL)
                    out_len = (p - ret);
                else
                {
                    out_len = in_len;
                    p = ret;
                }
            }
            if (p != ret)
            {
                for (p = ret, op = &buf[0];
                        p < top && *p != 0; op += *p, p += *p + 1)
                    memcpy(op, p+1, *p);
                ret = &buf[0];
                out_len = op - &buf[0];
            }
        }
        break;
#ifndef SQLITE3
    case ORA_DATE:                /* 7 bytes long               */
        ordate(&buf[0],p + 1);
        out_len=20;
        break;
    case ORA_UNSIGNED:             /* Unsigned long int          */
#endif
    case ORA_INTEGER:              /* Use for signed char, short  or long */
        if (!(*p))
        {
            buf[0] = '\0';
            out_len = 0;
        }
        else
        {
            no_quote = 2;
            if (*p == ORA_UNSIGNED)
                out_len = sprintf(&buf[0],"%u",counted_int(p+1,1));
            else             
                out_len = sprintf(&buf[0],"%d",counted_int(p+1,0));
        }
        break;
    case ORA_FLOAT:                /* Use for float or double    */
        no_quote = 2;
        if (*p == 4)
        {
            memcpy((char *) &f, p+1, 4);
            out_len = sprintf(&buf[0],"%.23g",(double) f);
        }
        else
        {
            memcpy((char *) &f, p+1, 8);
            out_len = sprintf(&buf[0],"%.23g",(double) f);
        }
        break;
#ifndef SQLITE3
    case ORA_PACKED:               /* Packed Decimal (not used much in C) */
        no_quote = 2;
        out_len = in_len * 2;
        p = pout(&buf[0], p+1, out_len);
        break;
#ifdef WEIRD
    case ORA_VARCHAR:              /* The traditional VARCHAR struct */
    case ORA_VARRAW:               /* A VARCHAR-like way of returning RAW */
        out_len = (int) ((in_len << 8) + ((unsigned) *(p+1)));
        ret = (char *) (p + 2); 
        break;
    case ORA_LONG:                 /* Up to 2**31 - 1 bytes long */
    case ORA_LONG_RAW:             /* Up to 2**31 - 1 bytes long */
    case ORA_LONG_VARCHAR:         /* Long int + byte data       */
    case ORA_LONG_VARRAW:          /* Long int + byte data       */
        out_len = (int) (((in_len *p) << 24) + (((unsigned) *(p+1)) << 16) +
               (((unsigned) *(p + 2)) << 8) + ((unsigned) *(p+3)));
        ret = (char *) (p + 4); 
        break;
#endif
#endif
    }
/*
 * Note that this wouldn't work if there were genuinely to be a huge length
 * However, the SQL*NET V.2 protocol ensures that there will not
 * be.
 */
    if (in_len > 65536 || out_len < 0)
    {
        printf("Failure to understand ORACLE Type %d, in_len = %u\n", ftype,
                  in_len);
        (void) gen_handle(stdout, p,top,1);
        fflush(stdout);
        return -1;
    }
    return out_field(fp,(FILE *) NULL, ret, (short int) out_len,
             (short int) ((last_flag ? 2 : 3) - no_quote), offset);
} 
#endif
#endif
/*
 * Routine to break up SQL statements so that an editor can be
 * used on them.
 *
 * Input Parameters :
 * - Length
 * - Statement
 * - Statement
 * - Return value for how much is still to come
 * Returns:
 * - 0 for a PL/SQL statement
 * - -1 for an explainable statement
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
int wrapsql(fp, len, buf, top, more_flag)
FILE *fp;
int len;
unsigned char * buf;
unsigned char * top;
int * more_flag;
{
unsigned char * bound;
int ret = -1;
unsigned char *x1, *x2;
int i;
unsigned char *x = buf;

#ifdef DEBUG
    fprintf(fp, "wrapsql(%lx, %d, %lx, %lx, %lx)\n",
          (long) fp, len, (long) buf, (long) top, (long) more_flag);
    fflush(fp);
#endif
    if (top < buf+len)
    {
        *more_flag = buf + len - top;
        i = top - buf;
    }
    else
    {
        *more_flag = 0;
        i = len;
    }
/*
 * Trim leading spaces and trailing line feeds
 */
    while (i > 0
      && (*x == ' ' || *x == '\t' || *x == '\r' || *x == '\n'))
    {
        x++;
        i--;
    }
    bound = x + i - 1;
    if (!(*more_flag))
    {
        while (i > 0
          && (*bound == ' ' || *bound == '\t' || *bound == '\r'
               || *bound == '\n'))
        {
            bound--;
            i--;
        }
    }
    if ((strspn(x,"DdEeCcLlAaRr") == 7) || (strspn(x,"BbEeGgIiNn") == 5))
        ret = 0;
/*
 * Break up the statement into separate lines:
 * - Do not split strings over lines.
 * - Inject line feeds when multiple spaces are not in a string and
 *   The multiples do not appear at the start of the line.
 */
    for (x1 = x, x2 = x; x <= bound; )
    {
    int flag1, flag2;
/*
 * Search forwards
 */
        for (i = 78, flag1=0, flag2 = 0; i; i--, x++)
        {
/*
 * We print the line so far if:
 * - We have reached the end of the statement
 * - We have encountered a line feed
 * - We are not in a string, we have seen a non-space character, and we
 *   encounter two adjacent spaces (the assumption being that multiple
 *   spaces are only used for indenting at the start of a line
 */
            if (x >= bound || *x == '\n' ||
               (flag1 == 0 && flag2 == 1 && *x == ' '
                && x != bound && *(x+1) == ' ' ))
            {   /* Line feed, flush anyway */
                fwrite(x1,sizeof(char),(x-x1 + 1),fp);
                if (flag1 == 0 && flag2 == 1
                    && *x == ' ' && x != bound && *(x+1) == ' ')
                    fputc('\n', fp);
                x1 = x + 1;
                x = x1;
                x2 = x1;
                flag1 = 2;    /* ie. We have printed it */
                break;
            }
            else
            if (*x == '\'')
            {
                flag2 = 1;
                if (flag1 == 0)
                {
                    flag1 = 1;
                    x2 = x;
                }
                else
                if (*(x+1) != '\'' )
                {
                    x2 = x + 1;
                    flag1 = 0;
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
 * injected a line feed) or we have gone 78 characters. In this case,
 * we need to move x backwards until it points to a space, or we reach
 * the bounds of a string (that is, x2)
 */
        if ( flag1 == 1)
        {   /* We are in the middle of a string */
            if (x2 != x1)
            {    /* There was data before the string */
                fwrite(x1,sizeof(char),(x2-x1),fp);
                fputc('\n', fp);
                x = x2;
                x1 = x2; 
            }
            else   /* The string is longer than 78; search for its
                      end */
            {
                for (x = x2 + 1;
                       x <= bound && *x != '\n' && *x != '\0';
                           x++)
                {
                    if (*x == '\'')
                    {
                        if (x == bound || *(x+1) != '\'')
                            break;
                        else
                            x++;
                    }
                }
                fwrite(x1,sizeof(char),(x-x1 + 1),fp);
                x++;
/*
 * Inject a line feed
 */
                if (x <= bound && *x != '\n')
                    fputc('\n', fp);
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
            for (; *x != ' ' && *x != ',' && x > x2; x--);
            fwrite(x1,sizeof(char),(x-x1 + 1),fp);
            if (x != x2)
                fputc('\n', fp);
            x1 = x + 1;
            x2 = x1;
        }
    } 
    if (!(*more_flag))
        fputs("\n/\n", fp);
#ifdef DEBUG
    fflush(fp);
#endif
    return ret;
}
