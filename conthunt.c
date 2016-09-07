/*******************************************************************************
*  Context Hunting        SCCS ID: %W% %G%
*
* Introduction
* ------------
*
* Context hunting is considered in two parts.
*
* The first is the generation of the control structures from the
* Search node tree structure.
*
* The second is finding a location that contains a context in a given
* document; the context hunter.
*
* The context hunter setter-upper is called with the whole search node tree as
* its input after the searches are completed; the structures are passed to
* the browser.
*
* Otherwise, it is called when a SEQ or PROX node is encountered in the
* search evaluation.
*
* The context hunter itself is called once per SEQ and PROX search node,
* and is then called by the browse module when "next context" is requested
* and the document section has not previously been scanned.
*
* The Context Hunter
* ------------------
*
* The input to the context hunter is:
*
* A list of the compiled regular expressions that are known to occur in
* it, in the form of a counted list of pointers to the control structures
* defined in IRsearch.h.
*
* A bit map, with one bit per regular expression in the list, indicating
* simple term matches. If the regular expression corresponding to a bit
* that is set is found, the routine returns successfully.
*
* A counted list of counted lists of elements in sequence, indicating which
* sequences of regular expressions must be matched directly one after the
* other to give Sequential context match.
*
* A counted list of bit maps with bits set for the regular expressions or
* sequences that must match to give an occurrence of a Paragraph context match.
*
* All these are passed together by giving a pointer to a structure through
* which the individual elements can be located.
*
* The address of the routine to call to obtain "words".
*
* A point beyond which it should not scan (go to the next paragraph beyond
* this point).
*
* The context hunter returns either with
*
* An indication that "no more words" was returned by the word fetcher.
*
* Or that the limit had been reached with no matches found, in which case
* the search up to offset is returned.
*
* Or the offset and length of the context that was matched. This is either
* a single word, a sequence of words or a whole paragraph.
*
* Or, an indication of an unexpected error.
*
* The Context Hunter Setter Upper
* -------------------------------
*
* The context hunter setter upper is given
*
* A search node.
*
* The list of documents to look for. This is the output from the final list,
* when processing the final request, but is the output from the previous
* node when setting up for a SEQ or PROX list.
*
* The setter upper recursively follows the input and together chains looking
* for the documents in the list.
*
* Since all the lists are sorted, this is not as painful as it might seem.
*
* For each document, a structure along the lines indicated above is built.
*
* A chain is not followed if it is a NOT chain.
*
* The recursive calls stop when
*
* A node is encountered which has none of the documents being looked for.
*
* The node has a regular expression associated with it. The regular expression
* is added to an appropriate place in the list.
*
* The recursive calls are initially looking for elements to put in the
* list of simple expressions. However, when a PROX node is encountered,
* a PROX list entry is created, and the elements are added to a prox list.
*
* When a Sequence node is encountered, a Sequence bitmap is created; if there
* is a PROX in existence the sequence is added to the PROX list. Regular
* expressions found are set in the Sequence list.
*
* PROX lists and sequences are cloned when OR nodes are encountered.
*
* The output from the context hunter setter upper is generated
* after a pass through whatever it has found, since there is no knowing what
* order things will be found. I believe at this stage that the SEQ regular
* expressions are encountered in the correct sequence, but this needs to be
* tested!
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
#include "conthunt.h"
/*
 * The Data structures managed.
 *
 * -  Structure to track progress
 */
struct reg_found {
    union {
        struct comp_reg_con * wanted_reg;
        unsigned int prox_count;
        unsigned int seq_count;
    } cont_ptr;
    struct reg_found * next_found;
    char found_type;
};
/*
 * The context hunter setter upper takes a list of documents (usually all
 * the documents that satisfied the search condition) and works backwards
 * through the search_node tree in order to find all the regular expressions,
 * regular expression sequences and regular expression proximities (regular
 * expressions that must occur in the same paragraph) that are known to (or may)
 * match somewhere in this document.
 *
 * When one of these entities is recognised, a reg_found structure is chained
 * to an entry for this document (the doc_con structure).
 *
 * The reg_found structure can be one of three types;
 *
 * -   IR_REG
 * -   IR_SEQ
 * -   IR_PROX
 *
 * If it is an IR_REG, it holds a pointer to the appropriate regular expression.
 *
 * If it is an IR_SEQ or an IR_PROX it holds a count of the regular expressions
 * (that follow it in the chain) that constitute the sequence or proximity
 * collection.
 *
 * When the search is completed, the context setter upper follows the chains,
 * and recasts the data into a format better suited to the context hunter.
 *
 * The context hunter works by
 *
 *  -    reading a word from the document
 *  -    comparing the word with each of the regular expressions found
 *       in that document
 *
 *       If the word matches, there are three possibilities;
 *
 *       -    The user wanted to see this word
 *       -    The user wanted to see this word in a sequence
 *       -    The user wanted to see this word (perhaps as
 *            part of a sequence) only if it occurred in
 *            the same paragraph as a number of other
 *            regular expression matches.
 *
 * As can readily be seen, a data organisation that is primarily organised by
 * regular expression is more suitable for this purpose. See below.
 *
 * The doc_con structure provides the anchor for the regular expression 
 * chain established by the find_regs function, and some fields used to manage 
 * the creation of this chain.
 */
struct doc_con {
    doc_id doc_ptr;          /* The document to which the structure relates */
    unsigned int reg_count;  /* The count of regular expressions found (= the
                              * length of the reg_found chain) */
    unsigned int prox_count; /* The count of proximity expressions met */
    unsigned int seq_count;  /* The count of sequential expressions met */
    unsigned int prox_reg_count;
              /* The count of regular expressions that are part of proximities
               * (the context hunter expects for each proximity expression a
               * label, a count of its regular expressions, and then labels for
               * its regular expressions. The space that needs to be reserved is
               * thus 2*prox_count + prox_reg_count words
               * (each label is 2 bytes long))
               */
    unsigned int seq_reg_count;
              /* The count of regular expressions that are part of sequences
               * (the sequence structure is the same
               * as the proximity structure).
               */
/*
 * The rest of the structure consists of control mechanisms for the reg_found
 * chain
 */
    struct reg_found * reg_root;   /* The reg_found anchor */
    struct reg_found * link_point; /* The reg_found element to which
                                    * the next reg_found will be chained
                                    */
    struct reg_found * seq_curr;
              /* If there is a sequence, the reg_found pointer in which the
               * count of expressions in this sequence should be updated.
               */
    struct reg_found * prox_curr;
              /* if there is a proximity, the reg_found pointer in which the
               * count of expressions in this proximity should be updated.
               */
};
/*
 * Structure based on the document list that enables the regular expressions in
 * each document to be tracked
 */
