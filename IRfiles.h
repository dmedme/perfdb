/* IRfiles.h - definitions for the Information Retrieval private files
    %W% %G%
*/
#ifndef IRFILES_H
#define IRFILES_H
#ifndef MINGW32
#include <sys/param.h>
#endif
#ifdef POSIX
#ifndef LCC
#ifndef VCC2003
#include <dirent.h>
#endif
#endif
#else
#include <sys/dir.h>
#endif
#include <sys/stat.h>
#ifndef MAXNAMLEN
#ifdef NAME_MAX
#define MAXNAMLEN NAME_MAX
#else
#define MAXNAMLEN FILENAME_MAX
#endif
#endif
#ifdef MINGW32
#define SIGHUP 1
#define SIGQUIT 3
#define SIGBUS 10
#define SIGPIPE 13
/*
 * SIGALRM should be 14, but use SIGBREAK to implement alarm()
 */
#define SIGALRM SIGBREAK
#define SIGUSR1 16
#ifndef MAXPATHLEN
#define MAXPATHLEN MAX_PATH
#endif
#ifndef MAX_PATH
#define MAX_PATH 256
#endif
#endif
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
#ifndef O_BINARY
#define O_BINARY 0
#endif
/******************************************************************************\
*                                                                              *
*     The structure of the Information Retrieval file system                   *
*     ------------------------------------------------------                   *
*                                                                              *
*     There is notionally a root directory, in which the document              *
* collections are defined. The IR Control File is held here.                   *
*                                                                              *
*     Each collection has a sub-directory of this directory to itself.         *
*                                                                              *
*     In this sub-directory are:                                               *
*                                                                              *
*         -    The Information Retrieval-private files for this                *
*             collection.                                                      *
*                                                                              *
*     Directory structures are not significant to IR, which "flattens"         *
* them in its control file.                                                    *
*                                                                              *
*     Deletions are signalled by adding a zero length file to an input         *
* directory with the same name as the document to be deleted.                  *
*                                                                              *
\******************************************************************************/

/* Error codes returned by various modules */

#define OUTOFMEM -1        /* malloc failed; out of space */
#define NOSUCHFILE -2      /* fopen failed; file does not exist */ 
#define FILECORRUPT -3     /* unexpected EOF from read; file layout corrupt */

extern char * ircon; /* = "ircon"; the name of the IR Control file */

extern char * searchterms; /* = "searchterms";  the document collection words */
extern char * docind; /* = "docind";    The document collection word index */
extern char * docmem; /* = "docmem";    The document collection members */
extern char * docrad; /* = "docrad";    The index into the word list */

extern char * changemem; /* ="changemem";  the documents in the changes
                             sub-directory */
extern char * newmem;    /* ="newmem";      the documents in the changes
                   sub-directory merged with the
                   actual files, before renaming. */
extern char * newind; /* = "newind";  The new index output */
extern char * stemp; /* = "stemp";  The search termpory file */
extern char * ccont; /* = "ccont";  The context hunting temporary file */
extern char * docidname; /* = "docidname"; the doc id/name look up dbm database name
                    */
extern char * docidalloc; /* = "docid"; the bitmap giving the allocation of
                    document ids (which ones are in use) */
extern char doccon[];    /* The name of the context hunt control
                            file passed from the query to the browser;
                            name depends on the user */

extern char  doclist[];    /* The list of document ids that can become
                 input to the next query; name depends on the
                    user */

extern char docrep[];    /* The search summary report that is produced
                   by the query processor; name depends on
                    the user */
extern int all_known_flag; /* Whether or not we want to extract all words, or
                              just known words (found in searchterms).  */
       
/**************************************************************************\
*                                                                          *
*  IR Control File                                                         *
*                                                                          *
*     Header Record; configuration information.                            *
*                                                                          *
*     -    Count of all document collections                               *
*     -    Nightly Indexing run status? (for restart information?)         *
*     -    Directories to use for Temporary Disk space                     *
*     -    Root directory for Document Collection private files?           *
*     -    Names for each of the private files (to be added to the         *
*         root)                                                            *
*     -    Summary of Resources consumed?                                  *
*                                                                          *
\**************************************************************************/


