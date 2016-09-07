/******************************************************************************
 *  getfiles.c %W% %G% - module to produce an alphasorted list of all files
 *  that are not directories, descended from a given path.
 *
 *  Used to identify the files that need to be updated during an index run. 
 */
static char * sccs_id="@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1998";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef VCC2003
#define NEED_DIR_STUFF
#endif
#ifdef LCC
#define NEED_DIR_STUFF
#endif
#ifdef NEED_DIR_STUFF
/*
 * Need to provide an implementation of opendir(), readdir(), closedir()
 */
#include <windows.h>
#include <io.h>
#ifndef S_ISDIR
#define S_ISDIR(x) (x & S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(x) (x & S_IFREG)
#endif
struct dirent {
    struct _finddata_t f;
    int read_cnt;
    long handle;
};
typedef struct dirent DIR;
#define d_name f.name

DIR * opendir(fname)
char * fname;
{
char buf[260];
DIR * d = (struct dirent *) malloc(sizeof(struct dirent));

    strcpy(buf, fname);
    strcat(buf, "/*");
    if ((d->handle = _findfirst(buf, &(d->f))) < 0)
    {
        fprintf(stderr,"Failed looking for %s error %d\n",
                          buf, GetLastError());
        fflush(stderr);
        free((char *) d);
        return (DIR *) NULL;
    }
    d->read_cnt = 0;
    return d;
}
struct dirent * readdir(d)
DIR * d;
{
    if (d->read_cnt == 0)
    {
        d->read_cnt = 1;
        return d;
    }
    if (_findnext(d->handle, &(d->f)) < 0)
        return (struct dirent *) NULL;
    return d;
}
void closedir(d)
DIR *d;
{
    _findclose(d->handle);
    free((char *) d);
    return;
}
#else
#include <unistd.h>
#endif
#include <errno.h>
#include "IRfiles.h"

extern char * getcwd();
extern char * filename();

/*
 * Details directory entries found.
 */
typedef struct _path_det {
char * path_name;              /* Full path name of the file */
struct _path_det * next_path;
} PATH;
static PATH * getpathbyfile();
/***************************************************************************
 * Implementation of scan_dir.
 *
 * getnextpath()
 * - Function reads the next path
 * - Recursively follows sub-directories.
 */
static PATH *
getpaths(path_chan,dir_name,path)
DIR * path_chan;
char * dir_name;
PATH * path;
{
#ifdef POSIX
    struct dirent * path_dir;
#else
    struct direct * path_dir;
#endif
#ifdef POSIX
    while ((path_dir = readdir(path_chan)) != (struct dirent *) NULL)
#else
    while ((path_dir = readdir(path_chan)) != (struct direct *) NULL)
#endif
        if (strcmp(path_dir->d_name,".") &&
            strcmp(path_dir->d_name,".."))
        path = getpathbyfile(path_dir->d_name,dir_name,path);
    return path;
}
/*
 * getpathbyfile()
 * Routine to check if a file is not a directory, and if so to
 * set up the PATH record.
 */
static PATH * getpathbyfile(file_name,dir_name,path)
char * file_name;
char * dir_name;
PATH * path;
{
struct stat path_stat;
PATH * ret_path;
char * path_to_open;

    path_to_open = filename("%s/%s",dir_name,file_name);   
    if (stat(path_to_open,&path_stat) < 0)
    {
#ifdef DEBUG
        perror("stat() on file failed");
#endif
        free(path_to_open);
        return path;     /* not a path file */
    }
#ifdef DEBUG
    fprintf(stderr, "%s/%s\n",dir_name,file_name);
#endif
    if (S_ISDIR(path_stat.st_mode))
    {                             /* Recurse if a directory */
    DIR * path_chan;

        if ((path_chan=opendir(path_to_open)) == (DIR *) NULL) 
        {
            perror("Cannot open the sub-directory");
            return path;
        }
        path = getpaths(path_chan,path_to_open,path);
        (void) closedir(path_chan);
        (void) free(path_to_open);
    }
    else
    if (S_ISREG(path_stat.st_mode))
    {
        ret_path = (PATH *) malloc(sizeof(struct _path_det));
        ret_path->path_name = path_to_open;
        ret_path->next_path = path;
        path = ret_path;
    }
    return path;
}
/*********************************************************************
 * Routine to initialise the list of files in a directory. 
 * As a side-effect, this directory becomes the current directory.
 */
