/*
 *   IRindgen.c - Generation of an index for a document collection.
 *
 *  %W% %G%
 */
static char *sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1998";
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef LCC
#ifndef VCC2003
#include <unistd.h>
#endif
#endif
#include "IRfiles.h"
#include "mergecon.h"
#include "orlex.h"
#ifdef BADSORT
#include "tabdiff.h"
#endif
extern char * index();
extern int errno;

struct DOCIDalloc current_id, delete_id;
                /* the document id allocation bitmaps */
static char buf[BUFSIZ];
static char buf1[BUFSIZ];

#define EOF_CHANGE 0x1
#define EOF_ACTUAL 0x2
#define HIGH_CHANGE 0x4
#define LOW_CHANGE 0x8
#define MATCH_CHANGE 0x10

/*******************************************************************************
 * Recreate the document name/number index after a failure.
 */
static void recover_docid_map()
{
FILE * actual_channel;
struct DOCMEMrecord  actual_buffer;
unsigned char merge_flag = 0;
/*
 * Clear the document allocation bitmap
 */
    memset((char *) &current_id, 0, sizeof(current_id));
/*
 * Clear the Document ID/Name mapping files
 */
    (void) dbmclose();
    sprintf(buf, "%s.dir", docidname);
    if ((actual_channel = fopen(buf, "wb")) == (FILE *) NULL)
    {
        perror("fopen(docidname.dir)");
        fputs("Cannot truncate list of members index! Aborting\n", stderr);
        return;
    }
    (void) fclose(actual_channel);
    sprintf(buf, "%s.pag", docidname);
    if ((actual_channel = fopen(buf, "wb")) == (FILE *) NULL)
    {
        perror("fopen(docidname.dir)");
        fputs("Cannot truncate list of members data! Aborting\n", stderr);
        return;
    }
    (void) fclose(actual_channel);
    (void) dbminit(docidname);
/*
 * Open the current actual member file for input
 */
    if ((actual_channel = fopen(docmem, "rb")) == (FILE *) NULL)
    {    /* logic error; no actual member file */
        puts("Cannot open list of members! Aborting\n");
        return;
    }
    setbuf(actual_channel,buf);
    READMEM(actual_channel,actual_buffer, EOF_ACTUAL);
/*******************************************************************************
 *     Main Control; loop read in the entries and write out the mapping
 *****************************************************************************
 */
    while  ((merge_flag & EOF_ACTUAL) == 0)
    {
/*
 * Set the bit map entry for this value
 */
        *(current_id.alloc_bitmap + (actual_buffer.this_doc/32))
                       |= 1 << (actual_buffer.this_doc % 32);
/*
 * Store the name/document_id mapping
 */

        doc_id_key.dptr = (char *) &(actual_buffer.this_doc);
        doc_id_key.dsize = sizeof (doc_id);
        doc_name_content.dptr = actual_buffer.fname;
        doc_name_content.dsize = strlen(actual_buffer.fname);
        store (doc_id_key, doc_name_content);
/*
 * Read the next record
 */
        READMEM(actual_channel,actual_buffer, EOF_ACTUAL);
    }
    (void) fclose(actual_channel);
/*
 * Write out the bitmap
 */
    if ((actual_channel = fopen(docidalloc,"wb")) == (FILE *) NULL)
    {    /* logic error; no allocation bitmap */
        perror("Cannot create allocation bitmap");
        return;
    }
    setbuf(actual_channel,buf);
    fwrite(&current_id, sizeof(current_id),1,actual_channel);
                                   /* Write the document id allocation table */
    fclose(actual_channel); /* Close the file */
    return;
}
/*******************************************************************************
 * Main indexing routine
 */
