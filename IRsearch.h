/*
 * IRsearch.h - header file for the IR Search sub-system
 * SCCS ID: %W% %G%
 */
#ifndef IRSEARCH_H
#define IRSEARCH_H
#ifdef BADSORT
#include "tabdiff.h"
#else
#include "natregex.h"
#endif
/*
 * comp_reg_con structures are headers for compiled regular expressions.
 */
struct comp_reg_con {
    reg_comp reg_ptr;
    int exp_len;
    int match_ind;
    struct search_node * reg_node_ptr;
    struct comp_reg_con * dup_reg_ptr;
    struct comp_reg_con * next_reg_ptr;
} ;

struct search_node {
    struct search_node * prev_eval_node; /* previous node evaluated */
    struct search_node * next_eval_node; /* next node (to be) evaluated */
    char node_type;                      /* type of evaluation  (see below) */
    unsigned int input_node_count;       /* count of lists to merge */
    struct search_node * input_node;  /* pointer to last input nodes to merge */
    struct search_node * together_node;
                /* pointer to previous node to merge with this one to merge */
    union {
        char * word_ptr;
        struct comp_reg_con * reg_con_ptr;
    } aux_ptr;
    unsigned int doc_count;          /* Count of documents that satisfied this
                                      * condition */
    unsigned int * doc_list;         /* Pointer to list of documents that
                                      * satisfied this operation. This is
                                      * sized by the evaluation routine, based
                                      * on the operator concerned, and the
                                      * number of pointers input. (OR, sum;
                                      * AND, max; NOT, first; SEQ and PROX
                                      * there is only one input stream).
                                      */
};
/*
 * The search node structure serves two purposes;
 *
 * (i)   It is decoded to evaluate the search condition. Each node
 *       corresponds to a single pointer primitive. There is a pointer
 *       to the next node that must be evaluated (and pointers back
 *       to previous ones; these may be useful latter).
 * (ii)  Sub-sections of it have the property that they are evaluated
 *       by the context hunting module for particular documents.
 * (iii) It must be possible to work backwards through the evaluated
 *       structure to find which contexts need to be looked for in
 *       which documents.
 */
/*
 * Macro for adding node to the evaluation chain
 */
#define ADDEVAL(x) {if(last_eval_node)last_eval_node->next_eval_node=(x);\
        else start_node = (x);\
(x)->prev_eval_node=last_eval_node;(x)->next_eval_node=NULL;last_eval_node=(x);}
/*
 * Macro for linking node x and y together
 */
#define TOGLINK(x,y) (x)->together_node=(y)
/*
 * Macro for specifying that node y is the first
 * input node for node x
 */
#define INPLINK(x,y) (x)->input_node=(y)
/*
 * Macro for creating an initialised search node x.
 */
#define NEWSEARCH(x) {(x)=(struct search_node *)\
calloc(sizeof(struct search_node),1);\
bin_root = mem_note(bin_root, ((char *)(x)));}
/*
 * Macro for creating an initialised compiled regular expression node.
 */
#define NEWCOMPREG(x) {(x)=(struct comp_reg_con *)\
calloc(sizeof(struct comp_reg_con),1);x->match_ind=-1;\
bin_root = mem_note(bin_root, ((char *)(x)));}

#define IR_UNDEFINED 0      /* The initial value for the type */
#define IR_OR        0x1    /* OR the lists */
#define IR_AND       0x2    /* AND the lists */
#define IR_NOT       0x4    /* Take from the first, if not in the others */
#define IR_PROX      0x8    /* Hunt for paragraph context */
#define IR_SEQ       0x10   /* Hunt for terms in sequence */
#define IR_REG       0x20   /* There is a regular expression associated
                             * with this node */
#define IR_WORD      0x40   /* there is a word associated with this node */

/*
 * Declaration for the start element in the search node tree.
 */
extern struct search_node * start_node;
/*
 * Declaration for the addition point to the regular expression chain.
 */
extern struct comp_reg_con * reg_start;
/*
 * Declaration for the anchor point to the regular expression chain.
 */
extern struct comp_reg_con * last_reg;
/*
 * Declaration for the anchor point for the stuff that can be binned at the end.
 */
extern struct mem_con * bin_root;
#endif

