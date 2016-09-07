%{
/*
 * IRsearch.y - Search Processor Parser and Main Control
 *
 * Originally, the program took the search definition on stdin, emitted
 * a list of files on stdout, and created a context hunt control file named
 * with reference to the user name.
 *
 * Additionally, it can now take an argument, a file name with a SQL statement
 * in it. It generates the search definition from this, and then continues as
 * before.
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1998";
#ifndef NT4
#ifndef LINUX
#ifndef HP7
#ifndef E2
#define INCLUDE_EARLY
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
#include <errno.h>
#include <signal.h>
#ifndef NT4
#include <string.h>
#else
#ifdef MINGW32
#include <string.h>
#else
#include <strings.h>
#endif
#endif
#include <setjmp.h>
#include "orlex.h"
#include "IRsearch.h" 
#include "IRfiles.h" 
#include "conthunt.h"
struct prog_con {
    char * name;
    unsigned long module_id;
    struct prog_con * next;
};
extern char * strnsave();
/*
 * Support for abandoning the parse on unexpected or syntax errors
 */
jmp_buf parse_env;
/*
 * Termination signal that interrupts execution; files are closed, things
 * should get cleaned up
 */
void sigterm();
int sig = SIGTERM;
/*
 * Define the root of the search node structure
 */
struct mem_con * bin_root = NULL;
/*
 * Define the root of the search node structure
 */
struct search_node * start_node = NULL;
/*
 * Frig to remember the input to a SEQ
 */
static struct search_node * frig_node = NULL;
/*
 * Define the root of the chain of regular expressions.
 */
struct comp_reg_con * reg_start = NULL;
/*
 * Define the addition point for the chain of regular expressions.
 */
struct comp_reg_con * last_reg = NULL;
/*
 * Define the end of the search node structure
 */
struct search_node * last_eval_node = NULL;
/*
 * The expressions returned by the rules recognised are values on
 * the yacc stack; these are a pair of search node pointers, an
 * "input node", and an output node.
 *
 * The order of evaluation is driven by the order in which "input
 * nodes" are encountered.
 *
 * The other chains are updated with reference to the "output nodes?"
 */
long int retval;
#ifdef OSF
long int con_set();
#endif

typedef struct { struct search_node * in_node;
               struct search_node * out_node;
} inout;
%}
%union {
int intval;
inout inoutval;
}

%token <intval> RIGHTBRACKET DUMMYAND DUMMYNOT
%token <intval> AND NOT ENDPROX ENDSEQ STARTPROX STARTSEQ REGEXP LEFTBRACKET
%left OR REGEXP
%left AND NOT
%left DUMMYAND DUMMYNOT
%left RIGHTBRACKET
%right  LEFTBRACKET
%right STARTPROX
%left  ENDPROX
%right STARTSEQ
%left  ENDSEQ
%start term
%type <intval> empty 
%type <inoutval> term endprox startproxexp endseq startseq 
%type <inoutval>  endand endnot word
%{
/*
 * Incorporate the Search token recognition code
 */
#ifdef INCLUDE_EARLY
#include "IRslex.c"
#endif
%}
%%