void do_some_indexing(scan_con,fname, con)
struct scan_con * scan_con;
char * fname;
struct sess_con * con;
{
int ret;
FILE * doc_alloc_channel;
/*
 * Re-initialise (for clarity)
 */
    next_buffer = 0;
    no_of_files = 0; 
    last_channel = (FILE *) NULL;
    output_channel = (FILE *) NULL;
    if ((doc_alloc_channel = fopen(docidalloc,"rb")) == (FILE *) NULL)
    {    /* logic error; no allocation bitmap */
        fputs("No allocation bitmap exists! Re-creating...\n", stderr);
        recover_docid_map();
    }
    else
    {
        setbuf(doc_alloc_channel,buf);
        fread(&current_id, sizeof(current_id),1,doc_alloc_channel);
                                /* Read the document id allocation table */
        fclose(doc_alloc_channel);  /* Close the file */
    }
/*
 * Check the index file is OK. Re-write it if it is the wrong way round
 */
    if ((ret = ind_iterate(docind, (FILE *) NULL, NULL)) == 1)
    {
        if ((doc_alloc_channel = fopen (newind, "wb")) == (FILE *) NULL)
        {                                   /* Open the output file       */
            perror("Converting the index endian-ness");
            fputs("Failed to open ", stderr);
            fputs(newind, stderr);
            fputc('\n', stderr);
        }
        else
        {
            setbuf(doc_alloc_channel,buf);
            (void) ind_iterate(docind, doc_alloc_channel, reverse_doc_list);
            fclose(doc_alloc_channel);  /* Close the file */
#ifdef MINGW32
            unlink(docind);
#endif
            rename(newind, docind);
        }
    }
    else
        fprintf(stderr, "Index status check returned %d\n", ret);
/*
 * Find out what we have to do
 */
    if (merge_change_actual(scan_con, fname, con)==EOF)
        return;
/*
 * Combine the remaining word files (which were generated from the new and
 * amended documents in the previous phase) to give a smaller number of files
 * in the "docind" format.
 */
    if (merge_words_official(next_buffer) < 0)
        return;
/*
 * Combine the new words list with the existing list (throwing away existing
 * references if their documents have been deleted)
 */
    if (merge_current() < 0)
        return;
/*
 * Closedown
 */
    if ((doc_alloc_channel = fopen(docidalloc,"wb")) == (FILE *) NULL)
    {    /* logic error; no allocation bitmap */
        perror("Cannot create allocation bitmap");
        return;
    }
    setbuf(doc_alloc_channel,buf);
    fwrite(&current_id, sizeof(current_id),1,doc_alloc_channel);
                                   /* Write the document id allocation table */
    fclose(doc_alloc_channel); /* Close the file */
#ifdef MINGW32
    unlink(docmem);
    tf_zap();                  /* Get rid of all the temporary files */
#endif
    rename(newmem,docmem);     /* Supersede the old list of members */
    return;
}
#ifdef STAND
struct comp_reg_con * reg_start = NULL;
struct mem_con * bin_root = NULL;
extern int optind;
extern char * optarg;
void irdocini(dname)
char * dname;
{
FILE * nfp;

#ifdef MINGW32
#ifdef LCC
    mkdir(dname, 0755);
#else
    mkdir(dname);
#endif
#else
    mkdir(dname, 0755);
#endif
    chdir(dname);
    if ((nfp = fopen(docind, "wb")) != (FILE *) NULL)
        (void) fclose(nfp);
    if ((nfp = fopen(docmem, "wb")) != (FILE *) NULL)
        (void) fclose(nfp);
    if ((nfp = fopen(docind, "wb")) != (FILE *) NULL)
        (void) fclose(nfp);
    if ((nfp = fopen("docidname.dir", "wb")) != (FILE *) NULL)
        (void) fclose(nfp);
    if ((nfp = fopen("docidname.pag", "wb")) != (FILE *) NULL)
        (void) fclose(nfp);
    return;
}
/*******************************************************************************
 * Main Program starts here
 * VVVVVVVVVVVVVVVVVVVVVVVV
 */