struct doc_list_element {
    doc_id doc_ptr;
    struct doc_con * home_point;
};
/*
 * Structure that is merged with the list of documents for a given call to
 * find_regs()
 */
union reg_ptr {
    struct comp_reg_con * next_reg_con;
    unsigned int  prox_con;
    unsigned int  seq_con;
};
/*
 * Anchor for list of control structures
 */
struct doc_con * doc_con_start;
/*
 * Function to find the regular expressions
 */
static int find_regs(cur_doc_list,cur_doc_count,find_flag,input_node)
struct doc_list_element * cur_doc_list;
unsigned int cur_doc_count;
int find_flag;
struct search_node * input_node;
{
struct doc_list_element * in_list, * out_list, * out_list_base;
unsigned int in_count, cur_count, out_count, i, j;
int ret_val;
doc_id * in_ptr ;
struct search_node * i_search_node;

    in_count = input_node->doc_count;
/*
 *  Allocate space for the list of documents whose regular expressions
 *  are being searched for
 */
    if (( out_list_base =(struct doc_list_element *)
            malloc (((cur_doc_count < in_count)
                  ? cur_doc_count
                  : in_count)
                 * (sizeof(struct doc_list_element)))) == NULL)
        return (OUTOFMEM);                                  /* Allocate space */
    bin_root = mem_note(bin_root, (char *) out_list_base);
/*
 *  Merge the searched for list (in_list) with the output from this
 *  node to give a new list of interesting documents (out_list)
 */
    for (cur_count = cur_doc_count,
         in_ptr = input_node->doc_list,
         in_list = cur_doc_list,
         out_list = out_list_base,
         out_count=0;
             (cur_count > 0) && (in_count > 0);)
    {    /* Loop - merge the elements */
    struct doc_con * home_doc_con;

        if ( *in_ptr == in_list->doc_ptr)
        {            /* Match */
            out_list->doc_ptr = *in_ptr;
            out_count++;
            home_doc_con = in_list->home_point; 
            out_list->home_point = home_doc_con;
/*
 *  If we need a new reg_found node, set it up
 */     
            if (input_node->node_type & (IR_REG | IR_PROX | IR_SEQ))
            {     /* An expression that can be hunted for */
            struct reg_found * new_reg_found;

                if (( new_reg_found = (struct reg_found *)
                        malloc (sizeof (struct reg_found))) == NULL)
                                        /* Allocate space  for a new node */
                    return (OUTOFMEM);
                bin_root = mem_note(bin_root, (char *) new_reg_found);
/*
 *  Link it to the chain for this document
 */
                if (home_doc_con->reg_root == NULL)
                {    /* If this is the first for this document */
                    home_doc_con->reg_root = new_reg_found;
                    home_doc_con->link_point = new_reg_found;
                }
                else
                {    /* Some expressions already found */
                    home_doc_con->link_point->next_found = new_reg_found;
                    home_doc_con->link_point = new_reg_found;
                }
/*
 *  Update the counts of reg_found structures
 */
                home_doc_con->reg_count++; 
                if (find_flag & IR_SEQ)
                {    /* If this is a SEQ expression */
                    home_doc_con->seq_curr->cont_ptr.seq_count++;
                    home_doc_con->seq_reg_count++;
                }
                else
                if (find_flag & IR_PROX)
                {    /* If this is a PROX expression */
                    home_doc_con->prox_curr->cont_ptr.prox_count++;
                    home_doc_con->prox_reg_count++;
                }
/*
 *  Process the node depending on the search node type
 */
                switch (input_node->node_type & (IR_REG | IR_PROX | IR_SEQ))
                {    /* Finish creating the node value */
                case IR_REG:    /* Regular Expression */
                    new_reg_found->found_type = IR_REG;
                    new_reg_found->cont_ptr.wanted_reg =
                                  input_node->aux_ptr.reg_con_ptr;
                    break;
                case IR_PROX:        /* Proximity Operator */
                    new_reg_found->found_type = IR_PROX;
                    new_reg_found->cont_ptr.prox_count = 0;
                    home_doc_con->prox_curr = new_reg_found;
                    home_doc_con->prox_count++;
                    break;
                case IR_SEQ:        /* Sequence Operator  */
                    new_reg_found->found_type = IR_SEQ;
                    new_reg_found->cont_ptr.seq_count = 0;
                    home_doc_con->seq_curr = new_reg_found;
                    home_doc_con->seq_count++;
                    break;
                default:
                    break;
                }
            }
/*
 *  Update the pointers, etc.
 */
            out_list++;
            in_list++;
            in_ptr++;
            in_count--;
            cur_count--;
        }
        else
        if (*in_ptr < in_list->doc_ptr)
        {
            in_ptr ++;
            in_count --;
        } 
        else
        {
            in_list++;
            cur_count--;
        }
    }
/*
 *  If no documents left, or if it was a regular expression, exit.
 */
    if (out_count==0 || (input_node->node_type & IR_REG))
        return (0);
/*
 *  Prepare to process the input nodes to the current node
 *
 *  - Set up the seq-prox flag
 */
    if (input_node->node_type & IR_SEQ)
        find_flag |= IR_SEQ;
    else
    if (input_node->node_type & IR_PROX)
        find_flag |= IR_PROX;
/*
 * - Decide the number of nodes to be processed
 */
    if (input_node->node_type & (IR_SEQ | IR_PROX | IR_NOT))
        j = 1;
    else
        j = input_node->input_node_count;
/*
 * - Find the regular expressions for these nodes; exit if unexpected error
 */
    for (i = j, i_search_node = input_node->input_node;
              i;
                  i--, i_search_node = i_search_node->together_node)
         if (ret_val = find_regs(out_list_base, out_count, find_flag,
                                 i_search_node) < 0)
             return (ret_val);
/*
 * Scarper
 */
    return(0);
}
/*
 * Context Hunter Setter Upper.
 *
 * The return value must be the same size as a pointer on the target hardware,
 * for what seemed to be good reasons at the time.
 */