term   :  endprox %prec ENDPROX
{
/*
 *  Return the output term (usually a PROX operation) as the output node,
 *  with the input node as null.
 */
    { 
        $$.in_node=NULL;
        $$.out_node=$1.out_node;
    }
}
    | endnot %prec NOT
{
/*
 *  Add the output term to the evaluation chain. Return it as the 
 *  output node, with the input node as null.
 */
    { 
        ADDEVAL($1.out_node);
        $$.in_node=NULL;
        $$.out_node=$1.out_node;
    }
}
    | endand %prec AND
{
/*
 *  Add the output term to the evaluation chain. Return it as the 
 *  output node, with the input node as null.
 */
    { 
        ADDEVAL($1.out_node);
        $$.in_node=NULL;
        $$.out_node=$1.out_node;
    }
}
    |  LEFTBRACKET term  RIGHTBRACKET
{
/*
 *  Return the values from the term rule. Otherwise nothing.
 */
    $$.out_node=$2.out_node;
    $$.in_node=NULL;
}
    | word %prec OR
{
/*
 *  Return the values from the word rule.
 */
    $$.out_node=$1.out_node;
    $$.in_node=NULL;
}
    | term term %prec OR
{
/*
 *  The problem is the absence of the OR operator; the code must check if
 *  a final OR output node is needed, in which case it is linked to the
 *  output nodes from the two terms, or if they are themselves OR expressions,
 *  the existing output node is retained, and the new term is linked to it.
 *
 *  If a new node is created, return the new node as output node, the existing 
 *  output node as input node. Set count of input nodes in output node to 1. 
 *  Point the next eval from the input to the output.
 *
 * Otherwise chain the new term output node to the existing input node;
 * increment the count of input nodes. Insert the input node in the evaluation
 * chain.
 *
 * This is done whenever a term is recognised.
 */
    {
        if ($1.out_node->node_type == IR_OR ) 
/*
 * OR is associative; combine the OR operations if possible
 */
        {
        struct search_node * x, * x1, * x2;
            x = $1.out_node;
            x1 = $1.out_node->prev_eval_node;
            x2 = $1.out_node->next_eval_node;
/*
 *    Begin by unhitching the first term from the evaluation chain
 */
            x1->next_eval_node = x2;
            x2->prev_eval_node = x1;
/*
 *    Now add the second term as an input to the first term
 */
            TOGLINK($2.out_node,x->input_node);
            INPLINK(x,$2.out_node);
            x->input_node_count++;
/*
 *    Now add the first term back into the evaluation chain
 */
            ADDEVAL(x);
/*
 *    Make the first term the output node, clear the input
 */
            $$.out_node = $1.out_node;
            $$.in_node = NULL;
        }
        else
/*
 * Must create a new top level OR node
 */
        {
        struct search_node * y;
    
            NEWSEARCH(y);
            if (y == (struct search_node *) NULL)
                longjmp(parse_env, OUTOFMEM);
                                           /* Abandon if out of memory space */ 
            y->node_type = IR_OR ;
            INPLINK(y,$1.out_node);
            TOGLINK($1.out_node,$2.out_node);
            y->input_node_count = 2;
            ADDEVAL(y);
            $$.in_node=NULL;
            $$.out_node=y;
        }
    }
}
    | term empty %prec STARTSEQ
    | empty term %prec STARTPROX
{
    $$.out_node = $2.out_node;
    $$.in_node = NULL;
}
    ;

empty : STARTPROX ENDPROX %prec ENDSEQ
      | STARTSEQ ENDSEQ %prec ENDSEQ
      | LEFTBRACKET RIGHTBRACKET %prec ENDSEQ
      | empty empty %prec ENDSEQ
    ;


