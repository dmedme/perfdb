/*
 * IRreghunt.c - function that hunts for the words that match the regular
 * expressions, and adds them to the search node structure.
 *
 * This program depends on the implementation of the stdio library for
 * fread and fseek. It runs much faster by only doing fread and fseek when
 * a new block is needed; otherwise it accesses the fields in the _iobuf
 * structure directly.
 */
static char * sccs_id = "@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1998";

#include <stdio.h>
#include <stdlib.h>
#ifndef LCC
#ifndef VCC2003
#include <unistd.h>
#endif
#endif
#include "IRsearch.h"
#include "IRfiles.h"
#include "orlex.h"
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#ifndef LINUX
#ifndef SOL10
#define KNOW_STDIO_INT
#endif
#endif
static char buf[BUFSIZ];
/*
 * Data to set up the document scan
 */
static struct scan_con * curr_search;
static int words_seen;
static int words_len;
struct word_con {
    unsigned char * word;
    struct word_con * next_word;
    struct comp_reg_con * this_reg;
} * wanchor;
static long int rad[256];
/*
 * Ready to remember the interesting words
 */
static void token_reset()
{
    if (curr_search != (struct scan_con *) NULL)
        scan_discard(curr_search);
    else
    {     /* First time through - load the radix index */
    FILE * rad_chan;
    int i;
        if ((rad_chan = fopen(docrad, "rb")) == (FILE *) NULL)
        {
            for (i = 0; i < 256; i++)
                rad[i] = -1;
        }
        else
        {
            setbuf(rad_chan, NULL);
            if (fread(&rad[0], sizeof(char), sizeof(rad), rad_chan) <= 0)
            {
                for (i = 0; i < 256; i++)
                    rad[i] = -1;
            }
            fclose(rad_chan);
        }
    }
    words_seen = 0;
    words_len = 0;
    wanchor = (struct word_con *) NULL;
    return;
}
/*
 * Squirrel away an interesting word
 */
static int squirrel(len, wp, this_reg)
int len;
unsigned char * wp;
struct comp_reg_con * this_reg;
{
struct word_con * x = (struct word_con *)
               malloc(sizeof(struct word_con) + len + 2);
    if (x == (struct word_con *) NULL)
        return 0; 
    words_seen++;
    words_len += len + 2;
    x->word = ((unsigned char *) x) + sizeof(struct word_con);
    *(x->word) = (unsigned char) len;
    memcpy((x->word + 1), wp, len);
    *(x->word + len + 1) = (unsigned char) '\0';
    x->this_reg = this_reg;
    x->next_word = wanchor;
    wanchor = x;
    return 1;
}
/*
 * Initialise the lexical scanner
 */
static struct scan_con * token_init()
{
struct scan_con * scan_con;
unsigned char * x1_ptr;
unsigned int work_space_size;
int i;
struct word_con * wp;
unsigned char  **curr_sort;

    work_space_size = words_seen * sizeof(unsigned char *) + words_len + 8;
/*
 * Find the size of the word list, and malloc space for 1 pointer
 * per word, and 1 byte length plus the length of each word, and for
 * an estimate of the output size.
 *
 */
    if ((scan_con = (struct scan_con *) malloc(sizeof(struct scan_con)))
                       == (struct scan_con *) NULL)
        return (struct scan_con *) NULL;
    if ((scan_con->in_words = (unsigned char **) malloc(work_space_size))
                  == (unsigned char **) NULL)
    {
        free(scan_con);
        return (struct scan_con *) NULL;
    }
    x1_ptr = (unsigned char *) scan_con->in_words;
    scan_con->sort_area = scan_con->in_words +
                            (words_len + 8)/sizeof(unsigned char *);
    curr_sort = scan_con->sort_area;
/******************************************************************************
 * Loop - get_words until EOF, adding the lengths and words to one buffer,
 *        and pointers to their starts to the sort area
 */
    for (wp = wanchor; wp != (struct word_con *) NULL; )
    {                             /* Loop until all words done */
        *curr_sort++ = x1_ptr;
        memcpy(x1_ptr, wp->word, ((unsigned int) *(wp->word)) + 2);
        x1_ptr += *x1_ptr + 2;
        wp = wp->next_word;
    }
    scan_con->words_seen = words_seen;
    if (x1_ptr == (unsigned char *) scan_con->in_words
     || !scan_compile(scan_con))
    {                          /* if the file has no alphabetical words (???) */
        free(scan_con->in_words);
        free(scan_con);        /* Release the space we got at the beginning */
        return (struct scan_con *) NULL;
    }
    else
    {
/*
 * Mark the regular expressions with the words they match. 
 *
 * The words in the word_con chain are in reverse alphabetical order, hence
 * the descending loop.
 *
 * There should not be any duplicated words in the list.
 */
        for (i = scan_con->words_seen, wp = wanchor; i > 0; )
        {
            i--;
            if (wp->this_reg->match_ind > i)
                wp->this_reg->match_ind = -2; /* More than one word matches */
            else
            if (wp->this_reg->match_ind != -2)
                wp->this_reg->match_ind = i;
            wanchor = wp;
            wp = wp->next_word;
            free(wanchor);
        }
        if (wp != (struct word_con *) NULL)
            fprintf(stderr, "Logic Error: word count %d but extra word %s\n",
                scan_con->words_seen, wp->word);
        else
            wanchor = (struct word_con *) NULL;
        scan_reset( scan_con );
        return scan_con;
    }
}
/******************************************************************************
 * Function to dump out a document list
 */