long int con_set(input_node)
struct search_node * input_node;
{
unsigned int i,j;                    /* Loop counters */
struct doc_con * doc_con_curr;       /* Current document control element */
struct DOCCON_cont_header * doc_cont_header,
                   * doc_cont_anchor,
                   * doc_cont_prev;  /* Context record control structures */
struct doc_list_element * doc_list_curr, * doc_list_start;
                                     /* Current and anchor for the merge list */
doc_id * in_doc_list;
unsigned int in_doc_count;
doc_id * doc_id_ptr;
/*
 *    Initialise
 */
#ifdef DEBUG
    puts("con_set() called");
#endif
    in_doc_list = input_node->doc_list;
    in_doc_count = input_node->doc_count;
    if (!in_doc_count)
        return 0;
    if (( doc_con_start =(struct doc_con *)
            malloc ((in_doc_count * (sizeof(struct doc_con)
                + sizeof(struct doc_list_element))))) == NULL)
                                    /* Allocate space */
        return (OUTOFMEM);
    bin_root = mem_note(bin_root, (char *) doc_con_start);
/*
 * Share the space between the lists; saves a malloc call
 */
    doc_list_start =  (struct doc_list_element *) (doc_con_start +
                    in_doc_count);
/*
 * Loop through the list of input documents, initialising the structures
 */
    for (doc_con_curr = doc_con_start,
         doc_list_curr = doc_list_start,
         doc_id_ptr = in_doc_list,
         i= in_doc_count;
             i;
                 i--,
                 doc_con_curr++,
                 doc_list_curr++,
                 doc_id_ptr++)
    {
        doc_con_curr->doc_ptr = * doc_id_ptr;
        doc_list_curr->doc_ptr = * doc_id_ptr;
        doc_con_curr->reg_count = 0;
        doc_con_curr->prox_count = 0;
        doc_con_curr->seq_count = 0;
        doc_con_curr->prox_reg_count = 0;
        doc_con_curr->seq_reg_count = 0;
        doc_con_curr->reg_root = NULL;
        doc_con_curr->seq_curr = NULL;
        doc_con_curr->prox_curr = NULL;
        doc_con_curr->link_point = NULL;
        doc_list_curr->home_point=doc_con_curr;
    }
/*
 * Main Processing - Find all the regular expressions for the
 * documents in the document list; exit if unexpected error.
 */
    if ((i=find_regs(doc_list_start,in_doc_count,0,input_node)) < 0)
        return (i);
/*
 * Closedown; generate the input to the Context Hunter Document by Document
 * - For each document
 */
    for (doc_con_curr = doc_con_start,
         i = in_doc_count,
         doc_cont_anchor = NULL;
             i;
                 i--,
                 doc_cont_prev = doc_cont_header,
                 doc_con_curr++)
    {
    unsigned int list_size;
    struct reg_found * main_reg;
    unsigned int seq_rem, prox_rem;
    unsigned int * seq_list, * prox_list; 
    int * off_list;
    unsigned int * bit_map_base;
    struct comp_reg_con ** reg_list, **base_list, **wrk_list;
/*
 * Allocate space for
 * -    A header which identifies the document
 * -    A list of regular expression pointers
 * -    The regular expression bit-map
 * -    The prox lists
 * -    The seq lists
 */
        list_size = doc_con_curr->reg_count;

        if (( doc_cont_header =(struct DOCCON_cont_header *)
            malloc (sizeof (struct DOCCON_cont_header) +
            list_size * sizeof (struct comp_reg_con *) +
            list_size * sizeof (int) +
            ((list_size + 31)/32) * sizeof (unsigned int) +
            (doc_con_curr->prox_reg_count +
             doc_con_curr->seq_reg_count  +
              doc_con_curr->prox_count +
              doc_con_curr->seq_count*2)
             * sizeof(unsigned int))) == NULL)
                                                   /* Allocate space */
            return (OUTOFMEM);
        bin_root = mem_note(bin_root, (char *) doc_cont_header);
/*
 * Set up the header; this is read by the browser to enable
 * it to decode the rest of the stuff for this document.
 *
 * Note that the pointers will become offsets within the
 * file. These are fixed when the records are written out
 * to disk.
 */
        doc_cont_header->this_doc = doc_con_curr->doc_ptr;
        doc_cont_header->reg_count = list_size;
        doc_cont_header->reg_control.reg_size = 0; /* not yet known */

        if (doc_cont_anchor == NULL)
            doc_cont_anchor = doc_cont_header;
        else
        {
            doc_cont_header->backward_pos_control.prev_cont_header =
                             doc_cont_prev;
            doc_cont_prev->forward_pos_control.next_cont_header =
                             doc_cont_header;
        }
        doc_cont_header->forward_pos_control.next_cont_header = NULL;
        doc_cont_header->reg_con_count = doc_con_curr->reg_count -
              doc_con_curr->seq_count - doc_con_curr->prox_count;

        doc_cont_header->seq_count = doc_con_curr->seq_reg_count +
                    doc_con_curr->seq_count*2;
        doc_cont_header->prox_count = doc_con_curr->prox_reg_count +
                    doc_con_curr->prox_count;
/*
 * Process the regular expression chain, starting from the doc_con entry for
 * this document.
 * -    Set bits in the regular expression bit mask
 *      if the expression signifies a context to be matched.
 * -    Add to lists when prox and seq nodes are encountered.
 * -    Add the regular expression to the pointer list.
 * -    Update counts (used when the structure is decoded
 *      after having been written out to disk).
 *
 * Loop through all the regular expressions.
 */
        for (main_reg = doc_con_curr->reg_root,
             reg_list = (struct comp_reg_con * *) (doc_cont_header + 1),
             base_list = reg_list,
             bit_map_base = (unsigned int *) (reg_list + list_size),
             prox_list = (unsigned int *) (bit_map_base +
                           (list_size + 31)/32),
             seq_list  = prox_list + doc_cont_header->prox_count,
             off_list  = (int *) (seq_list + doc_cont_header->seq_count),
             seq_rem =0,
             prox_rem = 0,
             j=0;
                 j < list_size;

                     main_reg = main_reg->next_found,
                     j++)
        {
            switch (main_reg->found_type)
            {
            case IR_REG:              /* Ordinary regular expression */
                if (main_reg->cont_ptr.wanted_reg->dup_reg_ptr != 
                        (struct comp_reg_con *) NULL)
                    *reg_list = main_reg->cont_ptr.wanted_reg->dup_reg_ptr;
                else
                    *reg_list = main_reg->cont_ptr.wanted_reg;
                *off_list = -1;
                for (wrk_list = base_list; wrk_list < reg_list; wrk_list++)
                    if (*wrk_list == *reg_list)
                    {
                        *off_list = (wrk_list - base_list);
                        break;
                    }
                reg_list++;
                if (seq_rem || prox_rem)
                    *(bit_map_base + j/32) &=  ~(1 << (j % 32));
                else
                    *(bit_map_base + j/32) |= (1 << (j % 32));
                if (seq_rem)
                {
                    seq_rem--;
                    *seq_list++ = j;
                }
                else
                if (prox_rem)
                {
                    prox_rem--;
                    *prox_list++ = j;
                }
                break;
            case IR_PROX:             /* Proximity expression */
/*
 * Proximities cannot be nested, so do not bother to set the bitmap entry
 */
                prox_rem =  main_reg->cont_ptr.prox_count;
                *reg_list++ = NULL;
                *prox_list++ = prox_rem + 1;
                break;
            case IR_SEQ:               /* Sequence of regular expressions */
                seq_rem =  main_reg->cont_ptr.seq_count;
                *reg_list++ = NULL;
                *seq_list++ = seq_rem+2;
                *seq_list++ = j;
                if (prox_rem)         
                {
                    *(bit_map_base + j/32) &= ~(1 << (j % 32));
                    prox_rem--;
                    *prox_list++ = j;
                }
                else
                    *(bit_map_base + j/32)  |= (1 << (j % 32));
                break;
            }
            off_list++;
        }
    }
    return ((long int) doc_cont_anchor); /* exit O.K. at end of function */
}
/*
 * The incremental search routine; the Context Hunter
 */