main (argc, argv, envp)
int argc;
char * argv[];
char * envp[];
{
/*******************************************************************************
 * On entry, the current working directory is assumed to be the root
 * directory for this document collection. i.e. the private files are in the
 * current working directory.
 *
 * The documents are listed in a file called actual.
 *
 * A directory of documents to index is provided as an argument.
 *
 * Indexes are generated for files that are newer than the last index run.
 *******************************************************************************
 */
int i;
struct scan_con * scan_con;
    if (all_known_flag)
        scan_con =scan_setup(searchterms);  /* Read indexed words */
    else
        scan_con = NULL;
/*
 * Initialise the names of the actual and change directories. These are
 * used as reference points by routines playing with the files
 */
    while ((i = getopt(argc, argv, "i:hd")) != EOF)
    {
        switch (i)
        {
        case 'd':
            inddump(docind);
            break;
        case 'i':
            irdocini(optarg);
            break;
        default:
            fputs("Options:\n-d human-friendly index dump\n-i directory to initialise\nProvide a list of directories to index\n", stderr);
            exit(0);
        }
    }
    if (optind >= argc)
        exit(0);
    dbminit(docidname);         /* Initialise the dbm database */
/*
 * Identify what files need to be updated, and create the word files for them 
 */
    for (i = optind; i < argc; i++)
        do_some_indexing(scan_con,argv[i],(struct sess_con *) NULL);
    exit(0);
} 
#endif
/*
 * Find the next free document ID from where we are in the bitmap
 */
doc_id get_next_docid(current_id, free_id)
struct DOCIDalloc *current_id;
doc_id free_id;
{
register unsigned int * i_ptr;
register unsigned int k;
register unsigned int l;
register unsigned int i;

    i=free_id;
    for (i_ptr = &current_id->alloc_bitmap[0] + i/32,
         l = *i_ptr++,
         k =  1 << (i % 32);
            (i < 0xffffffff);
                i++)
    {    /* This steps through the allocation bitmap */
        if (!(k & l))
        {    /* if this value is free, scarper */
            free_id =i;
            i_ptr--;
            *i_ptr |=k;       /* set the bit in the current bitmap */
            if (current_id->high_alloc < i)
                current_id->high_alloc = i;
            break;
        }
        if (k==(1 << 31))
        {   /* advance to the next bitmap entry */
            l = *i_ptr++;
            k=1;
        }
        else
            k = (k << 1);
    }
    return free_id;
}
/*
 * Function that identifies what this update run needs to do
 */
int merge_change_actual(scan_con, changes, con)
struct scan_con * scan_con;
char * changes;
struct sess_con * con;
{
FILE * change_channel;
FILE * actual_channel;
FILE * new_channel;
unsigned char merge_flag;
doc_id free_id;

struct DOCMEMrecord change_buffer, actual_buffer;
/*
 * Create a list of the files to match in changemem
 */
    if (changes != (char *) NULL)
        change_channel = get_files(changes,changemem);
#ifdef BADSORT
    else
    if (con != (struct sess_con *) NULL)
        change_channel = get_procs(con,changemem);
#endif
    else
        return EOF;
    merge_flag = 0;
/*
 * Get the first record from the change list
 */
    READMEM(change_channel,change_buffer,EOF_CHANGE);

    if (merge_flag & EOF_CHANGE)
        return (EOF);
                     /* No changes, exit without further ado */

    if ((actual_channel = fopen(docmem, "rb")) == (FILE *) NULL)
                     /* Open the current actual member file for input */
    {    /* logic error; no actual member file */
        puts("Cannot open list of members! Aborting\n");
        return EOF;
    }
    setbuf(actual_channel,buf);

    READMEM(actual_channel, actual_buffer, EOF_ACTUAL);
            /* read the first record */