void dump_doc_list(fp, narr, cnt, dp)
FILE * fp;
char * narr;
unsigned int cnt;
doc_id * dp;
{
int i;
    fputs(narr, fp);
    for (i = cnt; i; i--, dp++)
        fprintf(fp, "   %u\n", *dp);
    return;
}
/******************************************************************************
 * Reverse bytes in situ
 */
void other_end(inp, cnt)
char * inp;
int cnt;
{
char * outp = inp + cnt -1;
     for (cnt = cnt/2; cnt > 0; cnt--)
     {
         *outp = *outp ^ *inp;
         *inp = *inp ^ *outp;
         *outp = *inp ^ *outp;
         outp--;
         inp++;
     }
     return; 
}
/******************************************************************************
 * Function to write out a document list with the numbers reversed.
 */
void reverse_doc_list(fp, narr, cnt, dp)
FILE * fp;
char * narr;
unsigned int cnt;
doc_id * dp;
{
int i;
struct DOCINDheader header;
register doc_id * rdp;
    header.doc_count = cnt;
    header.word_length = strlen(narr);
    for (i = cnt, rdp=dp; i; i--, rdp++)
        other_end((char *) rdp, sizeof(doc_id));
    fwrite((char *) &header, sizeof(char), sizeof(header), fp);
    fwrite(narr, sizeof(char), header.word_length+1, fp);
    fwrite((char *) dp, sizeof(char), header.doc_count*sizeof(doc_id), fp);
    return;
}
/******************************************************************************
 * Function to iterate over an index file, doing something to each word
 * Returns an index status:
 * -1 - failed to open the index
 * 0  - Normal
 * 1  - Wrong-endian for this computer
 * >1 - some kind of error condition
 */