struct hunt_results * cont_hunt(max_offset,open_block,hunt_stuff)
int max_offset; /* The offset beyond which the hunter should
                 * not scan; used to look ahead a screenload or
                 * so; must go on to the end of a paragraph.
                 */
struct open_results * open_block;
                /* Pointer to the structure returned by opening
                 * the document. This structure serves to
                 * decouple IR from the actual document file
                 * implementation. The interesting functions are
                 * word_func and get_pos_func.
                 */
struct DOCCON_cont_header * hunt_stuff;
                /* Pointer to the information about the
                 * expressions in this document.
                 */
{
unsigned int * reg_bit_map, * seq_bit_map, * prox_bit_map;
static struct hunt_results hunt_output;
unsigned int reg_bit_size,
                prox_bit_size,
                seq_bit_size;
unsigned int * fixed_reg_bit_map;
unsigned int * curr_prox,
                * prox_list,
                * curr_seq,
                * seq_list;
int * curr_off, *off_list;
struct comp_reg_con ** curr_reg,
                ** reg_list;
int list_size;
int found_flag,
           prox_change_flag,
           reg_match_flag;
int i, j;
unsigned int * i_ptr, *j_ptr;
struct word_results * got_word;
unsigned int seq_buf_size = hunt_stuff->seq_count;
/*
 * Cyclic buffer to track the start of the sequence
 */
struct seq_pos_element {
    unsigned int seq_reg_id;
    long seq_start_pos;
} * seq_pos_buffer,
  * seq_buf_ptr,
  * seq_buf_work;
#ifdef DEBUG
    puts("cont_hunt() called");
#endif
/*
 * If the file is not open, exit with NOT_FOUND
 */
    if (open_block->doc_fd == -1)
    {
        hunt_output.type_of_answer = NOT_FOUND;
        return (&hunt_output);
    }
/*
 * Initialise
 */
    if ((seq_pos_buffer = (struct seq_pos_element *)
        malloc(sizeof(struct seq_pos_element)*seq_buf_size)) == NULL)
    {
        hunt_output.type_of_answer = OUTOFMEM;
        return (&hunt_output);
    }
    bin_root = mem_note(bin_root, (char *) seq_pos_buffer);
    seq_buf_ptr = seq_pos_buffer;
/*
 * Need to allocate and zero the progress monitoring tables and
 * flags.
 *
 * There is:
 * - A bitmap with one element per regular expression;
 * - A bitmap with one element per item in the prox_list;
 * - A bitmap with one element per item in the seq_list;
 */
    reg_bit_size = (hunt_stuff->reg_count + 31)/32;
    prox_bit_size =  (hunt_stuff->prox_count + 31)/32;
    seq_bit_size  =  (hunt_stuff->seq_count + 31)/32;
    if (( reg_bit_map =(unsigned int *)
        malloc (sizeof (unsigned int) *
        (reg_bit_size + prox_bit_size + seq_bit_size))) == NULL)
                                                   /* Allocate space */
    {
        hunt_output.type_of_answer = OUTOFMEM;
        return (&hunt_output);
    }
    bin_root = mem_note(bin_root, (char *) reg_bit_map);
    prox_bit_map = reg_bit_map + reg_bit_size;
    seq_bit_map = prox_bit_map + prox_bit_size;
/*
 * The result structure (a pointer to which is passed) needs
 * to be initialised.
 */
    hunt_output.type_of_answer = NOT_FOUND;
    hunt_output.start_pos = (*open_block->get_pos_func)
                (open_block->doc_channel);
    hunt_output.end_pos = hunt_output.start_pos;
/*
 * The prox and seq bit maps must be cleared
 */
    for (i_ptr = prox_bit_map,
         j_ptr = seq_bit_map + seq_bit_size;
             i_ptr < j_ptr;
                 *i_ptr++ = 0) ;
/*
 * Clear all flags
 */
    found_flag = NOT_FOUND;
    prox_change_flag = NOT_FOUND;
    reg_match_flag = NOT_FOUND;
/*
 * Calculate base addresses for the items in the document context
 * control record.
 */
    reg_list = (struct comp_reg_con * *) (hunt_stuff + 1);
                    /* List of regular expression pointers starts after the
                     * header
                     */
    list_size = hunt_stuff->reg_count;
    fixed_reg_bit_map = (unsigned int *) (reg_list + list_size);
    prox_list = (unsigned int *) (fixed_reg_bit_map +
                         list_size/32 + ((list_size % 32) ? 1 : 0));
    seq_list  = prox_list + hunt_stuff->prox_count;
    off_list  = (int *) (seq_list + hunt_stuff->seq_count);
/*
 * Main Process - Loop; process words until EOF, or match found;
 * continue to the next paragraph boundary past the
 * requested last position of interest
 */
    for (;;)
    {
        got_word=(struct word_results *)
                (*open_block->word_func)
                 (open_block);
        if ((got_word == NULL ) || (got_word->word_type == PARA))
        {                      /* We have come to the end of a paragraph */
            if (prox_change_flag == FOUND)
            {     /* There are proximity expressions and some have been found */
/*
 * Loop - search through the proximity expressions to see
 * if any have been matched in their entirety. This will
 * be true if all the bits are set for the expression.
 */
                for (curr_prox = prox_list,
                     i = 0;
                          (curr_prox < seq_list) && (found_flag == NOT_FOUND);
                              i += *curr_prox,
                          curr_prox += *curr_prox)
                {    /* This steps through the prox expressions */
                unsigned int k;
                unsigned int l;
/*
 * For each prox expression, step through its bitmap to see
 * if the regular expressions have been found in this paragraph
 */
                    for (j = 0,
                         found_flag=FOUND,
                         i_ptr = prox_bit_map + (i+1)/32,
                         l = *i_ptr++,
                         k =  1 << ((i+1) % 32);
                             j < *(curr_prox)-1;
                                  j++)
                    {
                        if (!(k & l))
                        {    /* If regular expression was not found, scarper */
                            found_flag = NOT_FOUND;
                            break;
                        }
                        if (k==(1 << 31))
                        {   /* Advance to the next bitmap entry */
                            l = *i_ptr++;
                            k=1;
                        }
                        else
                            k = (k << 1);
                    }
                }
            }
/*
 * Exit (to close-down) if match, or EOF, or gone as far as the user requested
 */
            if ((found_flag == FOUND) || (got_word == NULL)
             || (got_word->start_pos > max_offset))
                 break;
/*
 * Zeroise the prox bitmap and the seq bitmap
 */
            for (i_ptr = prox_bit_map,
                 j_ptr = seq_bit_map + seq_bit_size;
                     i_ptr < j_ptr;
                          *i_ptr++ = 0) ;
/*
 * Set new paragraph marker to the current position
 */
            hunt_output.start_pos = (*open_block->get_pos_func)
                                      (open_block->doc_channel);
        }
        else /* if (got_word->word_type  == TEXTWORD) */
        {    /*    If we have an ordinary word */
/*
 * Zeroise the regular expression bitmap
 */
            for (i_ptr = reg_bit_map; i_ptr < prox_bit_map; *i_ptr++ = 0) ;
/*
 * Loop; compare this word with the compiled regular expressions, to see if it
 * matches any of them
 */
            for (curr_reg = reg_list,
                 curr_off = off_list,
                 i = 0,
                 reg_match_flag = NOT_FOUND;
                     /* Clear flag that says that this word matches something */

                     i < hunt_stuff->reg_count;
                         i++,
                         curr_off++,
                         curr_reg++)
            {
            int match_result;
/*
 * Skip prox and seq entries
 */
                if (*curr_reg == NULL)
                    continue;
/*
 * Compare this word with each of the regular expressions, using IR_exec()
 */
                if (*curr_off == -1)
                {
                    if ((*curr_reg)->match_ind == got_word->match_ind)
                        match_result = FOUND;
                    else
                    if ((*curr_reg)->match_ind < 0) /* Should be seldom */
                        match_result =
                           IR_exec(got_word->word_ptr,(*curr_reg)->reg_ptr);
                    else
                        match_result = NOT_FOUND;
                    if (match_result == FOUND)
                    {    /* Word matches regular expression */
#ifdef DEBUG
                        fprintf(stderr, "%s : %d\n", got_word->word_ptr, i);
#endif
                        if (*(fixed_reg_bit_map + (i/32)) & (1 << (i % 32)))
                        {    /* If the bitmap flag for this expression is set */
                            found_flag = FOUND;
/*
 * Regular expression implies a match; set flag, and exit
 */
                            hunt_output.start_pos = got_word->start_pos;
                            break;
                        }
                        else
/*
 * Regular expression is part of a Prox or Seq expression
 */
                        {
                            reg_match_flag = FOUND;
                            *(reg_bit_map + (i/32)) |= (1 << (i % 32));
                        }
                    }
                }
                else
/*
 * We have already evaluated this. Use the previous value. Note that this
 * must be a sequence, because we would have stopped searching already
 * otherwise. Also, the flag is already set.
 */
               if (*(reg_bit_map + (*curr_off/32)) & (1 << (*curr_off % 32)))
                   *(reg_bit_map + (i/32)) |= (1 << (i % 32));
           } /* End of loop through regular expressions */
           if (found_flag == FOUND)
               break;                /* Exit to closedown */
           else
           if (reg_match_flag != FOUND)
           {
               if (hunt_stuff->seq_count !=0)
                   for (i_ptr = seq_bit_map,
                        j_ptr = seq_bit_map + seq_bit_size;
                            i_ptr < j_ptr;
                                *i_ptr++ = 0) ;
           }
           else  /* if (reg_match_flag == FOUND) */
           {     /* Matches found, update seq and prox bit-maps */
/*
 * If there are sequential expressions, see if these matches
 * are components of any of them. If so, update their bitmaps,
 * and see if there is a complete match
 */
               if (hunt_stuff->seq_count != 0)
               {
                   for (curr_seq = seq_list,
                        i = 0;
                            (i < hunt_stuff->seq_count)
                          && (found_flag == NOT_FOUND);
                              i += *curr_seq,
                              curr_seq += *curr_seq)
                   {    /* This steps through the seq expressions */
                   unsigned int k; /* Current bit */
                   unsigned int l; /* Word with current bit */
                   unsigned int m; /* Prior bit (in sequence) */
                   unsigned int n; /* Word with prior bit */

                       j = *curr_seq - 1;
#ifdef DEBUG
                       fprintf(stderr, "Bitmap Position: %d\n", i+j);
#endif
                       i_ptr = seq_bit_map + (i+j)/32;
                       l = *i_ptr;
                       k =  1 << (((i+j)) % 32);
                       m = k;
                       if (m==1)
                       {
                           j_ptr = i_ptr - 1;
                           n= *j_ptr;
                           m = (1 << 31);
                       }
                       else
                       {
                           m = (m >> 1);
                           j_ptr = i_ptr;
                           n = l;
                       }
/*
 * We are interested in the immediately prior set bit as well
 *
 * If this regular expression in the sequence has been found
 */
                       if (*(reg_bit_map + (*(curr_seq + j))/32) &
                            (1 << (*(curr_seq + j) % 32)))
                       {
#ifdef DEBUG
                           fputs("Found This\n", stderr);
#endif
                           if ((m & n))
                           {  /* If we have found the prior sequence members */
/*
 * If this value constitutes a context match
 */
#ifdef DEBUG
                               fputs("Found Prior\n", stderr);
#endif
                               if (*(fixed_reg_bit_map + (*(curr_seq+1)/32)) &
                                           (1 << (*(curr_seq+1) % 32)))
                                   found_flag = FOUND;
/*
 * Found; we need to get the start position
 */
                               else
/*
 * Otherwise, this seq must be embedded in a PROX; set the flag in
 * the regular expression bit map, and pick it up later
 */
                                   *(reg_bit_map + (*(curr_seq+1)/32))
                                          |= (1 << (*(curr_seq+1) % 32));
                           }
                       }
/*
 * Clear the previous sequence (always, whether there was a match this time or
 * not).
 */
                       n &= ~m;
                       *j_ptr = n;
                       if (i_ptr == j_ptr)
                           l=n;
/*
 * Advance the current bit one position (or rather, move it back!)
 */
                       if (k==1)
                       {
                           i_ptr--;
                           l=n;
                       }
                       k = m;
   /*
    * Now, loop through the remaining elements in the sequence. If
    * FOUND, we are looking for the start position of the first
    * element in the sequence. In the unlikely event that the buffer
    * in which these are held is not large enough, break out.
    */
                       for (j--; j > 1; j--)
                       {
                            if (found_flag == FOUND)
                            {
/*
 * We found it: need to hunt for the corresponding position
 * element in the start position buffer. Results are rubbish if
 * we go right round the buffer.
 */
                               for (seq_buf_work =
                                           ((seq_buf_ptr == seq_pos_buffer) ?
                                             (seq_pos_buffer + seq_buf_size -1)
                                           : (seq_buf_ptr -1));
                                       seq_buf_work != seq_buf_ptr
                                   && (seq_buf_work->seq_reg_id !=
                                       *(curr_seq + j)) ;
                                            seq_buf_work =
                                            ((seq_buf_work == seq_pos_buffer) ?
                                             (seq_pos_buffer + seq_buf_size -1)
                                          : (seq_buf_work -1)));

                               seq_buf_ptr = seq_buf_work;
                           }
                           else
/*
 * We check if we have managed a partial match for the sequence
 */
                           {
                               m = k;
                               if (m==1)
                               {
                                   j_ptr--;
                                   n = *j_ptr;
                                   m = (1 << 31);
                               }
                               else
                                   m = (m >> 1);
/*
 * We are interested in the immediately prior set bit as well
 *
 * If the current regular expression in the sequence has been found
 */
                               if (*(reg_bit_map + (*(curr_seq + j))/32) &
                                    (1 << (*(curr_seq + j) % 32)))

                               {
/*
 * If the previous members in the sequence have been
 * found, or if this is the first element encountered
 * in the sequence;
 *
 *  This is not the last of a sequence, but it means we have found
 *  some sequence; set the current bit (k) and write back to memory
 *  (l); correct n (previous flag word) if it is the same.
 */
                                   if ((m & n) || j == 2)
                                   {
                                       l |= k;
                                       *i_ptr = l;
                                       if (i_ptr == j_ptr)
                                           n=l;
/*
 * We need to add the current position to the cyclic buffer that is
 * searched when looking for the start position of a sequence.
 */
                                       seq_buf_ptr->seq_reg_id = *(curr_seq+j);
                                       seq_buf_ptr->seq_start_pos = 
                                                got_word->start_pos;
                                       seq_buf_ptr++;
                                       if (seq_buf_ptr ==
                                             (seq_pos_buffer + seq_buf_size))
                                           seq_buf_ptr = seq_pos_buffer;
                                   }
                               }
/*
 * Clear the previous sequence (always, whether there was a match this
 * time or not). This is strictly incorrect if this is the
 * last element before exit, but the bit cleared is not used for
 * any purpose.
 */
                               n &= ~m;
                               *j_ptr = n;
                               if (i_ptr == j_ptr)
                                   l=n;
/*
 * Advance the current bit one position (or rather, move it back!)
 */
                               if (k==1)
                               {
                                   i_ptr--;
                                   l=n;
                               }
                               k = m;
                           }
                       }    /* End of For loop through a single sequence */
                       if (found_flag == FOUND)
                           hunt_output.start_pos = seq_buf_ptr->seq_start_pos;
                   }    /* End of For loop through all sequences */
/*
 * Scarper if match on the sequence
 */
                   if (found_flag == FOUND)
                       break;
               }    /* End of If block when there is a sequence */
/*
 * If there are any prox expressions, see if any regular expression bits need
 * setting
 */
               if (hunt_stuff->prox_count != 0)
               {
/*
 * Loop - search through the proximity expressions to see if any have been
 * matched.
 */
                   for (curr_prox = prox_list,
                        i = 0;
                           (i < hunt_stuff->prox_count)
                        && (found_flag == NOT_FOUND);
                                i += *curr_prox,
                                curr_prox += *curr_prox)
                   {    /* This steps through the prox expressions */
                   unsigned int k;
/*
 * Loop through the prox regular expressions, setting bitmap bits if any have
 * been found earlier
 */
                       for (j = 1,
                            i_ptr = prox_bit_map + (i+1)/32,
                            k =  (1 << ((i+1) % 32));
                                 j < *curr_prox;
                                      j++)
                       {
/*
 * If this regular expression (part of the prox) has been found
 */
                            if (*(reg_bit_map + (*(curr_prox + j))/32) &
                                         (1 << (*(curr_prox + j) % 32)))
                            {
                                *i_ptr |= k;
                                prox_change_flag = FOUND;
                            }
                            if (k==(1 << 31))
                            {
                                i_ptr++;
                                k=1;
                            }
                            else
                                k = (k << 1);
                        }
                    }
                }      /* End of block if there are some proxes to find */
            }      /* End of regular expressions found block */
        }      /* End of TEXTWORD block */
    }      /* End of infinite for loop */
/*
 * Closedown
 *
 * - If found, the start position has already been set
 */
    hunt_output.type_of_answer = found_flag;
    if (got_word == NULL)
    {
        (*open_block->set_pos_func) (open_block->doc_channel,0,2);
        hunt_output.end_pos = (*open_block->get_pos_func)
                    (open_block->doc_channel);
    }
    else 
        hunt_output.end_pos = got_word->start_pos + got_word->word_length
                      - 1;
    return (&hunt_output);
}                         /* End of context Hunter */
/*
 * Structure used by the routines that read and write context hunting
 * control records. Private to these routines.
 */
