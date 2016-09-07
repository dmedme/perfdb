/* IRfilenames.c - hard coded list of the filenames known to the
   Information Retrieval System

    %W%    %G%

*/
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1998";
#include <stdio.h>
#include <stdlib.h>
#ifndef LCC
#ifndef VCC2003
#include <unistd.h>
#endif
#endif
#include "orlex.h"
#include "IRfiles.h"
char * ircon = "ircon";        /* The name of the IR Control file */

char * docind = "docind";    /* The document collection word index */
char * docmem = "docmem";    /* The document collection members */
char * docrad = "docrad";    /* The index into the word list */

char * changes = "changes";    /* The changes sub-directory */
char * actual = "actual";    /* The actual sub-directory */

char * changemem ="changemem";  /* The documents in the changes
                    sub-directory */
char * newmem ="newmem";      /* The documents in the changes
                   sub-directory merged with the
                   actual files, before renaming. */
char * searchterms = "searchterms"; 
                                /* The document collection words */
char * newind = "newind"; /* The new index output */
char * stemp = "stemp"; /* The search temporary file */
char * ccont = "ccont"; /* The context hunting temporary output */
char * docidname = "docidname"; /* The doc id/name look up dbm database name */
char * docidalloc = "docid"; /* The bitmap giving the allocation of
                    document ids (which ones are in use) */
char doccon[MAXNAMLEN];    /* The name of the context hunt control
                   file passed from the query to the browser;
                       name depends on the user */

char  doclist[MAXNAMLEN];  /* The list of document ids that can become
                              input to the next query; name depends on the
                              user */

char docrep[MAXNAMLEN];    /* the search summary report that is produced
                              by the query processor; name depends on
                              the user */
int all_known_flag;        /* Whether or not we want to extract all words, or
                              just known words (found in searchterms).  */
/*
 * Initialisation routine for embedding purposes
 */
void ir_ini()
{
    dbminit(docidname);
    return;
} 
