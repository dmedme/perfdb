/************************************************************************
 * englex.c - Routines to support finding english words in arbitrary
 * potentially binary files.
 ***********************************************************************
 *This file is a Trade Secret of E2 Systems. It may not be executed,
 *copied or distributed, except under the terms of an E2 Systems
 *PATH license. Presence of this file on your system cannot be construed
 *as evidence that you have such a licence.
 ***********************************************************************/
static unsigned char * sccs_id =
    (unsigned char *) "Copyright (c) E2 Systems Limited 1994\n\
@(#) $Name$ $Id$";
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include "IRfiles.h"
#ifndef O_BINARY
#define O_BINARY 0
#endif
static unsigned char buf1[16384]; /* One of the two alternate buffers         */
static int buf1_cnt;             /* How many bytes are occupied in it         */
static unsigned char buf2[16384]; /* The other of the two alternate buffers   */
static int buf2_cnt;             /* How many bytes are occupied in it         */
static unsigned char * cur_base; /* The base of the current buffer            */
static unsigned char * cur_ptr;  /* The next character in the current buffer  */
static unsigned char * cur_top;  /* The character past the current buffer end */
static int back_flag;
static int eof_flag;             /* We have reached the end of the stream     */
static int white_space;          /* If true, all white space matches          */
static int case_insense;         /* If true, matches are case insensitive     */
/*
 * Compress a buffer by:
 * -    Ignoring non-printable characters
 * -    Converting Unicode ASCII characters to ASCII characters by
 *      simple-mindedly removing the NUL's between them.
 * -    Reducing white space to single characters
 *
 * Return the new length
 */
static int strip_uninteresting(wrd,i)
unsigned char * wrd;
int i;
{
unsigned register char * x = wrd;
unsigned register char * y = x;
unsigned register char * bound;
register char * top = x + i;

    while ((x = bin_handle(NULL, x,top, 0)) < top) 
    {
        if (((bound = uni_handle(NULL, x, top, 0)) - x) > 3)
        {
            while ( x < bound)
            {
                *y = *x;
                y++;
                if ( isspace(*x))
                {
                    do
                    {
                        x += 2;
                    }
                    while (x < bound && isspace(*x));
                }
                else
                    x += 2;
            }
            *y++ = '\0';
        }
        else
        {
            bound = asc_handle(NULL, x, top, 0);
            while ( x < bound)
            {
                *y++ = *x;
                if ( isspace(*x))
                {
                    do
                    {
                        x++;
                    }
                    while (x < bound && isspace(*x));
                }
                else
                        x++;
            }
            if (y < top)
            {
                *y++ = '\0';
                if (x < y)
                    x = y;
            }
        }
    }
    return y - wrd;
}
/*
 * ready to handle another file
 */
static void cclear()
{
    buf1_cnt = 0;
    buf2_cnt = 0;
    cur_top = (unsigned char *) NULL;
    cur_ptr = (unsigned char *) NULL;
    cur_base = (unsigned char *) NULL;
    eof_flag = 0;
    back_flag = 0;
    return;
}
/*
 * Return the next character from the stream
 *
 * This routine handles case insensitivity and white space equivalence.
 */
static int cget(fd)
int fd;
{
int ret;
/*
 * Buffer switch or first time?
 */
    if (cur_base == (unsigned char *) NULL
     || (cur_base == &buf2[0] && cur_ptr >= cur_top &&
                         (!eof_flag || back_flag)))
    {
        if (!back_flag)
        {
            buf1_cnt = read(fd, &buf1[0], sizeof(buf1));
            if (buf1_cnt < sizeof(buf1))
                eof_flag = 1;
            buf1_cnt = strip_uninteresting(&buf1[0],buf1_cnt);
        }
        else
            back_flag  = 0;
        cur_ptr = &buf1[0];
        cur_base = cur_ptr;
        cur_top = cur_base + buf1_cnt;
    }
    else
    if (cur_base == &buf1[0] && cur_ptr >= cur_top && (!eof_flag || back_flag))
    {
        if (!back_flag)
        {
            buf2_cnt = read(fd, &buf2[0], sizeof(buf2));
            if (buf2_cnt < sizeof(buf2))
                eof_flag = 1;
            buf2_cnt = strip_uninteresting(&buf2[0],buf2_cnt);
        }
        else
            back_flag  = 0;
        cur_ptr = &buf2[0];
        cur_top = cur_ptr + buf2_cnt;
        cur_base = cur_ptr;
    }
    else
    if (cur_ptr >= cur_top && eof_flag)
        return EOF;
    ret = (int) *cur_ptr;
    if (isupper(ret))
        ret = tolower(ret);
    cur_ptr++;
    return ret;
}
/*
 * Macro Versions to speed these functions up
 */
#define CGET(fd) ((cur_ptr<cur_top)?\
((case_insense && islower(*cur_ptr))?(toupper(*cur_ptr++)):*cur_ptr++):\
cget(fd))
#define CSEEK(fp,pos) (((pos>=0&&((pos+cur_ptr)<cur_top))||\
((pos<0&&((pos+cur_ptr)>=cur_base))))?((cur_ptr+=pos),0):\
(cseek(fd,pos),0))
/*
 * Return the stream position definitively (mostly counting spaces etc.)
 */ 
static int ctell(fd)
int fd;
{
int ret = lseek(fd,0,1) - (cur_top - cur_ptr);
    if (back_flag)
       ret -= sizeof(buf1);
    return ret;
}
/*
 * Move forwards within the stream, or backwards within the buffers.
 *
 * This routine will fail if there has not been at least one read.
 */