/*******************************************************************\
*                                                                   *
*     Data Record (1 per Document Collection)                       *
*                                                                   *
*     -    Document Collection Archive ID                           *
*     -    Directory ID for the IR private files for this           *
*         collection (relative to the root?)                        *
*     -    Access control pointer?                                  *
*     -    Count of Documents in the collection                     *
*     -    collection status                                        *
*     -    Amount of Disk space consumed by this collection         *
*                                                                   *
\*******************************************************************/

/**************************************************************************\
*                                                                          *
*  The Document Collection Member Control                                  *
*                                                                          *
* This file is a IR-private list of the members of a document collection.  *
* Eventually it will contain:                                              *
*                                                                          *
*     -    The document's Archive Identifier                               *
*     -    The language it is in                                           *
*     -    The format (CPT, ASCII, CIP)                                    *
*     -    The internal IR identifier for this document                    *
*     -    The last modified date and time for this document               *
*     -    The date and time that IR indexed this document                 *
*     -    possibly extra data                                             *
*                                                                          *
\**************************************************************************/

typedef unsigned int doc_id;     /*    definition of a document pointer    */

struct DOCMEMrecord {
         doc_id this_doc;
         long mtime;
         unsigned long fsize;
         char fname[MAXPATHLEN];
};
/***************************************************************************\
*                                                                           *
* The include stuff to make dbm "databases" accessible. Primary use is      *
* for the routines openid (open by document ID) and get_word (get a word).  *
*                                                                           *
\***************************************************************************/

typedef struct { char *dptr;
         int dsize; } datum;

extern int     dbminit();

extern     datum fetch();

extern int     store();

extern    int delete();

extern     datum firstkey();

extern     datum nextkey();

    datum doc_id_key, doc_name_content;
        /* stuff to support access to documents by document ID */
#define READMEM(chan,buffer,EOFFLAG) {\
        if (fscanf((chan), "%u %u %u%*c%[^\n]\n", &((buffer).this_doc),\
          &((buffer).mtime), &((buffer).fsize), &((buffer).fname)) != 4)\
         merge_flag |= (EOFFLAG); }

#define WRITEMEM(chan,buffer)  \
      fprintf ((chan), "%u %u %u %s\n", (buffer).this_doc, (buffer).mtime,\
           (buffer).fsize, (buffer).fname);

extern FILE * get_files();    /* function that generates a file like
                   the above from a directory and its
                   sub-directories */
extern FILE * get_procs();    /* function that generates a file like
                   the above from the ORACLE schema. */
extern FILE * get_source();    /* function that generates a file containing
                   procedure source from the ORACLE schema. */

struct DOCIDalloc {
            unsigned int high_alloc;
            unsigned int alloc_bitmap[8192];
           };


/***************************************************************************\
*                                                                           *
*  The Document Collection Index                                            *
*                                                                           *
* This will be the initial implementation. The word scan will cost a large  *
* number of short seeks.                                                    *
*                                                                           *
*     -    flags (e.g. is this a stop word?)                                *
*     -    (attribute identifier (not release 1?))                          *
*     -    count of document occurrences for this word                      *
*     -    length of word plus terminator                                   *
*                                                                           *
\***************************************************************************/

struct DOCINDheader {
/*            unsigned int word_flag;
*/
            unsigned int doc_count;
            unsigned char word_length;

            };

void reverse_doc_list();
/*****************************************************************************\
*                                                                             *
*     -    word (as defined by the tokenisation module)                       *
*     -    null terminator (for the benefit of the library routines).         *
*                                                                             *
*     -    a list of the Document ID's                                        *
*                                                                             *
\*****************************************************************************/