static int e2scandir(dir_name,name_base)
char * dir_name;
char *** name_base;
{
PATH * path, *p;
DIR * path_chan;
int fcnt;
char **x;

    path = (PATH *) NULL;
    if ((path_chan=opendir(dir_name)) == (DIR *) NULL) 
    {
        perror("Cannot open the sub-directory");
        return 0;
    }
    path = getpaths(path_chan,dir_name,path);
    (void) closedir(path_chan);
/*
 * Find out how many there are
 */
    for (fcnt=0, p = path; p != (PATH *) NULL; p=p->next_path, fcnt++);
/*
 * Allocate space for a pointer list.
 */
    *name_base = (char **) malloc(sizeof(char *) * fcnt);
/*
 * Ferret them away.
 */
    for (x = *name_base, p = path;
             p != (PATH *) NULL;
                 *x = p->path_name, x++,p=p->next_path);
/*
 * Sort them
 */
    qwork(*name_base, fcnt, strcmp);
/*
 * Free up space
 */
    for (p = path; p != (PATH *) NULL; )
    {
        path = p;
        p = p->next_path;
        free((char *) path);
    }
    return fcnt;
}
char work_direct[MAXPATHLEN];
/*
 * Routine to produce a list of files in alpha order, for comparison
 * with those processed by the previous index run
 */
int alpha_files(out_channel,pathname)
FILE * out_channel;    /* channel to write the data out to */
char * pathname;       /* directory to process */
{
int i;
char **name_base, ** work_name;
struct stat buf;
struct DOCMEMrecord outr;

    outr.this_doc = 0;
    for (i = e2scandir(pathname,&name_base), work_name = name_base;
            i > 0;
                 i--, work_name++)
    {                       /*    loop; process each directory entry */
        if (**work_name == '.')
            continue;      /* ignore files that start with a "." */
        if (stat(*work_name,&buf) < 0)
            continue;      /* ignore files that cannot be stat()'ed */
        outr.mtime = buf.st_mtime;
        outr.fsize = buf.st_size;
        strcpy(outr.fname,*work_name);
        WRITEMEM(out_channel, outr);
        free(*work_name);
    }
    return(0);
}
static char buf[BUFSIZ];
/******************************************************************************
 * Routine that returns an output channel positioned to the start of a list of
 * the files in a given directory, in a file that is named out_name.
 ******************************************************************************
 */
FILE * get_files (direct_name, out_name)
char * direct_name;
char * out_name;
{
register char * x1_ptr, * x2_ptr;
FILE * output_channel;

    if (*direct_name != '/' && *(direct_name + 1) != ':')
    {
        x2_ptr = (work_direct+strlen(getcwd(work_direct,sizeof(work_direct))));
        *x2_ptr++ = '/';
    }
    else
        x2_ptr = work_direct;

    for (x1_ptr = direct_name; *x1_ptr != '\0'; *x2_ptr++ = *x1_ptr ++) ;

    *x2_ptr = '\0';

    output_channel =  fopen(out_name,"wb+");

    if (output_channel == NULL)
    {    /* logic error; cannot create member file */
        fputs("Cannot create list of members! Aborting\n", stderr);
        return (FILE *) NULL;
    }
    setbuf(output_channel,buf);
    alpha_files(output_channel,work_direct);
    fseek(output_channel,0,0);
    return output_channel;
}
