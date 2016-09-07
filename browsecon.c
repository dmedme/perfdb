/* browsecon.c - routines for assisting the browser step from context to
 * context.    %W% %G%
 *
 * Each document that is identified by the IRSearch module has a file
 * created for it. This file is named by the USER environment variable,
 * and the document number.
 *
 * The first record of the file contains the length of the file that has been
 * scanned for patterns, and a flag that indicates whether any more remain
 * to be found.
 *
 * The subsequent records of the file contain the start position and end
 * position for the expressions that have been found.
 *
 * Whenever the browser displays part of a document, it checks to see if
 *  -   It is required to display beyond the maximum searched
 *      extent, in which case the context hunter is invoked to
 *      look further ahead, to find the next context (if there is
 *      one)
 *  -   If it is required to display text that occurs between a
 *          start position and an end position, in which case the
 *      standout characteristics of termcap are used so that
 *      the user can see what is required.
 *
 * It is implicit in the following routines that browsing is only active
 * on a single file at a time. The header record is read when the file
 * becomes active, and is updated in memory as the file is extended. When
 * the user wishes to go to another file, the header is written back, and
 * the file is closed.
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
#include "IRfiles.h"
#include "conthunt.h"

union BROWSE_record current_rec;
union BROWSE_record updated;
FILE * browse_channel;
static char buf[BUFSIZ];

struct open_results * browse_object;
struct hunt_results * hunt_ret;
/*
 * Function to remove old browse control files
 */
int browse_unlink (doc_to_remove)
    doc_id doc_to_remove;
{
char file_name[128];
    sprintf(file_name,"b%u", doc_to_remove);
            /* Create the file name to delete */
    unlink (file_name);
            /* Do not worry about errors */
    return (0);
}
struct DOCCON_cont_header * browse_cont_header;
/*
 * Routine to initialise browse control for a document
 */
int browse_open(hunter_control)
struct DOCCON_cont_header * hunter_control;
{
char file_name[128];
    sprintf(file_name,"b%u",hunter_control->this_doc);
                             /* Create the file name to open */
/*
 * If this is the first time through, the browse control file must be created
 * first
 */
    while ((browse_channel = fopen(file_name,"rb+")) == NULL)
    {
        if ((browse_channel = fopen(file_name,"wb")) == NULL)
            return (-1);    /* Exit if cannot create */
        updated.header.last_looked = 0;
        updated.header.eof_reached = EOF_NOT_REACHED;
        setbuf(browse_channel,NULL);
        fwrite (&updated, sizeof (updated), 1, browse_channel);
        fclose (browse_channel);
    }
    setbuf(browse_channel,buf);
    if (fread (&updated, sizeof (updated), 1, browse_channel) < 1)
        return (-1);    /* Logic error if cannot read the header */
/*
 * Open the document itself, by document id, to set up for the
 * context hunting routines.
 */
    if ((browse_object = openid(hunter_control->this_doc)) == NULL)
        return (-1);    /* Logic error if document cannot be opened */
/*
 * The next piece of code is for the benefit of the hacked browser, which
 * fails to make use of stdio, never mind the Information Retrieval files
 * interface, and which needs the document name
 */
    if ((hunter_control->reg_control.doc_name =
        (char *) malloc(strlen(browse_object->doc_name) + 1)) == NULL)
        return (-1);
    strcpy(hunter_control->reg_control.doc_name,browse_object->doc_name);
/*
 * If there are no contexts yet
 */
    if (fread (&current_rec, sizeof (current_rec), 1, browse_channel) < 1)
    {
        hunt_ret = cont_hunt(0x7fffffff,browse_object,hunter_control);
        if (hunt_ret->type_of_answer    != FOUND)
            return (-1);    /* Logic error; there must be at
                             * least one context in every
                             * document! */
        current_rec.ordinary.start_pos = hunt_ret->start_pos;
        current_rec.ordinary.end_pos = hunt_ret->end_pos;
        updated.header.last_looked = current_rec.ordinary.end_pos;
        fseek(browse_channel,0,2);
        fwrite(&current_rec,sizeof(current_rec),1,browse_channel);
        fseek(browse_channel,0,2);
    }
    browse_cont_header = hunter_control;
    return (0);
}
/*
 * Get the next or the previous browse record, relative to the passed position
 * on the document file
 */