word : REGEXP
{
/*
 *  Compile the regular expression; if fail, longjmp out; the parse is
 *  aborted.
 *
 *  Add the compiled regular expression to the list of regular
 *  expressions.
 *
 *  Create a new search node for the regular expression; input count
 *  zero, output pointers null, type OR.
 *
 *  Add it to the evaluation chain (macro addeval)
 *
 *  Tidy up all the pointers.
 *
 *  Return the output node as this, the input node as null.
 */
    {
    struct comp_reg_con * x;
    struct search_node * y;
    long int buf_size;
    long int exp_size;
    char * x1;

        NEWCOMPREG(x);

        if (x== (struct comp_reg_con *) NULL)
            longjmp(parse_env, OUTOFMEM);   /* Abandon if out of memory space */
        if (last_reg == NULL)
            reg_start = x;
        else
            last_reg->next_reg_ptr = x;
        last_reg=x;
        last_reg->next_reg_ptr = NULL;
        for (x1 = yytext; *x1 != 0; x1++)
             if (islower(*x1))
                 *x1 = toupper(*x1);
/*
 * Attempt to compile the regular expression
 */
        for (buf_size=ESTARTSIZE;;)
        {
            if ((last_reg->reg_ptr=(reg_comp) malloc(buf_size))==NULL)
                longjmp(parse_env, OUTOFMEM);
            exp_size = (long) re_comp(yytext,last_reg->reg_ptr,buf_size);

            if (exp_size != (buf_size + 1))
                break;
            buf_size = (buf_size << 1);
            free(last_reg->reg_ptr);
        }
        bin_root = mem_note(bin_root, (char *) last_reg->reg_ptr);
        if (exp_size > buf_size)
/*
 * The compile routine found an error; return an expression
 * that points to an error string
 */
            longjmp(parse_env,(exp_size - 1 - buf_size));
/*
 * O.K.; set up the search node, and the pointers
 */
        NEWSEARCH(y);
        if (y == (struct search_node *) NULL)
            longjmp(parse_env, OUTOFMEM);   /* Abandon if out of memory space */
        last_reg->reg_node_ptr = y;
        last_reg->exp_len = exp_size;
        last_reg->dup_reg_ptr = (struct comp_reg_con *) NULL;
        for (x = reg_start; x != last_reg; x = x->next_reg_ptr)
             if (!strcmp(x->reg_ptr, last_reg->reg_ptr))
             {
                 last_reg->dup_reg_ptr = x;
                 break;
             }
        y->node_type = IR_OR | IR_REG;
        y->aux_ptr.reg_con_ptr = last_reg;
        ADDEVAL(y);
        $$.in_node=NULL;
        $$.out_node=y;
    }
}
     | endseq %prec ENDSEQ
     | STARTPROX word ENDPROX
{
/*
 *  Return the values from the word rule.
 */
    $$.out_node=$2.out_node;
    $$.in_node=NULL;
}
     | STARTSEQ word ENDSEQ
{
/*
 *  Return the values from the word rule.
 */
    $$.out_node=$2.out_node;
    $$.in_node=NULL;
}
     | LEFTBRACKET word RIGHTBRACKET
{
/*
 *  Return the values from the word rule.
 */
    $$.out_node=$2.out_node;
    $$.in_node=NULL;
}
     | empty word %prec ENDSEQ
{
/*
 *  Return the values from the word rule.
 */
    $$.out_node=$2.out_node;
    $$.in_node=NULL;
}
     | word empty %prec ENDSEQ
{
/*
 *  Return the values from the word rule.
 */
    $$.out_node=$1.out_node;
    $$.in_node=NULL;
}
     ;

startproxexp : STARTPROX word word %prec STARTPROX
{
/*
 *  Create two new new search nodes, an AND node and a PROX node, with
 *  the second as input to the first, and the evaluation pointers
 *  filled in (since we need to get the PROX node back later).
 *
 *  Chain the new term as input to the AND node.
 *
 *  Return the AND node as output, and the term node as input.
 */
    {
    struct search_node * x, * x1;

        NEWSEARCH(x);
        if (x == (struct search_node *) NULL)
            longjmp(parse_env, OUTOFMEM);   /* Abandon if out of memory space */
        NEWSEARCH(x1);
        if (x1 == (struct search_node *) NULL)
            longjmp(parse_env, OUTOFMEM);   /* Abandon if out of memory space */
        x->node_type = IR_AND ;
        x1->node_type = IR_PROX ;
        x->input_node_count = 2;
        x1->input_node_count = 1;
        INPLINK(x1,x);
        INPLINK(x,$2.out_node);
        TOGLINK($2.out_node,$3.out_node);
        x->next_eval_node = x1;
        x1->prev_eval_node = x;
        $$.in_node=$3.out_node;
        $$.out_node=x;
    }
}
         | startproxexp word %prec STARTPROX
{
/*
 *   Link the term to the input node from startproexp.
 *
 *   Increment the count of inputs for  the output node.
 *
 *   Return the startproexp (AND) output node as output, and the out node
 *   from the new term as input.
 */
    {
        TOGLINK($1.in_node, $2.out_node);
        $1.out_node->input_node_count++;
        $$.out_node = $1.out_node;
        $$.in_node  = $2.out_node;
    }
}
    ;