/*****************************************************************************\
*                                                                             *
* The processing of the records in this file involves                         *
*                                                                             *
*     picking up the fixed length record header, which includes the           *
*     word length and the number of documents.                                *
*                                                                             *
*     read the word.                                                          *
*                                                                             *
*     check if the word matches any of the wanted regular expressions         *
*                                                                             *
*                                                                             *
*     if the pointers are wanted, read them into the the program; otherwise,  *
*     seek to the next record.                                                *
*                                                                             *
* see IRreghunt.c                                                             *
*                                                                             *
\*****************************************************************************/

/*****************************************************************************\
*                                                                             *
*  Document Attributes                                                        *
*                                                                             *
* All word recognition modules must return tagged word occurrences; the       *
* tags are the document attributes that are known to IR (and which are common *
* to all document types). These are:                                          *
*                                                                             *
*     -    Title Words                                                        *
*     -    Authors                                                            *
*     -    Key Words                                                          *
*     -    Frame boundaries (following the OS-91 Document Architecture)       *
*         Frames are nested, in general; the term "paragraph" is used         *
*         to collect words that occur in the same frame at the same           *
*         level                                                               *
*     -    Text words.                                                        *
*                                                                             *
* See doctoks.h.                                                              *
*                                                                             *
\*****************************************************************************/


/*  list of attribute values returned by tokenisation
*/

#define TITWORD  1    /* Word embedded in a title */
#define TEXTWORD 2    /* Word embedded in the text */
#define KEYWORD  3    /* Key word entered by the user */
#define AUTHOR   4    /* The name of an author */
#define DATE     5    /* Date last modified */
#define PARA     6    /* Paragraph (Frame) Boundary */
#define DOC_EOF  7      /* End of File */


struct word_results {
    unsigned char * word_ptr;
    int word_type;
    unsigned char word_length;
    int match_ind;
    long start_pos;
}; /* structure returned by tokenisation */

struct open_results {
    FILE * doc_channel;
    int doc_fd;
    char * doc_name;
    unsigned long doc_len;
    struct word_results * (*word_func)();
    off_t (*get_pos_func)();
    off_t (*set_pos_func)();
    int (*close_func)();
}; /* structure returned by openid. The implementation
    is object oriented in so far as the only
    operations allowed (read a token, get the stream
    position and set the stream position)  are passed
    back in the open_results structure. The parameters to
    these calls are stdio-compatible.

    To start with, only one tokenising module, get_word,
    exists. Note the use of sed on the lex output in
    the makefile. */
struct word_results * eng_get_word();
off_t eng_seek();
off_t eng_tell();
int eng_close();

extern struct open_results * openid();
            /* Function that opens documents by document ID */
extern struct open_results * openbyname();
            /* Function that opens documents by document name */

/******************************************************************************\
*                                                                              *
*  Search Document List                                                        *
*                                                                              *
* This is a list of document identifiers in the same format as the word lists, *
* and which can be fed back into a query. It is output by the Search module.   *
*                                                                              *
*     -    Document ID                                                         *
*                                                                              *
\******************************************************************************/

