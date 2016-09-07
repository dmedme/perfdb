/*  getprocs.c %W% %G% - module to produce an alphasorted list of all stored
procedures, functions and packages.

Used to identify the procedures that need to be updated during an index run. 
*/
#include <stdio.h>
#include <stdlib.h>
#ifndef LCC
#ifndef VCC2003
#include <unistd.h>
#endif
#endif
#include <errno.h>
#include "IRfiles.h"
#include "tabdiff.h"

/**************************************************************************
 * ORACLE Elements. The database connection is opened elsewhere.
 *
 * SQL statement (fragments)
 */
static struct sess_con * gcon;
#define CURS_PROCS 40
static struct dyn_con * dprocs;
#ifdef SYBASE
static char * sprocs = "select a.name + '.' + b.name + '.' + a.type, a.id,\n\
datediff(mi,'1 jan 1970',a.cr_date)*60.0 from sysobjects a,\n\
     sysusers b\n\
where a.type in ('P ','R ','RI', 'TR', 'V','XP') and a.uid = b.id order by 1";
#else
static char * sprocs_7 = "select decode(a.type,7,'PROCEDURE',8,'FUNCTION',9,\n\
'PACKAGE',11,'BODY',12,'TRIGGER','UNKNOWN')||'.'||b.name||'.'||a.name,\n\
       a.obj#,\n\
       86400*(a.mtime - to_date('01-JAN-1970','DD-MON-YYYY'))\n\
from sys.obj$ a,\n\
     sys.user$ b\n\
where a.type in (7,8,9,11,12) and a.owner# = b.user# order by 1";
static char * sprocs = "select decode(a.type#,7,'PROCEDURE',8,'FUNCTION',9,\n\
'PACKAGE',11,'BODY',12,'TRIGGER','UNKNOWN')||'.'||b.name||'.'||a.name,\n\
       a.obj#,\n\
       86400*(a.mtime - to_date('01-JAN-1970','DD-MON-YYYY'))\n\
from sys.obj$ a,\n\
     sys.user$ b\n\
where a.type# in (7,8,9,11,12) and a.owner# = b.user# order by 1";
#endif
#define CURS_SOURCE 41
static struct dyn_con * dsource;
#ifdef SYBASE
static char * ssource="select text from syscomments where id = @obj_id\n\
order by number, colid2, colid";
#else
static char * ssource=
   "select source from sys.source$ where obj#=:obj_id order by line";
#endif
/****************************************************************************
 * Set up the SQL statements
 */
static void open_all_sql(in_con)
struct sess_con * in_con; 
{
    set_def_binds(1,20);
    if (in_con != (struct sess_con *) NULL)
    {
        gcon = in_con;
        curse_parse(gcon, &dprocs, CURS_PROCS, sprocs) ;
        dprocs->is_sel = 1;
        curse_parse(gcon, &dsource, CURS_SOURCE, ssource) ;
        dsource->is_sel = 1;
    }
    return;
}
/****************************************************************************
 * Load the bind variables into a dictionary
 */
static void load_dict()
{
struct dict_con * dict = get_cur_dict();
    if (dict == (struct dict_con *) NULL)
    {
        dict = new_dict( 20);
        (void) set_cur_dict(dict);
        set_long(16384);
    }
#ifdef SYBASE
    dict_add(dict,"@obj_id",ORA_INTEGER,  sizeof(long));
#else
    dict_add(dict,":obj_id",ORA_INTEGER,  sizeof(long));
#endif
    return;
}
/*
 * Details directory entries found.
 */
typedef struct _path_det {
char * path_name;              /* Full path name of the file */
struct _path_det * next_path;
} PATH;
static PATH * getpathbyfile();
/*
 * Routine to produce a list of procs in alpha order, for comparison
 * with those processed by the previous index run. This approach gives
 * maximum compatibility between files and ORACLE procedures.
 */
int alpha_procs(out_channel, in_con)
FILE * out_channel;          /* channel to write the data out to */
struct sess_con * in_con;    /* ORACLE channel */
{
struct DOCMEMrecord outr;
char *p[3];
int l[3];

    outr.this_doc = 0;
    if (gcon == (struct sess_con *) NULL)
    {
        load_dict();
        open_all_sql(in_con);
    }
    exec_dml(dprocs);
    if (gcon->ret_status != 0)
    {
        scarper(__FILE__,__LINE__,"Unexpected Error fetching procedures");
#ifdef SYBASE
        return 0;
#else
        curse_parse(gcon, &dprocs, CURS_PROCS, sprocs_7);
        dprocs->is_sel = 1;
        exec_dml(dprocs);
        if (gcon->ret_status != 0)
        {
            scarper(__FILE__,__LINE__,"Unexpected Error fetching procedures");
            return 0;
        }
#endif
    }
    dprocs->so_far = 0;
    while (dyn_locate(dprocs,&(l[0]),&(p[0])))
    {
        memcpy(&outr.fname[0],p[0],l[0]);
        outr.fname[l[0]] = '\0';
        outr.fsize = atoi(p[1]);
        outr.mtime = atoi(p[2]);
        WRITEMEM(out_channel, outr);
    }
    dyn_reset(dprocs);
    return(1);
}
static char buf[BUFSIZ];
static char buf1[BUFSIZ];
/******************************************************************************
 * Routine that returns an output channel positioned to the start of a list of
 * the files in a given directory, in a file that is named out_name.
 */
FILE * get_procs(in_con, out_name)
struct sess_con * in_con;
char * out_name;
{
FILE * output_channel =  fopen (out_name,"wb+");

    if (output_channel == (FILE *) NULL)
    {    /* logic error; cannot create member file */
         puts("Cannot create list of procedures! Aborting\n");
         return (FILE *) NULL;
    }
    setbuf(output_channel,buf);
    alpha_procs(output_channel,in_con);
    fseek(output_channel,0,0);
    return output_channel;
}
/******************************************************************************
 * Routine that returns an output channel positioned to the start of the source
 * of a stored procedure, in a file that is named out_name.
 */
FILE * get_source(obj_id, out_name)
long int obj_id;
char * out_name;
{
char *p;
int l;
FILE * output_channel =  fopen (out_name,"wb+");

    if (output_channel == (FILE *) NULL)
    {    /* logic error; cannot create SQL source file */
         puts("Cannot create file of source! Aborting\n");
         return (FILE *) NULL;
    }
    setbuf(output_channel,buf1);
    add_bind(dsource, E2FLONG, sizeof(long int), &obj_id);
    exec_dml(dsource);
    if (gcon->ret_status != 0)
    {
#ifdef DEBUG
        fprintf(stderr,"gcon: %x dsource->con: %x status: %d con->status\n",
           (long) gcon, (long) dsource->con, dsource->ret_status, dsource->con->ret_status);
#endif
        scarper(__FILE__,__LINE__,"Unexpected Error fetching procedures");
        fclose(output_channel);
        return (FILE *) NULL;
    }
    dsource->so_far = 0;
    while (dyn_locate(dsource,&l,&p))
        fwrite(p, sizeof(char), l,output_channel);
    dyn_reset(dsource);
    fseek(output_channel,0,0);
    return output_channel;
}