endprox : startproxexp ENDPROX
{
/*
 *  Get back the pointer to the PROX (it is in the AND
 *  next_eval_node), add the AND and PROX to the evaluation list.
 *
 *  Make the PROX output node.
 */
    {
    struct search_node * x;

        x=$1.out_node->next_eval_node;
        ADDEVAL($1.out_node);
        ADDEVAL(x);
        $$.out_node = x;
        $$.in_node = NULL;
    }
}
    ;
startseq : STARTSEQ word word %prec STARTSEQ
{
/*
 *  Create two new new search nodes, an AND node and a SEQ node, with
 *  the second as input to the first, and the two chained together
 *  by the evaluation pointers.
 *
 *  Chain the new term as input to the AND node.
 *
 *  Return the AND node as output, and the second word node as input.
 */
    {
    struct search_node * x, * x1;

        NEWSEARCH(x);
        if (x == (struct search_node *) NULL)
            longjmp(parse_env, OUTOFMEM);   /* Abandon if out of memory space */
        NEWSEARCH(x1);
        if (x1 == (struct search_node *) NULL)
            longjmp(parse_env, OUTOFMEM);   /* Abandon if out of memory space */
        x->node_type = IR_AND ;
        x1->node_type = IR_SEQ ;
        x->input_node_count = 2;
        x1->input_node_count = 1;
        INPLINK(x1,x);
        INPLINK(x,$2.out_node);
        TOGLINK($2.out_node,$3.out_node);
        x->next_eval_node = x1;
        x1->prev_eval_node = x;
        $$.in_node=$3.out_node;
        $$.out_node=x;
    }
}
        | startseq word %prec STARTSEQ
{
/*
 *   Link the term to the input node from startseq.
 *
 *   Increment the count of inputs for  the output node.
 *
 *   Return the startseq (AND) output node as output, and the out node
 *   from the new term as input.
 */
    {
        TOGLINK($1.in_node, $2.out_node);
        $1.out_node->input_node_count++;
        $$.out_node = $1.out_node;
        $$.in_node  = $2.out_node;
    }
}
    ;
endseq : startseq ENDSEQ
{
/*
 *  Get back the pointer to the SEQ (it is in the AND
 *  next_eval_node), add the AND and SEQ to the evaluation list.
 *
 *  The SEQ must be returned as output, and the AND as input.
 */
    {
    struct search_node * x;

        x=$1.out_node->next_eval_node;
        ADDEVAL($1.out_node);
        ADDEVAL(x);
        $$.out_node = x;
        $$.in_node = NULL;
    }
}
    ;
endand : term AND term %prec AND
{
/*
 *  Create a new search node, type AND, 2 for the input count.
 *
 *  Make the first term output the input for the AND node, chain
 *  the second term output together to the first term output.
 *
 *  Return the new node as the output node, the second output node
 *  as the input node. 
 */
    {
/*
 * Must create a new top level AND node
 */
    struct search_node * y;

        NEWSEARCH(y);
        if (y == (struct search_node *) NULL)
            longjmp(parse_env, OUTOFMEM);   /* Abandon if out of memory space */
        y->node_type = IR_AND ;
        INPLINK(y,$1.out_node);
        TOGLINK($1.out_node, $3.out_node);
        y->input_node_count = 2;
        ADDEVAL(y);
        $$.in_node=NULL;
        $$.out_node=y;
    }
}
    | endand AND term %prec DUMMYAND
{
/*
 *  Chain the term output node to the input node from the endand.
 *
 *  Increment the input count for the output search node. 
 *
 *  Return the output node from the endand as output, the output
 *  node from the term as input.
 */
    {
        TOGLINK($1.in_node, $3.out_node) ;
        $1.out_node->input_node_count++;
        $$.out_node = $1.out_node;
        $$.in_node  = $3.out_node;
    }
}
    ;