struct file_con {
    FILE * channel;
    long prev_offset;
    long curr_offset;
    long next_offset;
    struct DOCCON_cont_header * curr_mem;
} file_con;
struct DOCCON_cont_header work_header;
static char buf[BUFSIZ];
/*
 * Routine for creating a new context hunt control file
 */
int cont_create()
{
char file_name[128];
#ifdef NT4
    sprintf(file_name,"c%s",getenv("USERNAME"));
#else
    strcpy(file_name, ccont);
#endif
            /* Create the file name to open */
#ifdef DEBUG
    puts("cont_create() called");
#endif
    if ((file_con.channel = fopen(file_name,"wb+")) == NULL)
        return (-1);    /* exit if cannot create */

    setbuf(file_con.channel,buf);
    file_con.prev_offset = 0;
    file_con.curr_offset = 0;
    file_con.next_offset = 0;
    file_con.curr_mem = NULL;
    return (0);
}
/*
 * Routine to write out a context hunt control record
 */
int cont_write(header_to_write)
struct DOCCON_cont_header * header_to_write;
{
struct comp_reg_con work_reg;
struct comp_reg_con ** curr_reg;
int i;

#ifdef DEBUG
    puts("cont_write() called");
#endif
    work_header.this_doc = header_to_write->this_doc;
    work_header.backward_pos_control.prev_record_size =
        file_con.curr_offset - file_con.prev_offset;
    work_header.reg_con_count = header_to_write->reg_con_count;
    work_header.reg_count = header_to_write->reg_count;
    work_header.seq_count = header_to_write->seq_count;
    work_header.prox_count = header_to_write->prox_count;