/******************************************************************************\
*                                                                              *
*  Search Context Look for List                                                *
*                                                                              *
* This file is output from Search module, and is used by the browsing modules  *
* to locate (for highlighting) contexts. It is a drive table used by the       *
* context hunting module.                                                      *
*                                                                              *
* It must be possible to move forwards and backwards through this list.        *
*                                                                              *
* The header structure allows the rest of the record to be mapped out. This    *
* applies both when the record is in memory, and when it is on the disk.       *
* The difference (memory or disk) accounts for the unions that appear in it.   *
* When in memory, the pointer values are used; when on disk, the long          *
* integers are used (as offsets for seeks).                                    *
*                                                                              *
\******************************************************************************/
struct DOCCON_cont_header {
            doc_id this_doc;
            union
            {
             unsigned long int record_size;
             struct DOCCON_cont_header * next_cont_header;
            } forward_pos_control;
            union
            {
             unsigned long int prev_record_size;
             struct DOCCON_cont_header * prev_cont_header;
            } backward_pos_control;
                union
            {
             unsigned long int reg_size;
                /* The space (on disk) occupied by the
                   regular expressions for this document.
                 */
             char *doc_name;
                    /* pointer to the place in memory where
                   the document name is: warning; this is
                   a frig for IRbrowse, which wants to
                   open the file itself.
                   (only on re-reading)
                     */
            } reg_control;
            unsigned int reg_con_count;
                /* The count of the regular expressions
                   that are associated with this document.
                   This counts the comp_reg_con structures
                   that are next on the disk after the
                   concatenated compiled regular
                   expressions.
                */
            unsigned int reg_count;
                /* The count of recognisable regular
                   expressions.

                   This count INCLUDES sequences and
                   proxes. However, the proximities
                   are not used, because they cannot
                   be nested. See the code for
                   function cont_hunt.

                  This count sizes

                    - a list of comp_reg_con pointers
                      with nulls corresponding to the
                      positions of seqs and proxes

                    - a bitmap with bits set against
                      the regular expressions that
                      constitute a matching context.
                      prox positions are not set,
                      because a match is always
                      "found", so there is no need
                      to test it.

                 */
            unsigned int prox_count;
                /* this count sizes a list of prox
                    expression counts and regular
                    expression labels (relative to
                    the expression pointer list)
                */
            unsigned int seq_count;
                /* this count sizes a list of seq labels
                   (relative to the start of the regular
                    expression pointer list), with
                    expression counts and regular
                    expression labels (likewise relative to
                    the expression pointer list)
                   the order of entries is

                    - count (includes the count itself, and
                      the seq label)
                    - seq label
                    - regular expression labels    
                */
           };
/*
When set up for the context hunter, the order of structures is as follows:

    -    the header
    -    the comp_reg_con pointers (with nulls)
    -    the regular expression bitmap
    -    the prox list
    -    the seq list

The comp_reg_con structures themselves, and the heap of the compiled
regular expressions, are notionally elsewhere; no assumptions are made as
to contiguity.

After they have been re-read from disk, there is no need for the comp_reg_con
structures to be chained together.
*/

int cont_create();
    /* function to create a context record control file */

int cont_write();
    /* routine to write a context hunter control record to disk */

int cont_close();
    /* function to close the context hunt control file */

struct DOCCON_cont_header * cont_reopen ();
    /* function for opening an existing context hunter control file
       for reading. The function reads the first record */

#define NEXT 1
#define PREVIOUS 2

struct DOCCON_cont_header * cont_read ();
    /* function for reading the next (or the previous) context hunter
       control record */

/******************************************************************************\
*                                                                              *
*  Search Context Found list                                                   *
*                                                                              *
* The Search Context hunter will only work forwards. In order                  *
* to allow users to step backwards through contexts, for each document in the  *
* search list there is created (by the browser) a file that gives              *
*                                                                              *
*     -    The extent through the file that the context hunter                 *
*         has progressed                                                       *
*     -    File offsets and positions for the items that need to               *
*         be displayed highlighted.                                            *
*                                                                              *
\******************************************************************************/

union BROWSE_record
    {
     struct browse_header
     {
      long last_looked;
      long eof_reached;
      long at_eof;

#define EOF_NOT_REACHED  0
#define EOF_REACHED  1
     } header;

     struct ordinary
     {
      long start_pos;
      long end_pos;
     } ordinary;
    };

int browse_unlink ();
    /* function to remove an old browse control file */
int browse_open();
    /* routine to initialise browse control for a document */

union BROWSE_record * browse_change();
    /* function that finds the next or previous context, going from
       the current location. Used to implement get next context and
       get previous context.
         */

union BROWSE_record * browse_identify();
    /* function that provides a pointer to the first context
       in the range first_location to last_location. The calling
       program should call the routine repeatedly until the
       routine returns a null pointer, advancing its first_location
       after each call. */

int browse_close();
    /* function to update the header and close a browse control
       file for a document */
/*
 * Utility functions that work on counted strings
 */
unsigned char ** nodupsort();
int cntstrcmp();
#endif