endnot : term NOT term %prec NOT
{
/*
 *  Create a new search node, type NOT, 2 for the input count.
 *
 *  Make the first term output the input for the NOT node, chain
 *  the second term output together to the first term output.
 *
 *  Return the new node as the output node, the second output node
 *  as the input node. 
 */
    {
    struct search_node * y;

        NEWSEARCH(y);
        if (y == (struct search_node *) NULL)
            longjmp(parse_env, OUTOFMEM);   /* Abandon if out of memory space */
        y->node_type = IR_NOT ;
        INPLINK(y,$1.out_node);
        TOGLINK($1.out_node, $3.out_node);
        y->input_node_count = 2;
        ADDEVAL(y);
        $$.in_node=NULL;
        $$.out_node=y;
    }
}
    | endnot NOT term %prec DUMMYNOT
{
/*
 *  Chain the term output node to the input node from the endnot.
 *
 *  Increment the input count for the output search node. 
 *
 *  Return the output node from the endnot as output, the output
 *  node from the term as input.
 */
    {
        TOGLINK($1.in_node, $3.out_node);
        $1.out_node->input_node_count++;
        $$.out_node = $1.out_node;
        $$.in_node  = $3.out_node;
    }
}
    ;
%%
#ifdef DEBUG
#ifdef YYDEBUG
extern int yydebug;
#endif
#endif
static char buf[BUFSIZ];
#ifdef NT4
#ifdef MINGW32
/* #define stdin (FILE *) 0 */
/* #define stdout (FILE *) 1 */
#endif
#endif
#ifndef INCLUDE_EARLY
#include "IRslex.c"
#endif
/*******************************************************************************
 * Dismantle the search structures
 */
static void search_dismantle()
{
    mem_forget(bin_root);
    bin_root = (struct mem_con *) NULL;
    start_node = NULL;
    last_eval_node = NULL;
    reg_start = NULL;
    last_reg = NULL;
    return;
}
/*******************************************************************************
 * Allocate a prog_con structure
 */
struct prog_con * new_prog_con(canchor, this_doc)
struct prog_con * canchor;
doc_id this_doc;
{
datum look_up;
datum found;
struct prog_con * nc;

    look_up.dptr = (char *) &this_doc;
    look_up.dsize = sizeof(doc_id);
    found = fetch(look_up);
    if (found.dptr == NULL)
        fprintf(stderr, "Error; document look up failed for %u\n",
                       this_doc);
    nc = canchor;
    canchor = (struct prog_con *) malloc(sizeof(struct prog_con));
    canchor->next = nc;
    canchor->name = strnsave(found.dptr, found.dsize);
    canchor->module_id = this_doc;
    return canchor;
}
void clear_progs(pc)
struct prog_con * pc;
{
struct prog_con * ppc;

    while (pc != (struct prog_con *) NULL)
    {
        ppc = pc;
        pc = pc->next;
        free(ppc->name);
        free(ppc);
    }
    return;
}
/*
 * Export the name of a candidate program or procedure
 */
void prog_print(fp, p)
FILE * fp;
struct prog_con * p;
{
    fputs(p->name, fp);
    fputc('\n', fp);
    return;
}
/*******************************************************************************
 * Do any search.
 */
int do_any_search(fname, out_channel, hit_list)
char * fname;
FILE * out_channel;
struct prog_con ** hit_list;
{
unsigned int i;
struct DOCCON_cont_header * work_doc;
struct prog_con * canchor, * nc;
struct search_node * work_node;
int found_flag = 0;