    fseek (file_con.channel, sizeof(struct DOCCON_cont_header), 1);

    for (curr_reg = (struct comp_reg_con **) (header_to_write + 1),
         work_header.reg_control.reg_size = 0,
         work_reg.reg_ptr = NULL,
         work_reg.reg_node_ptr   = NULL,
         work_reg.next_reg_ptr   = NULL,
         i = work_header.reg_count;
             i;
                 curr_reg++,
                 i--)
    {
        if (*curr_reg == NULL)
            continue;
        work_reg.exp_len = (*curr_reg)->exp_len;
        if (work_reg.exp_len % 2)
        {    /* need to worry about alignment */
        char x;
            x = '\0';
            work_reg.exp_len++;
            fwrite ( &work_reg, sizeof(work_reg),1,file_con.channel);
            fwrite ((*curr_reg)->reg_ptr,work_reg.exp_len-1,1,file_con.channel);
            fwrite ( &x,1,1,file_con.channel);
        }
        else
        {
            fwrite ( &work_reg, sizeof(work_reg),1,file_con.channel);
            fwrite ( (*curr_reg)->reg_ptr, work_reg.exp_len,1,file_con.channel);
        }
        *curr_reg = (struct comp_reg_con *) (work_header.reg_control.reg_size +
                                sizeof(work_header));
                     /* The cast keeps the compiler happy; of course, the value
                      * stored is certainly no memory pointer!
                      */
        work_header.reg_control.reg_size += work_reg.exp_len + sizeof(work_reg);
    } 
    fwrite ((header_to_write + 1),
    (sizeof(struct comp_reg_con **) * work_header.reg_count +
          (work_header.reg_count/32 + ((work_header.reg_count % 32) ? 1 : 0)) *
          sizeof (unsigned int) + sizeof (unsigned int) *
          (work_header.seq_count + work_header.prox_count)),1,file_con.channel);
    file_con.next_offset    = ftell(file_con.channel) ;
    work_header.forward_pos_control.record_size = file_con.next_offset
                            - file_con.curr_offset;
    fseek (file_con.channel, file_con.curr_offset, 0);
    fwrite(&work_header, sizeof(work_header),1,file_con.channel);
    fseek (file_con.channel,file_con.next_offset,0);
    file_con.prev_offset = file_con.curr_offset;
    file_con.curr_offset = file_con.next_offset;
    browse_unlink(work_header.this_doc);
    file_con.curr_mem = NULL;
    return (0);
}
/*
 * Function to close the context hunter control file
 */