static void cseek(fd,pos)
int fd;
int pos;
{
    cur_ptr += pos;
    if (cur_ptr < cur_base)
    {
        if (cur_base == &buf1[0] && buf2_cnt == 0)
            cur_ptr = cur_base;
        else
        {
            back_flag = 1;
            if (cur_base == &buf1[0])
            {
                cur_ptr = &buf2[0] + buf2_cnt + (cur_base - cur_ptr); 
                cur_base = &buf2[0];
                cur_top = cur_base + buf2_cnt;
            }
            else
            {
                cur_ptr = &buf1[0] + buf1_cnt + (cur_base - cur_ptr); 
                cur_base = &buf1[0];
                cur_top = cur_base + buf1_cnt;
            }
            if (cur_ptr < cur_base)
                cur_ptr = cur_base;
        }
    }
    else
    if (cur_ptr >= cur_top)
    {
        if (back_flag)
        {
            back_flag = 0;
            if (cur_base == &buf1[0])
            {
                cur_base = &buf2[0];
                cur_ptr = cur_base + (cur_ptr - cur_top); 
                cur_top = cur_base + buf2_cnt;
            }
            else
            {
                cur_base = &buf1[0];
                cur_ptr = cur_base + (cur_ptr - cur_top); 
                cur_top = cur_base + buf1_cnt;
            }
            if (cur_ptr >= cur_top)
                cur_ptr = cur_top - 1;
        }
        else
        {
        int offset = cur_ptr - cur_top -1;
            if ( cget(fd) != EOF)
                cseek(fd,offset);
        }
    }
#ifdef DEBUG_FULL
    printf("base:%x cur:%x top:%x\n",cur_base, cur_ptr, cur_top);
#endif
    return ;
}
/*
 * Routines to satisfy linkage
 */
off_t eng_seek (channel, offset, flag)
FILE * channel;
long offset;
int flag;
{
 
    if (flag == 1)
    {
        cseek(fileno(channel), offset);
        return ctell(fileno(channel));
    }
    else
    if (flag == 0)
    {
        if (offset == 0)
        {
            cclear();
            return lseek(fileno(channel), 0, 0);
        }
        cseek(fileno(channel), offset - ctell(fileno(channel)));
        return ctell(fileno(channel));
    }
    else
    {
        cclear();
        return lseek(fileno(channel), offset, 2);
    }
}
off_t eng_tell(channel)
FILE * channel;
{
    return ctell(fileno(channel));
}
int eng_close(channel)
FILE * channel;
{
    cclear();
    return fclose(channel);
}
/****************************************************************************
 * Read in the next word
 * - The scan control must already have been set up.
 */
struct word_results * eng_get_word(op)
struct open_results * op;
{
double num_val;
int ret;
struct matches * mp;
char * x;                      /* for where strtod() gets to */
static struct word_results xwr;
static char parm_buf[255]; /* maximum size of single lexical element */
char * parm = parm_buf;
char * bound = &parm_buf[sizeof(parm_buf) - 1];

    xwr.word_type = TEXTWORD;
    xwr.word_ptr = parm_buf;

    for (;;)
    {
again:
        for ( *parm = ret = CGET(op->doc_fd);
                  parm < bound
               && ret != EOF
               && (*parm <= ' ' || *parm > (char) 126);
                  *parm = ret = CGET(op->doc_fd));
              /* skip over white space */
        if (ret == EOF)
            return NULL;
        while(ret != EOF && *parm != '\0')
        {
            switch (*parm)
            {                               /* look at the character */
            case ';':
            case ',':
            case '{':
            case '[':
            case '(':
            case '}':
            case ']':
            case ')':
            case '+':
            case '$':
            case '-':
            case '*':
            case '/':
            case '%':
            case ':':
            case '?':
            case '!':
            case '|':
            case '&':
            case '=':
            case '<':
            case '>':
            case '"':
            case '\\':
            case '`':
            case '\'':
/*
 * Skip punctuation
 */
                goto again;
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
            {
/*
 * First pass; collect likely numeric characters
 */
                for ( parm++, *parm = ret = CGET(op->doc_fd);
                          ret != EOF && parm < bound
                        && (  *parm == '.' || ( *parm >='0' && *parm <= '9'));
                               parm++, *parm = ret = CGET(op->doc_fd));
                if (ret != EOF)
                    cseek(op->doc_fd, -1);
                *parm = '\0';
/*
 * Now apply a numeric validation to it
 */
                num_val = strtod( parm_buf, &x);
                if (x == parm_buf)
                {
                    cseek(op->doc_fd, (x - parm) + 1);
                    parm = parm_buf;
                    goto again; /* '.' by itself */
                }
                else
                if (x != parm)
                    cseek(op->doc_fd, (x - parm));
                x = parm_buf + sprintf(parm_buf, "%.23g", num_val);
                goto bottom;
            }
            default:
/*
 * Treat everything as a word to index
 */
                for (;
                      ret != EOF && parm < bound && isalnum (*parm) ;
                         *parm = islower(*parm) ? toupper(*parm) : *parm,
                         parm++,
                         *parm = ret = CGET(op->doc_fd));
                if (parm <= parm_buf + 1 || parm > parm_buf + 64)
                    goto again;
                cseek(op->doc_fd, -1);
                *parm = '\0';
                x = parm;
                goto bottom;
            }
        }
    }
bottom:
    xwr.word_length = (int) (x - parm_buf);
    return &xwr;
}