    if (retval=setjmp(parse_env))
    {
        (void) fclose(yyin);
        if (retval == OUTOFMEM)
        {
            fputs("malloc failed; out of memory ?!\n", stderr);
            return 0;
        }
        else
        {
/*
 * Returned a pointer to an error string .. yuk!
 */
            fputs((char *) retval, stderr);
            return 0;
        }
    }
    else
    {
        yyparse();        /* process the input */
        (void) fclose(yyin);
/*
 *  At this point, start the search.
 *  -    Find the words that match the regular expressions (see IRreghunt.c)
 */
        if ((retval=reghunt(docind)) < 0)
        {
            printf("Search for Regular Expressions failed; error %d\n",retval);
            return 0;
        }
/*
 * -    Loop - process the search nodes in evaluation order;
 *      (for processing of each node see nodemerge.c)
 */
        for (work_node = start_node;
                work_node != NULL;
                    work_node = work_node->next_eval_node)
            if ((retval= node_merge(work_node)) < 0)
            {
                printf("Node Merging failed; error %d\n",retval);
                return 0;
            }
            else  /* Frig to avoid the second pass */
            if ((work_node->node_type & IR_SEQ)
             && work_node->doc_count == 0
             && work_node->input_node != (struct search_node *) NULL
             && work_node->input_node->doc_count != 0)
                frig_node = work_node->input_node;
/*
 * -    Call the context hunter setter upper (if anything found)
 *      to build the tables to be passed to the browse module
 */
        if ((retval=con_set(last_eval_node)) < 0)
        {
            printf("Context Hunter setter upper failed; error %d\n",retval);
            return 0;
        }
/*
 * -    Write out the browse module tables, the list
 *      of items found summary report, and the list of items
 *      to be fed back into the next query.
 */
        cont_create();
        canchor = (struct prog_con *) NULL;
        if (fname != (char *) NULL)
        {
            fputs(fname, out_channel);
            fputc('\n', out_channel);
        }
#ifdef DEBUG
        printf("sizeof(retval):%d retval:%lx%lx\n",
              sizeof(retval), retval >> 32, retval);
        fflush(stdout);
#endif
        for (work_doc = (struct DOCCON_cont_header *) retval;
                work_doc != NULL;
                    work_doc = work_doc->forward_pos_control.next_cont_header)
        {
            found_flag++;
            canchor = new_prog_con(canchor, work_doc->this_doc);
#ifdef DEBUG
            dump_doc_list(stdout, "Final output\n", 1, &(work_doc->this_doc));
#endif
            cont_write(work_doc);
        }
        cont_close();
    }
    if (canchor == (struct prog_con *) NULL
     && frig_node != (struct search_node *) NULL)
    {
    doc_id * dp = frig_node->doc_list;
        found_flag = -1;
        fputs("Falling back to All documents...\n", out_channel);
        for (i = frig_node->doc_count; i > 0; i--)
             canchor = new_prog_con(canchor, *dp++);
    }
    frig_node = (struct search_node *) NULL;
    if (bin_root != (struct mem_con *) NULL)
        search_dismantle();
    for (nc = canchor; nc != (struct prog_con *) NULL; nc = nc->next)
        prog_print(out_channel,nc);
    if (hit_list != (struct prog_con **) NULL)
        *hit_list = canchor;
    else
        clear_progs(canchor);
    return found_flag;
}
/*
 * So we can see the SQL searched for
 */
void dump_sql_file(out_channel, fname)
FILE * out_channel;
char * fname;
{
FILE * in_chan = fopen(fname, "rb");
int i;
    if (in_chan != (FILE *) NULL)
    {
        setbuf(in_chan, NULL);
        while ((i = fread(buf,sizeof(char),sizeof(buf),in_chan)) > 0)
            (void) fwrite(buf,sizeof(char),i,out_channel);
        (void) fclose(in_chan);
    }
    fflush(out_channel);
    return;
}
/*******************************************************************************
 * Do a search for some SQL. The statement is in a file.
 */
int do_sql_search(fname, out_channel, hit_list)
char * fname;
FILE * out_channel;
struct prog_con ** hit_list;
{
int found_flag = 0;