int ind_iterate(indexfile, fp, ofunc)
char * indexfile;
FILE * fp;
void (*ofunc)();
{
struct DOCINDheader header;
register FILE * inputstream;
register char * curr_word;
char * word_read;
int list_size;
int rev_flag = -1; 
char * buff_end;
#ifdef KNOW_STDIO_INT
int i;
#endif
/*
 * Initialise - open the index file
 */
    if ((inputstream = fopen(indexfile,"rb"))==NULL)
        return rev_flag;
    setbuf(inputstream,buf);
/*
 * Allocate a buffer to take the words; allocate another if it fills up
 */
    if ((word_read = (char *) malloc(4096)) == NULL )
    {
        fclose(inputstream);
        return rev_flag;
    }
    bin_root = mem_note(bin_root, (char *) word_read);
    buff_end = word_read + 4095;
    if ( fread(&header,sizeof(header),1,inputstream) <= 0)
    {
        fclose(inputstream);
        return 0;                         /* Exit if file is empty */
    }
/*
 * Loop - process words until EOF
 */
    for (;;)
    {
    doc_id * doc_id_list;       /* Position to hold doc id's read in */

        header.word_length++;
        if ((word_read + header.word_length) > buff_end)
        {
/*
 * Allocate a buffer to take the words; allocate another if it fills up
 */
            if ((word_read = (char *) malloc(4096)) == NULL )
            {
                rev_flag += 4;
                break ;
            }
            bin_root = mem_note(bin_root, word_read);
            buff_end = word_read + 4095;
        }
#ifdef KNOW_STDIO_INT
        if (header.word_length > inputstream->_cnt)
            curr_word = inputstream->_ptr;
        else
#endif
        {
/*
 * Exit on EOF when it shouldn't find it
 */
            if ( fread(word_read,sizeof (char),header.word_length,
                        inputstream) <= 0)
            {
                rev_flag += 8;
                break;
            }
            curr_word = word_read;
        }
/*
 * Guess if the file is the right endian-ness
 */
        if (rev_flag)
        {
            if (rev_flag == -1)
            {
                list_size = header.doc_count;
                other_end((char *) &list_size, sizeof(doc_id));
                if ( header.doc_count > list_size)
                {
                     header.doc_count = list_size;
                     rev_flag  = 1;
                }
                else
                     rev_flag = 0;
            }
            else
                other_end((char *) &(header.doc_count), sizeof(doc_id));
        }
        list_size = header.doc_count * sizeof(doc_id);
                                    /* Size of the list of document pointers */
        if ((doc_id_list = (doc_id *) malloc(list_size))
                    == NULL)
        {
            rev_flag += 2;
            break;
        }
        if (curr_word != word_read)
        {
/*
 * Exit on EOF when it shouldn't find it
 */
            if ( fread(word_read,sizeof (char),header.word_length,
                          inputstream) <= 0)
            {
                 rev_flag += 16;
                 break;
            }
            curr_word = word_read;
        }
/*
 * Read the pointers into the malloc'ed space.
 */
        if (fread(doc_id_list,sizeof(char),list_size,inputstream)
                     != list_size)
        {
             rev_flag += 32;
             break;
        }
/*
 * Output the matched word and its corresponding document ID's
 */
        if (ofunc != NULL)
            (*ofunc)(fp, curr_word, header.doc_count, (doc_id *) doc_id_list);
        free((char *) doc_id_list);
        word_read += header.word_length;
#ifdef KNOW_STDIO_INT
        if (inputstream->_cnt < sizeof(header))
#endif
        {
           if ( fread(&header,sizeof(header),1,inputstream) <= 0)
               break;           /* Exit on EOF */
        }
#ifdef KNOW_STDIO_INT
        else
        {
            memcpy((char *) &header, inputstream->_ptr,sizeof(header));
            inputstream->_ptr += sizeof(header);
            inputstream->_cnt -= sizeof(header);
        }
#endif
    }    /* End of Infinite for loop */
    mem_forget(bin_root);
    bin_root = (struct mem_con *) NULL;
    fclose(inputstream);
    return rev_flag;
}
/******************************************************************************
 * Function to dump out the index in a human readable format
 */
void inddump(indexfile)
char * indexfile;
{
    (void) ind_iterate(indexfile, stdout, dump_doc_list);
    return;
}
/*
 * For sorting a list of comp_reg_con structures
 */
static int comp_reg_cmp(x1, x2)
struct comp_reg_con *x1;
struct comp_reg_con *x2;
{
    return strcmp((x1)->reg_ptr, (x2)->reg_ptr);
}
/*
 * Function to match the words and the documents that they appear in.
 */