union BROWSE_record * browse_change(current_location, next_or_previous)
long current_location;
unsigned int next_or_previous;
{
int seek_interval;
/*
 * If "next", always attempt to extend by one, if we have not reached EOF.
 */
    if ((updated.header.eof_reached == EOF_NOT_REACHED)
              && (next_or_previous == NEXT))
/*
 * Seek to the last looked position in the document
 */
        if ((*browse_object->set_pos_func)(browse_object->doc_channel,
                  updated.header.last_looked,0) == -1)
            updated.header.eof_reached = EOF_REACHED;
        else
        {
            hunt_ret = cont_hunt(0x7fffffff,
            browse_object,browse_cont_header);
/*
 * If not found
 */
            if (hunt_ret->type_of_answer != FOUND)
                updated.header.eof_reached = EOF_REACHED;
            else
/*
 * Found a match
 */
            {
                updated.header.last_looked = hunt_ret->end_pos;
/*
 * Find the end of the browse control file
 */
                fseek (browse_channel,0,2);
                current_rec.ordinary.start_pos = hunt_ret->start_pos;
                current_rec.ordinary.end_pos   = hunt_ret->end_pos;
                fwrite(&current_rec,sizeof(current_rec),1,browse_channel);
                fseek(browse_channel,0,2);
            }
        }
/*
 * The current location lies between the last context in the document, and
 * the document end of file, and the user asked for next; return an indication
 * that there is none.
 */
    if ((current_location > updated.header.last_looked) &&
                (updated.header.eof_reached == EOF_REACHED)
            && (next_or_previous == NEXT))
        return (NULL);
/*
 * If the current record is the first record
 */
    if (ftell(browse_channel) <= 2*sizeof(current_rec))
        if (next_or_previous == PREVIOUS)
        {
            if (current_location < current_rec.ordinary.end_pos)
                return (NULL);
            else
                return &current_rec;
        }
        else    /* Next */
        if (current_location < current_rec.ordinary.start_pos)
            return &current_rec; /* The first record is the current. */
/*
 * We need to search for the next or previous context.  The next or the
 * previous context is respectively the first record lying completely before
 * or completely beyond the passed current location.
 *
 * If the current record may need to move forwards
 */
    if (current_rec.ordinary.end_pos < current_location)
    {
/*
 * Loop - serially search forwards until the record after
 * the beginning of the required range has been found
 */
        do
        {
/*
 * If we have reached EOF
 */
            if (fread(&current_rec,sizeof(current_rec),1,
                         browse_channel) < 1)
            {
                fseek(browse_channel,-((int) sizeof(current_rec)),2);
                fread(&current_rec,sizeof(current_rec),1, browse_channel);
                if (next_or_previous == NEXT)
                    return (NULL);
                else
                    return &current_rec;
            }
        }
        while (current_location > current_rec.ordinary.end_pos);
        if (next_or_previous == NEXT)
        {
/*
 * We have reached EOF
 */
            if ((current_location > current_rec.ordinary.start_pos)
              && (fread(&current_rec,sizeof(current_rec),1,
                               browse_channel)< 1))
            {
                fseek(browse_channel,-((int) sizeof(current_rec)),2);
                fread(&current_rec,sizeof(current_rec),1, browse_channel);
                return (NULL);
            }
        }
        else
        {
            fseek(browse_channel,-2*((int) sizeof(current_rec)),1);
            fread(&current_rec,sizeof(current_rec),1,browse_channel);
        }
    }
    else
/*
 * The current record needs to move backwards
 */
    {
        seek_interval = -2*sizeof(current_rec);
/*
 * Loop - serially search backwards until the record before the beginning of
 * the range has been found
 */
        do
        {
/*
 * If the current record is the first record; exit with success if we wanted
 * the next record, otherwise exit with failure
 */
            if (ftell(browse_channel) <= -seek_interval)
            {
                if (next_or_previous== PREVIOUS)
                    return (NULL);
                else
                    return &current_rec;
            }
/*
 * The earlier logic guarantees that this is not the first record
 */
            fseek(browse_channel,seek_interval,1);
            fread(&current_rec,sizeof(current_rec),1,browse_channel);
        }
        while (current_location <= current_rec.ordinary.start_pos);
        if ((current_location <= current_rec.ordinary.end_pos) &&
          (next_or_previous== PREVIOUS))
        {
            if (ftell(browse_channel) <= -seek_interval)
                return NULL;
            fseek(browse_channel,seek_interval,1);
            fread(&current_rec,sizeof(current_rec),1,browse_channel);
        }
        else
        if (next_or_previous== NEXT)
            if (fread(&current_rec,sizeof(current_rec),1,
                       browse_channel) < 1)
            {
                fseek(browse_channel,-((int) sizeof(current_rec)),2);
                fread(&current_rec,sizeof(current_rec),1,browse_channel);
                return NULL;
            }
    }
    return &current_rec;
}
/*
 * Function that provides a pointer to the first context in the range
 * first_location to last_location. The calling program should call the
 * routine repeatedly until the routine returns a null pointer, advancing its
 * first_location after each call.
 */