    if ((new_channel = fopen (newmem,"wb")) == (FILE *) NULL)
            /* open the output file */
    {    /* logic error; cannot create output member file */
        puts("Cannot create output list of members! Aborting\n");
        return EOF;
    }
    setbuf(new_channel,buf1);
    free_id = 0;                /* Initialise value for function that finds free
                                   document ids */

/*******************************************************************************
 *     Main Control; loop - merge the new and the existing until both are
 *     exhausted
 ******************************************************************************/
    while  ((merge_flag & (EOF_CHANGE |  EOF_ACTUAL))
            != (EOF_CHANGE | EOF_ACTUAL))
    {
#ifdef DEBUG
        printf("merge_flag: %x change: %s actual: %s\n",
               merge_flag, 
             (merge_flag & EOF_CHANGE)?"":change_buffer.fname,
             (merge_flag & EOF_ACTUAL)?"":actual_buffer.fname);
#endif
        if  ((merge_flag & (EOF_CHANGE |  EOF_ACTUAL)) == 0)
        {    /* If the two streams are still alive */
        int name_match;    /* flag for compare of names */

            if ((name_match = strcmp(actual_buffer.fname,
                          change_buffer.fname)) == 0)
                merge_flag =  MATCH_CHANGE;
            else
                merge_flag = (name_match == 1) ? LOW_CHANGE : HIGH_CHANGE;
        }
        else
            merge_flag &= ~(LOW_CHANGE | HIGH_CHANGE);

        if (merge_flag & (EOF_ACTUAL | LOW_CHANGE))
        {                 /* If the new record is needed, and does not match */
            free_id = get_next_docid(&current_id, free_id);

            change_buffer.this_doc = free_id;

            doc_id_key.dptr = (char *) &free_id;
            doc_id_key.dsize = sizeof (doc_id);
            doc_name_content.dptr = change_buffer.fname;
            doc_name_content.dsize = strlen(change_buffer.fname);
            store (doc_id_key, doc_name_content);
                          /* Add this document to the doc_id/name database */

            create_temp_word(scan_con, &change_buffer);
                          /* decompose this document into words */
            WRITEMEM(new_channel,change_buffer);
                          /* write out the record */
            READMEM(change_channel,change_buffer, EOF_CHANGE);
                    /* read the next record */
        }
        else
        if (merge_flag & (EOF_CHANGE | HIGH_CHANGE))
        {          /* if the old record is needed, and does not match */
            WRITEMEM(new_channel,actual_buffer);
                    /* write out the record */
            READMEM(actual_channel,actual_buffer, EOF_ACTUAL);
                    /* read the next record */
        }
        else /* if (merge_flag == CHANGE_MATCH) */
        {    /* If the records match */
            if (change_buffer.mtime > actual_buffer.mtime)
            {               /* The new one is a replacement for the old */
                *(delete_id.alloc_bitmap + (actual_buffer.this_doc/32))
                       |= 1 << (actual_buffer.this_doc % 32);
                               /* set the bit in the delete bitmap */
                if (change_buffer.fsize)
                {        /* if the new file is non zero in size */
                    change_buffer.this_doc = actual_buffer.this_doc;
                    WRITEMEM(new_channel,change_buffer);
                         /* write out the record */
                    create_temp_word(scan_con, &change_buffer);
                         /* decompose this document into words */
               }
               else
               {
                    doc_id_key.dptr = (char *) &actual_buffer.this_doc;
                    doc_id_key.dsize = sizeof (doc_id);
                    delete (doc_id_key);
                        /* remove this document from the doc_id/name database */
               }
           }
           else
               WRITEMEM(new_channel,actual_buffer);
           READMEM(change_channel,change_buffer, EOF_CHANGE);
                        /* read the next change record */
           READMEM(actual_channel,actual_buffer, EOF_ACTUAL);
                        /* read the next actual record */
       }
    }
    fclose(change_channel);
    fclose(actual_channel);
    fclose(new_channel);
    return 0;
}
