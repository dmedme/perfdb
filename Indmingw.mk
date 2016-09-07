# 
# Indmingw.mk
# @(#) $Name$ $Id$
# Copyright (c) E2 Systems Limited 1993
#
# Makefile - Command file for "make" to compile DME's new programs
#
# Usage: make
#
# NOTE:   ORACLE_HOME must be either:
#		. set in the user's environment
#		. passed in on the command line
#		. defined in a modified version of this makefile
#
#******************************************************************
INCS=-I. -I../e2common
CFLAGS=-DINCLUDE_EARLY -DSTAND -DPOSIX -O4 $(INCS) -DNT4 -DMINGW32 -DPATH_AT -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DNOBPF_H -DNOTCP_H -DNOETHER_H -D_WIN32 -DWIN32 -DNOIP_H -DNOIP_ICMP_H
# ************************************************************************
.SUFFIXES: .c .o .y
.c.o:
	$(CC) $(CFLAGS) -c $<
.y.c:
	bison -d $<
	mv $*.tab.c $@
	mv $*.tab.h cscalc.h
.y.o:
	bison -d $<
	mv $*.tab.c $*.c
	mv $*.tab.h cscalc.h
	$(CC) $(CFLAGS) -c $*.c
# ************************************************************************
# Usual suspects
#
CC= c:/mingw/bin/gcc
RC = c:/lcc/bin/lrc
VCC= c:/mingw/bin/gcc
XPGCC= c:/mingw/bin/gcc
RANLIB = c:/mingw/bin/ranlib
AR = c:/mingw/bin/ar rv
LD= c:/mingw/bin/gcc
YACC=bison
LEX=flex -l
# ************************************************************************
CLIBS=IRsalib.a ../e2common/comlib.a ../gdbm/libgdbm.a -Lc:/mingw/lib -lmingw32 -lws2_32 -ladvapi32 -lgdi32 -luser32 -lshell32 -lkernel32 -lmsvcrt -lntdll
##########################################################################
# The executables that are built
##########################################################################
# Database-unaware document indexing, as it was in the beginning
# **********************************************************************
EXES = IRindgen IRsearch

all: $(EXES)
	@echo "Performance Database Programs are Up To Date"

#
# For production:
# - Take out the -DDEBUG
# - (optional) change -gx to -O
#
################
#*************************************************************************
# Main Perfdb components
# VVVVVVVVVVVVVVVVVVVVVV
IRindgen: IRindgen.o IRsalib.a
	$(CC) $(LDFLAGS) -o $@ IRindgen.o $(CLIBS)
IRsearch: IRsearch.o IRsalib.a
	$(CC) $(LDFLAGS) -o $@ IRsearch.o $(CLIBS)
#*************************************************************************
# The Information Retrieval Stuff
# VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
# Compile time options
##########################################################################
# SLEX recognises the tokens in the Search Grammar
# ************************************************************************
SLEX = IRsilex.l
#		alternative (case sensitive) is IRslex.l
# ELEX recognises English Words for Indexing.
# ************************************************************************
# These routines are too slow on junk binary files containing little of
# interest. englex.c is a hopefully faster replacement.
# instead.
#ELEX = IRxienglex.l
#		alternative (case sensitive) is IRxenglex.l
#
##########################################################################
# The Information Retrieval Routines
##########################################################################
IRsalib.a: conthunt.o IRreghunt.o nodemerge.o IRfilenames.o cntserv.o \
         browsecon.o tempcreate.o getfiles.o mergeoff.o mergecurr.o \
         senlex.o orlex.o englex.o
	$(AR) $@ $?
	$(RANLIB) $@
##########################################################################
# IRsearch, the query processor
##########################################################################
IRsearch.o: IRslex.c IRsearch.c IRsearch.h IRfiles.h conthunt.h \
            orlex.h

IRsearch.c: IRsearch.y
	$(YACC) IRsearch.y
#	sed -e s/yy/yy_ir_/g y.tab.c > IRsearch.c
	sed -e s/yy/yy_ir_/g IRsearch.tab.c > IRsearch.c
	rm IRsearch.tab.c
#	rm y.tab.c

IRslex.c: $(SLEX)
	$(LEX) $(SLEX)
	sed -e s/yy/yy_ir_/g -e s/lex.yy_ir_.c/IRslex.c/ lex.yy.c > IRslex.c
	rm lex.yy.c

#IRxenglex.c: $(ELEX)
#	$(LEX) $(ELEX)
#	sed -e 's/yylex/struct word_results * eng_get_word/g' -e 's/int struct/struct/g' -e 's/yy/IRw/g' -e 's/IRwwrap/yywrap/g' -e 's/lex.IRw.c/IRxenglex.c/' lex.yy.c > IRxenglex.c
#	rm lex.yy.c

##########################################################################
# IRindgen, the index generator
##########################################################################
IRindgen.o: IRfiles.h mergecon.h orlex.h

##########################################################################
# The IR service routines
##########################################################################
browsecon.o: IRfiles.h conthunt.h

IRreghunt.o:IRsearch.h IRfiles.h

conthunt.o:IRsearch.h IRfiles.h conthunt.h

getfiles.o:IRfiles.h

tempcreate.o:IRfiles.h mergecon.h

mergecurr.o mergecurr.o:IRfiles.h mergecon.h

nodemerge.o:IRsearch.h IRfiles.h conthunt.h
##########################################################################
# Other random utilities
# VVVVVVVVVVVVVVVVVVVVVV
clean:
	rm -f *.o *.a IRslex.c IRxenglex.o IRsearch.c $(EXES)
