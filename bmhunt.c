/************************************************************************
 * bmhunt.c - Search for a string in a list of files quickly.
 ***********************************************************************
 *This file is a Trade Secret of E2 Systems. It may not be executed,
 *copied or distributed, except under the terms of an E2 Systems
 *UNIX Instrumentation for CFACS licence. Presence of this file on your
 *system cannot be construed as evidence that you have such a licence.
 ***********************************************************************/
static unsigned char * sccs_id =
    (unsigned char *) "Copyright (c) E2 Systems Limited 1994\n\
@(#) $Name$ $Id$";
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
extern int getopt();
extern char *  optarg;
extern int optind;
/*
 * The first argument is the string, the others are the files to search
 */
int main(argc, argv)
int argc;
unsigned char ** argv;
{
int i;
FILE * fp;
int forw[256];
int len;
unsigned char *wrd;
register unsigned char *x;
int ch;
/*
 * Place holder. Useful options would be case-insensitivity and
 * white space same-ness. Need to think about the jump-length though
 */
    while ((ch = getopt(argc, argv, "h")) != EOF)
    {
        switch (ch)
        {
        case 'h':
        default:
            fputs( "Provide a match string and files to search\n\
Options (provide first) -i (case insensitive) and -w (ignore white space)\n",
                      stderr);
            exit(0);
        }
    }
    if (argc - optind < 1 )
        exit(1);
    len = strlen(argv[optind]);
    wrd = argv[optind];
    len--;
    if (len <= 0)
        exit(1);
/*
 * By default, skip forwards the length - 1
 */
    for (i = 0; i < 256; i++)
        forw[i] = len;
/*
 * For characters that are present;
 * - when first seen, skip back and start matching from the beginning
 * - when seen out of place in the match, skip ahead again
 */ 
    for (x = wrd; *x != '\0'; x++)
    {
        forw[*x] = len - ( x - wrd ) -1;
#ifdef DEBUG
        printf("%c forw %d\n", *x, forw[*x]);
#endif
    }
    for (i = optind + 1; i < argc; i++)
    {    

        if ((fp = fopen(argv[i], "rb")) != (FILE *) NULL)
        {
        register int k;

            while ((k = getc(fp)) != EOF)
            {
                if (forw[k] != -1)
                    fseek(fp,forw[k], 1);
                else
                {
                    fseek(fp,(-len -1), 1);
                    for (x = wrd; *x != '\0'; x++)
                    {
                        k = getc(fp);
#ifdef DEBUG
                        printf("Found %c at %u\n", (char) k, ftell(fp));
#endif
                        if (*x != k)
                            break;
                    }
                    if (*x == 0)
                    {
                        printf("Found %s in %s at %u\n", argv[optind],
                                argv[i], ftell(fp));
                    }
                    else
                    if (k == EOF)
                        break;
                    else
                        fseek(fp,len - (x - wrd), 1);
                }
            }
            (void) fclose(fp);
        }
    }
    exit(0);
}