int cont_close()
{
#ifdef DEBUG
    puts("cont_close() called");
#endif
    fclose (file_con.channel);
    file_con.channel = NULL;
    return (0);
}
long last_position_read;
/*
 * Function to open the context hunter's temporary file for reading,
 * and to read the first record
 */
struct DOCCON_cont_header * cont_reopen ()
{
struct DOCCON_cont_header * ret_val;
struct comp_reg_con * reg_base;
struct comp_reg_con ** curr_reg;
char * reg_base_as_char;
int i;
char file_name[128];

    sprintf(file_name,"c%s",getenv("USERNAME"));
                               /* Create the file name to open */
    if ((file_con.channel = fopen(file_name,"rb")) == NULL)
        return (NULL);         /* Exit if cannot open */

    setbuf(file_con.channel,buf);
    file_con.prev_offset = 0;
    file_con.curr_offset = 0;
    if (fread (& work_header, sizeof(work_header), 1, file_con.channel)
            <= 0)
        return (NULL);
    file_con.next_offset = work_header.forward_pos_control.record_size;
    file_con.curr_mem = NULL;

    if ((reg_base = (struct comp_reg_con *)
                         malloc(work_header.reg_control.reg_size)) == NULL)
        return (NULL);
    bin_root = mem_note(bin_root, (char *) reg_base);
    if (fread(reg_base, work_header.reg_control.reg_size,1,
                  file_con.channel) <= 0)
        return (NULL);
    if ((ret_val = (struct DOCCON_cont_header *)
            malloc(work_header.forward_pos_control.record_size -
                work_header.reg_control.reg_size)) == NULL)
        return (NULL);
    bin_root = mem_note(bin_root, (char *) ret_val);
    if (fread ((ret_val + 1),(work_header.forward_pos_control.record_size
               - work_header.reg_control.reg_size - sizeof(work_header)),1,
               file_con.channel) <= 0)
        return (NULL);
    for (i = work_header.reg_count,
         reg_base_as_char = (char *) reg_base,
         curr_reg = (struct comp_reg_con **) (ret_val + 1);
             i;
                 i--,
                 curr_reg ++)
    {
        if (*curr_reg == NULL)
            continue;
        *curr_reg = (struct comp_reg_con *) (reg_base_as_char +
                      (int) (*curr_reg) - sizeof(work_header));
                            /* The file layout guarantees alignment */
        (*curr_reg)->reg_ptr = (reg_comp) (*curr_reg + 1);
    }
    ret_val->this_doc = work_header.this_doc;
    ret_val->forward_pos_control.next_cont_header = NULL;
    ret_val->backward_pos_control.prev_cont_header = NULL;
    ret_val->reg_con_count = work_header.reg_con_count;
    ret_val->reg_count = work_header.reg_count;
    ret_val->prox_count = work_header.prox_count;
    ret_val->seq_count = work_header.seq_count;
    last_position_read = ftell (file_con.channel);
    if (browse_open(ret_val) < 0)
        return (NULL);    /* initialise for browse control */
    file_con.curr_mem = ret_val;
    return (ret_val);
}
/*
 * Function to read the next or previous context hunt record.
 */