union BROWSE_record * browse_identify(first_location,last_location)
long first_location;
long last_location;
{
static union BROWSE_record browse_results;
int seek_interval;
/*
 * If the last location is beyond the bounds of
 * the current search, and we have not reached EOF,
 */
    if ((last_location > updated.header.last_looked) &&
                (updated.header.eof_reached == EOF_NOT_REACHED))
/*
 * Seek to the last looked position
 */
        if ((*browse_object->set_pos_func)(browse_object->doc_channel,
               updated.header.last_looked,0) == -1)
            updated.header.eof_reached = EOF_REACHED;
        else
        {
            hunt_ret = cont_hunt(last_location + 2048,
                                    browse_object,browse_cont_header);
/*
 * If a match has been found
 */
            if (hunt_ret->type_of_answer == FOUND)
            {
                updated.header.last_looked = hunt_ret->end_pos;
/*
 * Find the end of the browse control file
 */
                fseek (browse_channel,0,2);
                current_rec.ordinary.start_pos = hunt_ret->start_pos;
                current_rec.ordinary.end_pos   = hunt_ret->end_pos;
                fwrite(&current_rec,sizeof(current_rec),1,browse_channel);
                fseek(browse_channel,0,2);
            }
            else
                updated.header.last_looked = last_location + 2048;
        }
/*
 * If the first location lies between the last context in the document, and
 * the document end of file
 */
    if ((first_location > updated.header.last_looked) &&
                 (updated.header.eof_reached == EOF_REACHED))
        return (NULL);
    else
/*
 * It is conceivable that a context includes the calling range
 */
    {
/*
 * If the start location occurs in the range of the current browse record, the
 * current record is the record wanted
 */
        if ((current_rec.ordinary.start_pos <= first_location)
                && (current_rec.ordinary.end_pos >= first_location))
        {
            browse_results.ordinary.start_pos = first_location;
            browse_results.ordinary.end_pos =
                    (last_location > current_rec.ordinary.end_pos) ? 
                         current_rec.ordinary.end_pos
                       : last_location;
            return &browse_results;
        }
        else
/*
 * If the current record needs to move forwards
 */
        if (current_rec.ordinary.start_pos < first_location)
        {
/*
 * Loop - serially search forwards until the record after the beginning of the
 * required range has been found
 */
            do
            {
/*
 * If we have reached EOF
 */
                if (fread(&current_rec,sizeof(current_rec),1,
                                browse_channel) < 1)
                {
                    fseek(browse_channel,-((int) sizeof(current_rec)),2);
                    fread(&current_rec,sizeof(current_rec),1,browse_channel);
                    return NULL;
                }
            }
            while (first_location > current_rec.ordinary.end_pos);
/*
 * If no part of the range is a context
 */
            if (last_location < current_rec.ordinary.start_pos)
                return (NULL);
        }
        else
        {
            seek_interval = -2*sizeof(current_rec);
/*
 * Loop - serially search backwards until the record before the beginning of
 * the range has been found
 */
            do
            {
/*
 * Current record is the first record; evaluate this
 */
                if (ftell(browse_channel) <= -seek_interval)
                    if (last_location < current_rec.ordinary.start_pos)
                        return (NULL);
                    else
                        break;      
                fseek(browse_channel,seek_interval,1);
                fread(&current_rec,sizeof(current_rec),1,browse_channel);
            }
            while (first_location < current_rec.ordinary.start_pos);
            if (first_location > current_rec.ordinary.end_pos)
                if (fread(&current_rec,sizeof(current_rec),1,browse_channel)
                              < 1)
                {
                    fseek(browse_channel,-((int) sizeof(current_rec)),2);
                    fread(&current_rec,sizeof(current_rec),1,browse_channel);
                    return NULL;
                }
                if (last_location < current_rec.ordinary.start_pos)
                    return (NULL);      
        }
    }
/*
 * At this point, the current_rec holds the lowest matching position;
 * chop the bounds to be within the passed range
 */
    browse_results.ordinary.start_pos = (first_location <
                    current_rec.ordinary.start_pos)
                  ? current_rec.ordinary.start_pos
                  : first_location;
    browse_results.ordinary.end_pos = (last_location >
                    current_rec.ordinary.end_pos)
                    ? current_rec.ordinary.end_pos
                  : last_location;
    return &browse_results;
}
/*
 * Function to update the header and to close a browse control file
 */
int browse_close()
{
    if (browse_channel == NULL)
        return (0);
    fseek(browse_channel,0,0);
    fwrite(&updated, sizeof(updated), 1, browse_channel);
    fclose(browse_channel);
    fclose(browse_object->doc_channel);
    browse_channel = NULL;
    return (0);
}
