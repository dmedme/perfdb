#
# Makefile for Microsoft NT-4 in native mode
#
# Copyright (c) E2 Systems 1994. All Rights Reserved.
# @(#) $Name$ $Id$
#
#
YACC=byacc
LIBDIR=/cygnus/cygwin-b20/H-i586-cygwin32/lib
INCS=-I. -I../e2net -I../e2common -I/oracle/v80/oci80 -I/cygnus/cygwin-b20/H-i586-cygwin32/i586-cygwin32/include/mingw32
LIBS=IRlib.a ../native/e2comnt/comlib.a oracle/libgdbm.a -luser32 -lkernel32 -lcrtdll
CFLAGS=-DORV7 -DNOETHER_H -DNOIP_H -DNOTCP_H -DMINGW32 -DPOSIX -DNT4 -DAT -g $(INCS) -L$(LIBDIR) -L/cygnus/cygwin-b20/H-i586-cygwin32/lib/gcc-lib/i586-cygwin32/egcs-2.91.57  -fwritable-strings -mno-cygwin

AR = ar rv
RANLIB = ar ts
VCC = gcc
CC = gcc
XPGCC = gcc
#
#OCILDLIBS=/oracle/v80/oci80/ora803.lib
#OCILDLIBS=ora803.a
OCILDLIBS=oracle/libora73.a
#*************************************************************************
# Main Perfdb components
# VVVVVVVVVVVVVVVVVVVVVV
all: badsort tabdiff fhunt wordhunt
#all: badsort e2com e2sub e2cap
	@echo "Performance Database Programs are Up To Date"

simproc:simproc.c
	$(CC) $(CFLAGS) -o $@ simproc.c ../e2common/e2sort.c $(LIBS)
sarprep:sarprep.c
	$(CC) $(CFLAGS) -DCUTDOWN -DOSF -o $@ sarprep.c ../e2common/e2sort.c $(LIBS)
nsarprep:sarprep.c nmalloc.c
	$(CC) $(CFLAGS) -DCUTDOWN -DOSF -o $@ sarprep.c nmalloc.c ../e2common/e2sort.c $(LIBS)
usehist:usehist.c
	$(CC) $(CFLAGS) -o $@ usehist.c  $(LIBS)
testbed:testbed.o IRlib.a perf.a
	$(CC) $(CFLAGS) -o $@ testbed.o IRlib.a perf.a $(OCILDLIBS) $(LIBS)
tabdiff: tabdiff.o perf.a
	$(CC) $(CFLAGS) -o $@ tabdiff.o perf.a $(OCILDLIBS) $(LIBS)
badsort: badsort.o badsql.o e2orant.o e2orant_res.o perf.a IRlib.a
	$(CC) $(CFLAGS) -o $@ badsort.o badsql.o e2orant.o e2orant_res.o perf.a $(OCILDLIBS) $(LIBS)
e2com: e2com.o e2clib.o perf.a
	$(CC) $(CFLAGS) -o $@ e2com.o e2clib.o perf.a $(LIBS)

e2sub: e2sub.o perf.a
	$(CC) $(CFLAGS) -o $@ e2sub.o perf.a $(LIBS)

.c.o: siinrec.h perf.h
	$(CC) $(CFLAGS) -c $*.c

e2cap: e2cap.o perflib.o perf.a
	$(CC) $(CFLAGS) -o $@ e2cap.o perflib.o perf.a $(OCILDLIBS) $(LIBS)

perf.a: siinrec.o e2sqllib.o e2srclib.o e2oralib.o cspars.o csexe.o
	$(AR) $@ $?
	$(RANLIB) $@
csexe.o: csexe.c
	$(CC) $(CFLAGS) -c csexe.c
cspars.o: cspars.c
	$(CC) $(CFLAGS) -c cspars.c
e2oralib.o: e2sqllib.c
	$(CC) $(CFLAGS) -c e2oralib.c
e2sqllib.o: e2sqllib.c
	$(CC) $(CFLAGS) -c e2sqllib.c
e2srclib.o: e2srclib.c
	$(CC) $(CFLAGS) -c e2srclib.c
#**************************************************************************
# Specific NT stuff
#**************************************************************************
RC=/cygnus/win32/lcc/bin/lrc
e2orant.o: e2orant.c
	$(CC) $(CFLAGS) -c e2orant.c

e2orant_res.o: e2orant.rc e2orant.h
	$(RC) e2orant.rc
	windres -i e2orant.res -o e2orant_res.o
#*************************************************************************
# The Information Retrieval Stuff
# VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
# Compile time options
##########################################################################
# SLEX recognises the tokens in the Search Grammar
SLEX = IRslex.l
#		alternative (case sensitive) is IRsilex.l
# ELEX recognises English Words for Indexing. It is not used; orlex is used
# instead.
ELEX = IRxenglex.l
#		alternative (case insensitive) is IRxienglex.l
#
# Microsoft NT.4
#
LDFLAGS=$(LIBS)
RANLIB = ranlib
AR = ar rv
VCC = gcc
CC = gcc
XPGCC = gcc
YACC=byacc
LEX=flex -l
##########################################################################
# The Information Retrieval Routines
##########################################################################
IRlib.a: conthunt.o IRreghunt.o nodemerge.o IRfilenames.o orlex.o cntserv.o\
         browsecon.o tempcreate.o getfiles.o mergeoff.o mergecurr.o \
         IRsearch.o IRindgen.o getprocs.o senlex.o
	$(AR) $@ $?
	$(RANLIB) $@
##########################################################################
# IRsearch, the query processor
##########################################################################
IRsearch.o: IRslex.c IRsearch.c IRsearch.h IRfiles.h conthunt.h \
            orlex.h

IRsearch.c: IRsearch.y
	$(YACC) IRsearch.y
	sed -e s/yy/yy_ir_/g y.tab.c > IRsearch.c
	rm y.tab.c

IRslex.c: $(SLEX)
	$(LEX) $(SLEX)
	sed -e s/yy/yy_ir_/g lex.yy.c > IRslex.c
	rm lex.yy.c

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

senlex.o:IRfiles.h orlex.h

getfiles.o:IRfiles.h

mergecurr.o mergecurr.o tempcreate.o:IRfiles.h mergecon.h

nodemerge.o:IRsearch.h IRfiles.h conthunt.h

##########################################################################
# The test bed for the tokenising routines
##########################################################################
wordhunt: wordhunt.c orlex.h IRlib.a
	$(CC) -o wordhunt $(CFLAGS) wordhunt.c $(LIBS)

fhunt: fhunt.c
	$(CC) -o fhunt $(CFLAGS) fhunt.c $(LIBS)

##########################################################################
# Other random utilities
# VVVVVVVVVVVVVVVVVVVVVV
nt_util: relay getdpass
	@echo 'Make finished'
getdpass: getdpass.c
	$(CC) -o getdpass $(CFLAGS) getdpass.c $(LDFLAGS) $(LIBS) fred.a
relay: relay.c
	$(CC) $(CFLAGS) -o relay relay.c $(LIBS)
back_ready:
	rm -f *.o *.a IRslex.c IRsearch.c cspars.c *.exe
backup:
	tar cf ../perfdb.tar IRdocini P* *.l *.y *.c *.s* *.h *.awk *.trg