struct DOCCON_cont_header * cont_read (next_or_prev)
int next_or_prev;    /* next or previous */
{
register struct DOCCON_cont_header * ret_val;
struct comp_reg_con * reg_base;
char * reg_base_as_char;
register struct comp_reg_con ** curr_reg;
int i;

    if ((next_or_prev == PREVIOUS) && (( file_con.curr_offset == 0) ||
            ((file_con.prev_offset = -1)
       && (file_con.curr_mem->backward_pos_control.prev_cont_header == NULL))))
        return (NULL);    /* At beginning of file; no previous */
    else
    if ((next_or_prev == NEXT)
      && (file_con.curr_mem->forward_pos_control.next_cont_header == NULL))
    {    /* If we need to read another record */
        fseek(file_con.channel,last_position_read,0);
        if (fread (& work_header, sizeof(work_header), 1, file_con.channel)<= 0)
            return (NULL);
        if ((reg_base = (struct comp_reg_con *)
                      malloc(work_header.reg_control.reg_size)) == NULL)
            return (NULL);
        bin_root = mem_note(bin_root, (char *) reg_base);
        if (fread(reg_base, work_header.reg_control.reg_size,1,
                      file_con.channel) <= 0)
            return (NULL);
        if ((ret_val = (struct DOCCON_cont_header *)
                        malloc(work_header.forward_pos_control.record_size -
                work_header.reg_control.reg_size)) == NULL)
            return (NULL);
        bin_root = mem_note(bin_root, (char *) ret_val);

        if (fread ((ret_val + 1),(work_header.forward_pos_control.record_size
             - work_header.reg_control.reg_size - sizeof(work_header)),1,
                file_con.channel) <= 0)
            return (NULL);
        for (i = work_header.reg_count,
             reg_base_as_char = (char *) reg_base,
             curr_reg = (struct comp_reg_con **) (ret_val + 1);
                 i;
                     i--,
                     curr_reg ++)
        {
            if (*curr_reg == NULL)
                continue;
            *curr_reg = (struct comp_reg_con *) (reg_base_as_char
                         + (int) (*curr_reg) - sizeof(work_header));
            (*curr_reg)->reg_ptr = (reg_comp) (*curr_reg + 1);
        }
        ret_val->this_doc = work_header.this_doc;
        ret_val->forward_pos_control.next_cont_header = NULL;
        ret_val->backward_pos_control.prev_cont_header = 
        file_con.curr_mem;
        if (file_con.curr_mem != NULL)
            file_con.curr_mem->forward_pos_control.next_cont_header = ret_val;
        ret_val->reg_con_count = work_header.reg_con_count;
        ret_val->reg_count = work_header.reg_count;
        ret_val->prox_count = work_header.prox_count;
        ret_val->seq_count = work_header.seq_count;
        last_position_read = ftell (file_con.channel);
        file_con.prev_offset = file_con.curr_offset; 
        file_con.curr_offset = file_con.next_offset;
        file_con.next_offset = last_position_read;
        file_con.curr_mem = ret_val;
    }
    else
    if (next_or_prev == NEXT)
    {
        file_con.prev_offset = file_con.curr_offset; 
        file_con.curr_offset = file_con.next_offset;
        file_con.next_offset = -1;
        ret_val = file_con.curr_mem->forward_pos_control.next_cont_header;
    }
    else    /*next_or_prev == PREVIOUS */
    { 
        file_con.next_offset = file_con.curr_offset; 
        file_con.curr_offset = file_con.prev_offset;
        file_con.prev_offset = -1;
        ret_val = file_con.curr_mem->backward_pos_control.prev_cont_header;
    }
    browse_close();    /* tidy up the browse control file for this document */
    if (browse_open(ret_val) < 0)
        return (NULL);    /* if cannot initialise browsing, scarper */
    file_con.curr_mem = ret_val;
    return ret_val;
}