int reghunt(indexfile)
char * indexfile;
{
struct comp_reg_con *next_reg, **reg_list, **xnorm, **xwild, **xreg1, **xreg2,
    **xreg3;
int norm_cnt;
int wild_cnt;
struct DOCINDheader header;
struct search_node * word_node;
register FILE * inputstream;
register char * curr_word;
char * word_read;
int match_found;
register int list_size;
unsigned int i, j;
char * buff_end;
unsigned char rad_char;
long int fast_skip[256];
/*
 * Initialise - open the index file
 */
    if ((inputstream = fopen(indexfile,"rb"))==NULL)
        return (NOSUCHFILE);
    setbuf(inputstream,buf);
    token_reset();
/*
 * Initialise the skip control structure
 */
    memset(&fast_skip[0], 0, sizeof(fast_skip));
/*
 * Cut it down to the characters that are known
 *
 * This code knows about compiled regular expressions!? If the first character
 * is 0x02, then the next character is the first letter. Anything else, and the
 * assumption is that all characters satisfy (because we do not check character
 * classes, etc. etc.).
 */
    for (norm_cnt = 0,
         wild_cnt = 0,
         next_reg=reg_start;
             next_reg != NULL;
                 next_reg=next_reg->next_reg_ptr)
    {
#ifdef DEBUG
        gen_handle(stderr, (char *) next_reg->reg_ptr,
          (char *) next_reg->reg_ptr + strlen(((char *) next_reg->reg_ptr)), 1);
#endif
        if (*((char *) next_reg->reg_ptr) == 2)
        {
            fast_skip[ (*(((char *) next_reg->reg_ptr) + 1))]++;
            norm_cnt++;
        }
        else
            wild_cnt++;
    }
    if (norm_cnt && ((xnorm = (struct comp_reg_con **)
                   malloc(norm_cnt*sizeof(struct comp_reg_con *)))
                     == (struct comp_reg_con **) NULL))
    {
        printf("Failed to allocate norm_cnt pointers: %u\n",
                      norm_cnt * sizeof(struct comp_reg_con *));
        return (OUTOFMEM);
    }
    if (wild_cnt && ((xwild = (struct comp_reg_con **)
                   malloc(wild_cnt*sizeof(struct comp_reg_con *)))
                     == (struct comp_reg_con **) NULL))
    {
        if (norm_cnt)
            free(xnorm);
        fclose(inputstream);
        printf("Failed to allocate wild_cnt pointers: %u\n",
                      wild_cnt * sizeof(struct comp_reg_con *));
        return (OUTOFMEM);
    }
/*
 * Record the items in the separate lists
 */
    for (xreg1 = xwild,
         xreg2 = xnorm,
         next_reg=reg_start;
             next_reg != NULL;
                 next_reg=next_reg->next_reg_ptr)
    {
        if (*((char *) next_reg->reg_ptr) == 2)
            *xreg2++ = next_reg;
        else
            *xreg1++ = next_reg;
    }
/*
 * Sort the non-wild-card ones
 */
    if (norm_cnt > 1)
        qwork(xnorm, norm_cnt, comp_reg_cmp);
/*
 * Now work out how many pointers we are going to have to allocate.
 */
    for (i = 0, list_size = 1; i < 256; i++)
    {
        if (rad[i] != -1 && (wild_cnt || fast_skip[i] > 0))
            list_size += 3 + wild_cnt + fast_skip[i];
    }
    if ((reg_list = (struct comp_reg_con **)
                   malloc(list_size*sizeof(struct comp_reg_con *)))
                     == (struct comp_reg_con **) NULL)
    {
        if (norm_cnt)
            free(xnorm);
        if (wild_cnt)
            free(xwild);
        fclose(inputstream);
        printf("Failed to allocate list_size pointers: %d\n",
                      list_size * sizeof(struct comp_reg_con *));
        return (OUTOFMEM);
    }
/*
 * Now build the structure, which consists of repeating groups of:
 * - Numbers of things to do
 * - Seek locations
 * - Pointers to the applicable expressions.
 *
 * Terminate the list with a zero counter
 */
    for (i = 0,
         xreg1 = xnorm,
         xreg2 = reg_list;
             i < 256;
                 i++)
    {
         if (rad[i] != -1 && (wild_cnt || fast_skip[i] > 0))
         {
             *xreg2++ = ((struct comp_reg_con *) (wild_cnt + fast_skip[i]));
             *xreg2++ = ((struct comp_reg_con *) (rad[i]));
             *xreg2++ = ((struct comp_reg_con *) (i));
             for (xreg3 = xwild, j = wild_cnt; j > 0; j--)
                 *xreg2++ = *xreg3++;
             for (j = fast_skip[i]; j > 0; j--)
                 *xreg2++ = *xreg1++;
         }
    }
    *xreg2 = ((struct comp_reg_con *) 0);
    if (norm_cnt)
        free(xnorm);
    if (wild_cnt)
        free(xwild);
/*
 * Allocate a buffer to take the words; allocate another if it fills up
 */
    if ((word_read = (char *) malloc(4096)) == NULL )
    {
        fclose(inputstream);
        free(reg_list);
        puts("Failed to allocate 4096 byte buffer");
        return (OUTOFMEM);
    }
    bin_root = mem_note(bin_root, (char *) word_read);
    buff_end = word_read + 4095;
#ifdef USE_MEMORY_MAP
/*
 * Memory map the index
 */
#ifndef PROT_READ
#define PROT_READ 1
char * mmap();
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE 2
#endif
    inputstream->_cnt =  e2getflen(indexfile);
    inputstream->_base = mmap(NULL, inputstream->_cnt, PROT_READ, MAP_PRIVATE,
          fileno(inputstream), 0);
    inputstream->_ptr = inputstream->_base;
    fprintf(stderr,"Entry Memory Map: _base: %u _ptr: %u _cnt: %d\n",
                        inputstream->_base,
                        inputstream->_ptr,
                        inputstream->_cnt);
    fflush(stderr);
#endif
/*
 * Loop - process words until EOF
 */
    for (xreg1 = reg_list; *xreg1 != (struct comp_reg_con *) 0; xreg1 = xreg3)
    {
    doc_id * doc_id_list;       /* Position to hold doc id's read in */

        xreg3 = xreg1 + ((long int) *xreg1) + 3;
        xreg1++;
#ifdef USE_MEMORY_MAP
        inputstream->_cnt = ((inputstream->_ptr + inputstream->_cnt)
                              - inputstream->_base) - ((long int) *xreg1);
        inputstream->_ptr  = inputstream->_base + ((long int) *xreg1);
#else
        fseek(inputstream,((long int) *xreg1),0);
#endif
               /* Position to the next place of interest */
        xreg1++;
        rad_char = ((unsigned char)  *xreg1);
        xreg1++;
#ifdef USE_MEMORY_MAP
        if (sizeof(header) > inputstream->_cnt)
            break;
        memcpy((char *) &header, inputstream->_ptr, sizeof(header));
        inputstream->_ptr += sizeof(header);
        inputstream->_cnt -= sizeof(header);
#else
        if ( fread((char *) &header,sizeof(header),1,inputstream) <= 0)
            break;              /* Exit on EOF (should not happen) */
#endif
        for (;;)
        {
            header.word_length++;
            if ((word_read + header.word_length) > buff_end)
            {
/*
 * Allocate a buffer to take the words; allocate another if it fills up
 */
                if ((word_read = (char *) malloc(4096)) == NULL )
                {
#ifdef USE_MEMORY_MAP
                    munmap(inputstream->_base);
#endif
                    fclose(inputstream);
                    free(reg_list);
                    puts("Failed to allocate second 4096 byte buffer");
                    return (OUTOFMEM);
                }
                bin_root = mem_note(bin_root, word_read);
                buff_end = word_read + 4095;
            }
#ifdef KNOW_STDIO_INT
            if (header.word_length <= inputstream->_cnt)
                curr_word = inputstream->_ptr;
            else
#endif
            {
/*
 * Exit on EOF when it shouldn't find it
 */
#ifdef USE_MEMORY_MAP
                fprintf(stderr,"Exit Memory Map: _base: %u _ptr: %u _cnt: %d\n",
                        inputstream->_base,
                        inputstream->_ptr,
                        inputstream->_cnt);
                fflush(stderr);
                munmap(inputstream->_base);
                fclose(inputstream);
                free(reg_list);
                return (FILECORRUPT);
#endif
                if ( fread(word_read,sizeof (char),header.word_length,
                            inputstream) <= 0)
                {
                    fclose(inputstream);
                    free(reg_list);
                    return (FILECORRUPT);
                }
                curr_word = word_read;
                if (*curr_word != rad_char)
                    break;
            }
            list_size = header.doc_count * sizeof(doc_id);
                                    /* Size of the list of document pointers */
/*
 * Loop through the regular expressions calling IR_exec to see if they
 * match this word
 */
            for (match_found = FALSE, xreg2 = xreg1; xreg2 < xreg3; xreg2++)
            {
                next_reg = *xreg2;
/*
 * If this expression matches a previous match
 * or 
 * If this expression matches the word
 */
                if ((match_found == TRUE
                 && next_reg->dup_reg_ptr != (struct comp_reg_con *) NULL
                 && next_reg->reg_node_ptr->input_node_count <
                    next_reg->dup_reg_ptr->reg_node_ptr->input_node_count)
                || (IR_exec(curr_word, next_reg->reg_ptr) == TRUE))
                {
/*
 * If this is the first match on this word
 */
                    if (match_found == FALSE)
                    {
                        if ((doc_id_list = (doc_id *) malloc(list_size))
                                    == NULL)
                        {
#ifdef USE_MEMORY_MAP
                            munmap(inputstream->_base);
#endif
                            fclose(inputstream);
                            printf("Failed to allocate listsize: %d\n",
                                   list_size);
                            return (OUTOFMEM);
                        }
                        bin_root = mem_note(bin_root, (char *) doc_id_list);
                        if (curr_word != word_read)
                        {
/*
 * Exit on EOF when it shouldn't find it
 */
#ifdef USE_MEMORY_MAP
                            if (header.word_length > inputstream->_cnt)
                            {
                                munmap(inputstream->_base);
                                fclose(inputstream);
                                return (FILECORRUPT);
                            }
                            memcpy(word_read, inputstream->_ptr, 
                                        header.word_length);
                            inputstream->_ptr += header.word_length;
                            inputstream->_cnt -= header.word_length;
#else
                            if ( fread(word_read,sizeof (char),
                                       header.word_length, inputstream) <= 0)
                            {
                                fclose(inputstream);
                                return (FILECORRUPT);
                            }
#endif
                            curr_word = word_read;
                        }
/*
 * Read the pointers into the malloc'ed space.
 */
#ifdef USE_MEMORY_MAP
                        if (list_size > inputstream->_cnt)
                        {
                            munmap(inputstream->_base);
                            fclose(inputstream);
                            return (FILECORRUPT);
                        }
                        memcpy(doc_id_list, inputstream->_ptr, 
                                    list_size);
                        inputstream->_ptr += list_size;
                        inputstream->_cnt -= list_size;
#else
                        if (fread(doc_id_list,sizeof(char),list_size,
                                  inputstream) != list_size)
                        {
                            fclose(inputstream);
                            return (FILECORRUPT);
                        }
#endif
                        match_found = TRUE;
                    }
                    if ( next_reg->dup_reg_ptr == (struct comp_reg_con *) NULL)
                    {
                        if (!squirrel(header.word_length - 1, curr_word,
                                    next_reg))
                        {
                            puts("Failed to save this word");
#ifdef USE_MEMORY_MAP
                            munmap(inputstream->_base);
#endif
                            return (OUTOFMEM);
                        }
                    }
#ifdef DEBUG
/*
 * Output the matched word and its corresponding document ID's
 */
                    dump_doc_list(stdout, curr_word, header.doc_count,
                               (doc_id *) doc_id_list);
#endif
/*
 * For each match, create a search node, point the word pointer to the word
 * matched, and tie the new node to the regular expression
 *
 * Update the word search node so that its output points at the pointers that
 * have just been read in.
 */
                    NEWSEARCH(word_node);    /* create a new word node */
                    if (word_node == NULL)
                    {
#ifdef USE_MEMORY_MAP
                        munmap(inputstream->_base);
#endif
                        fclose(inputstream);
                        return (OUTOFMEM);
                    }
                    word_node->node_type = IR_OR | IR_WORD;
                    word_node->aux_ptr.word_ptr = word_read;
                    word_node->doc_count = header.doc_count;
                    word_node->doc_list  = doc_id_list;
                    if (next_reg->reg_node_ptr->input_node_count++)
                        TOGLINK (word_node,next_reg->reg_node_ptr->input_node);
                    INPLINK(next_reg->reg_node_ptr,word_node);
                }
            }    /* End of regular expression loop */
/*
 * If not found, seek the number of document pointers to get to the next
 * position.
 */
            if (match_found == FALSE)
            {
#ifdef KNOW_STDIO_INT
#define FSEEK(reldistance) {\
         if ((reldistance) > inputstream->_cnt)\
          fseek(inputstream,(reldistance),1);\
         else\
         {\
          inputstream->_ptr += (reldistance);\
          inputstream->_cnt -= (reldistance);\
         }\
}
#else
#define FSEEK(reldistance) fseek(inputstream,(reldistance),1)
#endif
/*
 * Position to the start of the next record
 */
                if (curr_word != word_read)
                    list_size += header.word_length;
                FSEEK (list_size);
            }
            else
                word_read += header.word_length;
#ifdef KNOW_STDIO_INT
            if (inputstream->_cnt < sizeof(header))
#endif
            {
#ifndef USE_MEMORY_MAP
               if ( fread(&header,sizeof(header),1,inputstream) <= 0)
#endif
                   break;           /* Exit on EOF */
            }
#ifdef KNOW_STDIO_INT
            else
            {
                memcpy((char *) &header, inputstream->_ptr,sizeof(header));
                inputstream->_ptr += sizeof(header);
                inputstream->_cnt -= sizeof(header);
            }
                                     /* Do not need to call a function */
#endif
        }    /* End of Infinite for loop */
    }    /* End of Infinite for loop */
    free(reg_list);
    if (words_seen == 0)
        fputs("Logic Error: No words at all!\n", stderr);
    if ((curr_search = token_init()) == (struct scan_con *) NULL)
    {
        fclose(inputstream);
        puts("Failed to process found words");
        return (OUTOFMEM);
    }
    fclose(inputstream);
    return 0;
}