    if (fname != (char *) NULL)
    {
    struct word_results * word_control;
    struct open_results * sql_object;
    int i;
/*
 * We have disabled the i == 1 processing because of the implementation of
 * search fall-back. Hence the strange loop below. We might change this if
 * we can think of a fuzzier match algorithm to use second bite. At present,
 * all we do is ignore words that do not crop up in the index at all.
 */
        for (i = 0; i < 1; i++)
        {
        int done_first = 0;
/*
 * We have two goes at it; once a tight look, the second time a more relaxed
 * look.
 */
            if ((sql_object=openbyname(fname))
                          == (struct open_results *) NULL
             ||  sql_object->doc_fd == -1)
            {
                fputs("Provide a valid input file name\n", stderr);
                return 0;
            }
            if ((yyin = fopen(stemp,"wb+")) == (FILE *) NULL)
            {
                perror("fopen(stemp)");
                fputs("Failed to open search file ", stderr);
                fputs(stemp, stderr);
                fputc('\n', stderr);
                return 0;
            }
            setbuf(yyin, buf);
/******************************************************************************
 * Loop - call get_words until EOF, adding the lengths and words to one buffer,
 * and pointers to their starts to the sort area.
 * - First time through, we look for the sequence (wrap in quotes)
 * - Second time through, just look for a file with all the words in it
 */
            if (i == 0)
                fputc('"', yyin);
            while ((word_control = (*sql_object->word_func)(sql_object)))
            {                        /* Loop until EOF on the input SQL */
                if (word_control->word_type != TEXTWORD)
                    continue;        /* Ignore paragraphs; should not happen */
                if (done_first)
                {
                    if (i == 1)
                        fputs(" &&\n", yyin);
                    else
                        fputc('\n', yyin);
                }
                else
                    done_first = 1;
                fwrite( word_control->word_ptr, sizeof(char),
                          (unsigned int) word_control->word_length, yyin);
            }
            if (!done_first)
            {
                fputs("Logic Error: ", out_channel);
                fputs(fname, out_channel);
                fputs(" has no search terms\n", out_channel);
                (*sql_object->close_func)(sql_object->doc_channel);
                fclose(yyin);
            }
            else
            {
                if (i == 0)
                    fputc('"', yyin);
                (*sql_object->close_func)(sql_object->doc_channel);
                fseek(yyin,0,0);
                if (found_flag = do_any_search(fname, out_channel, hit_list))
                {
                    dump_sql_file(out_channel,fname);
                    if (i == 0)
                        fputs("... Word Sequence Match\n", out_channel);
                    else
                        fputs("... All Words Match\n", out_channel);
                    break;        /* Exit if a match is found */
                }
            }
        }
    }
    else
        found_flag = do_any_search(fname, out_channel, hit_list);
    if (bin_root != (struct mem_con *) NULL)
        search_dismantle();
    return found_flag;
}
#ifdef STAND
/*******************************************************************************
 * Main Program
 * VVVVVVVVVVVV
 */
main(argc, argv, envp)
int argc;
char *argv[], *envp[];
{
int iter;

    (void) scan_setup(searchterms);       /* Read indexed words */
/*
 * Start by enabling SIGTERM trapping so that program will close files if killed
 */
     signal(sig,sigterm);
#ifdef DEBUG
#ifdef YYDEBUG
     yydebug = 1;
#endif
#endif
     dbminit(docidname);
/*
 * If we have been passed a SQL statement to search for, generate a search
 * condition from it. At present, we merely search for documents that have
 * all the search terms in them; dependent on how it works out in practice,
 * we may refine this in future.
 */
    if (argc < 2)
        do_sql_search((char *) NULL,stdout, NULL);
    else
    for (iter = 1; iter < argc; iter++)
    {
        do_sql_search(argv[iter], stdout, NULL);
    }
/*
 *  Then closedown
 */
    exit(0);
}
#endif
/*
 * Routine that traps SIGTERM signal, so that the program can be
 * elegantly stopped
 */
void sigterm()
{
    fputs("User aborted search\n", stderr);
    exit(2);
}
/*
 * Standard yacc error routine
 */
yyerror(s)
char *s;
{
static char buf[132];

    sprintf(buf,"Yacc Message %s\nLex Token %s\n",s,yytext);
    longjmp(parse_env, (unsigned long) buf);
/* Not Reached ! */
    return 0;
}
