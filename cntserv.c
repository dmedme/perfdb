/*
 * cntserv.c - Routines that do useful things to counted strings
 */
static char *sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1998";
#include <stdio.h>
#include <stdlib.h>
#ifdef ULTRIX
#define __inline
#endif
#ifdef AIX
#define __inline
#endif
#ifdef PTX
#define __inline
#endif
#ifdef ICL
#define __inline
#endif
#ifdef SOLAR
#define __inline
#endif
/*
 * Routine to compare two sequences of bytes, each consisting of a 1 byte
 * length, and then the characters to compare.
 * - Returns: 1, first greater than second
 *            0, the two are equal
 *            -1, the first is less than the second
 * - In addition, a pointer value is filled in with the number of characters
 *   in common.
 */
__inline int cntmatstrcmp(x1_ptr, x2_ptr, x3_ptr)
unsigned char * x1_ptr;
unsigned char * x2_ptr;
unsigned char * x3_ptr;
{
int switch_flag;
unsigned char i;
unsigned char * t_ptr;

    i =  *x2_ptr++;

    if (i == *x1_ptr)
    {                     /* Same length */
        switch_flag = 0;
        x1_ptr++;
    } 
    else
    if ( *x1_ptr < i)
    {                     /* First record shorter */
        i = *x1_ptr++;
        switch_flag = -1;
    }
    else
    {                     /* Second record shorter */
        switch_flag = 1;
        x1_ptr++;
    }
    if (i || switch_flag)
    {                     /* If this is not an empty one */
        for (t_ptr = x1_ptr + i ;
                (x1_ptr < t_ptr) && (*x1_ptr == *x2_ptr);
                      x1_ptr++, x2_ptr++);
                                  /* Find where they differ */
        if (x3_ptr != (unsigned char *) NULL)
            *x3_ptr = (unsigned char) (i - (t_ptr - x1_ptr));
        if (x1_ptr >= t_ptr)
            return switch_flag;
        else
        if (*x1_ptr > *x2_ptr)
            return 1;
        else
            return -1;
    }
    else
    {
        if (x3_ptr != (unsigned char *) NULL)
            *x3_ptr = (unsigned char) 0;
        return switch_flag;
    }
}
#ifndef VCC2003
__inline
#endif
unsigned char cntmatch(x1_ptr, x2_ptr)
unsigned char * x1_ptr;
unsigned char * x2_ptr;
{
unsigned char i;
unsigned char * t_ptr;

    i =  *x2_ptr++;

    if (i == *x1_ptr)
    {                     /* Same length */
        x1_ptr++;
    } 
    else
    if ( *x1_ptr < i)
    {                     /* First record shorter */
        i = *x1_ptr++;
    }
    else
    {                     /* Second record shorter */
        x1_ptr++;
    }
    for (t_ptr = x1_ptr + i ;
            (x1_ptr < t_ptr) && (*x1_ptr == *x2_ptr);
                  x1_ptr++, x2_ptr++);
                                  /* Find where they differ */
    return (unsigned char) (i - (t_ptr - x1_ptr));
}
__inline int cntstrcmp(x1_ptr, x2_ptr)
unsigned char * x1_ptr;
unsigned char * x2_ptr;
{
int switch_flag;
unsigned char i, j;
unsigned char * t_ptr;

    i =  *x2_ptr++;
    j =  *x1_ptr++;

    switch_flag =  (i > j) ? -1 : !(i == j);
    for ( t_ptr = x1_ptr + ((i < j) ? i : j) ;
            (x1_ptr < t_ptr) && (*x1_ptr == *x2_ptr);
                  x1_ptr++, x2_ptr++);
                              /* Find where they differ */
    return (x1_ptr >= t_ptr) ? switch_flag : ((*x1_ptr > *x2_ptr) ? 1 : -1);
}
#ifdef OSF
__inline int cstrcmp(x, y)
const void * x, *y;
{
unsigned char ** x1_ptr = x;
unsigned char ** x2_ptr = y;
#else
__inline int cstrcmp(x1_ptr, x2_ptr)
unsigned char ** x1_ptr;
unsigned char ** x2_ptr;
{
#endif
    return cntstrcmp(*x1_ptr,*x2_ptr);
}
/*******************************************************************************
 * Binary Search Lookup routine for sub-strings.
 *
 * Arguments:
 * - Pointer to list of pointers in ascending alphabetical order. The format
 *   is a 1 byte length followed by the thing itself, so the the things need
 *   not be strings.
 * - Number of such pointers
 * - Pointer to substring to match
 * - Length of substring to match
 *
 * Returns:
 * - Lowest Matching Index, or -1 if not found.
 */
__inline int find_first(word_list, high, match_word, match_len)
unsigned char ** word_list;
unsigned char ** high;
unsigned char * match_word;
int match_len;
{
unsigned char ** low=word_list;
unsigned char **guess;
int seen;
int match_dir;
int test_len;
    seen = -1;
    while (low <= high)
    {
        guess = low + ((high - low) >> 1);
        test_len = ((unsigned int) **guess);
        if (test_len > match_len)
            test_len = match_len;
        match_dir = memcmp((*guess)+1, match_word,test_len);
        if (match_dir > 0)
            high = guess - 1;
        else
        if (match_dir == 0 && test_len == match_len)
        {
/*
 * If we have found a match, search back to find the earliest
 */
            seen = guess - word_list;
            high = guess - 1;
        }
        else
            low = guess + 1;
    }
    return seen;
}
#ifndef VCC2003
__inline
#endif
unsigned char** find_any(low, high, match_word, match_len)
unsigned char ** low;
unsigned char ** high;
unsigned char * match_word;
int match_len;
{
unsigned char **guess;
register unsigned char*x1_ptr, *x2_ptr, *t_ptr;
int test_len;

    while (low <= high)
    {
        guess = low + ((high - low) >> 1);
        test_len = ((unsigned int) **guess);
        if (test_len > match_len)
            test_len = match_len;
        for (x1_ptr = (*guess) +1,
             t_ptr = x1_ptr + test_len,
             x2_ptr = match_word;
                   (x1_ptr < t_ptr) && (*x1_ptr == *x2_ptr);
                         x1_ptr++,
                         x2_ptr++);
        if (x1_ptr >= t_ptr)
        {
            if ( test_len == match_len)
                return guess;
            else
                low = guess + 1;
        }
        else
        if (*x1_ptr > *x2_ptr)
            high = guess - 1;
        else
            low = guess + 1;
    }
    return (unsigned char **) NULL;
}
/*
 * Use QuickSort to sort an array of character pointers. This implementation
 * is supposed to be very quick.
 */
void qwork();
/*
 * Function to choose the mid value for Quick Sort. Because it is so beneficial
 * to get a good value, the number of values checked depends on the number of
 * elements to be sorted. If the values are already ordered, we do not return a
 * mid value.
 */
static char * pick_mid(a1, cnt, cmpfn)
char **a1;
int cnt;
int (*cmpfn)();
{
char **top; 
char **bot; 
char **cur; 
char * samples[31];          /* Where we are going to put samples */
int i, j;

    bot = a1;
    cur = a1 + 1;
    top = a1 + cnt - 1;
/*
 * Check for all in order
 */
    while (bot < top && cmpfn(*bot, *cur) < 1)
    {
        bot++;
        cur++;
    }
    if (cur > top)
        return (char *) NULL;
                       /* They are all the same, or they are in order */
/*
 * Decide how many we are going to sample.
 */
    if (cnt < 8)
        return *cur;
    samples[0] = *cur;   /* Ensures that we do not pick the highest value */
    if (cnt < (2 << 5))
        i = 3;
    else 
    if (cnt < (2 << 7))
        i = 5;
    else 
    if (cnt < (2 << 9))
        i = 7;
    else 
    if (cnt < (2 << 11))
        i = 11;
    else 
    if (cnt < (2 << 15))
        i = 19;
    else 
        i = 31;
    j = cnt/i;
    cnt = i;
    for (bot = &samples[cnt - 1]; bot > &samples[0]; top -= j)
        *bot-- = *top;
    qwork(bot, cnt, cmpfn);
    top = &samples[cnt - 1];
    cur = &samples[cnt/2];
/*
 * Make sure we do not return the highest value
 */
    while(cur > bot && !cmpfn(*cur, *top))
        cur--;
    return *cur;
}
/*
 * Use QuickSort to sort an array of character pointers. This implementation
 * is supposed to be very quick (and it is).
 */
void qwork(a1, cnt, cmpfn)
char **a1;
int cnt;
int (*cmpfn)();
{
char * mid;
char * swp;
char **top; 
char **bot; 

    if ((mid = pick_mid(a1, cnt, cmpfn)) == (char *) NULL)
        return;
    bot = a1;
    top = a1 + cnt - 1;
    while (bot < top)
    {
        while (bot < top && cmpfn(*bot, mid) < 1)
            bot++;
        if (bot >= top)
            break;
        while (bot < top && cmpfn(*top, mid) > 0)
            top--;
        if (bot >= top)
            break;
        swp = *bot;
        *bot++ = *top;
        *top-- = swp;
    }
    if (bot == top && cmpfn(*bot, mid) < 1)
        bot++;
    qwork(a1,bot - a1, cmpfn);
    qwork(bot, cnt - (bot - a1), cmpfn);
    return;
}
/*
 * Routine to sort a list of pointers to words consisting of a one byte
 * length followed by the string, in such a way that duplicates are discarded.
 *
 * It returns the pointer to the start of the non-duplicate pointers 
 */
unsigned char ** nodupsort(els, sort_area)
int els;
unsigned char ** sort_area;
{
unsigned char ** sort_top,
   ** left_edge,
   ** next_sorted;

    qwork(sort_area, els, cntstrcmp);
    sort_top = sort_area + els;
/*
 * Re-write excluding duplicates
 */
    for (next_sorted = sort_top - 1, left_edge = sort_top - 2;
             left_edge >= sort_area;
                 left_edge--)
    {
        if ( cntstrcmp(*left_edge, *next_sorted))
        {
            next_sorted--;
            *next_sorted = *left_edge;
        }
    }
    return next_sorted;
}
