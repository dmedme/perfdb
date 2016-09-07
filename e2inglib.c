 /************************************************************************
 * e2inglib.c - INGRES support routines for e2sqllib.c
 *
 * The idea is that we can plug in anything here.
 */
static char * sccs_id =  "@(#) $Name$ $Id$\n\
Copyright (c) E2 Systems Limited 1993\n";
#ifndef MINGW32
#include <sys/param.h>
#endif
#include <sys/types.h>
#ifndef LCC
#ifndef VCC2003
#include <sys/file.h>
#endif
#endif
#ifdef V32
#include <time.h>
#else
#ifndef LCC
#ifndef VCC2003
#include <sys/time.h>
#endif
#endif
#endif
#ifdef SEQ
#include <fcntl.h>
#include <time.h>
#else
#ifdef ULTRIX
#include <fcntl.h>
#else
#ifdef AIX
#include <fcntl.h>
#else
#ifndef LCC
#ifndef VCC2003
#include <sys/fcntl.h>
#endif
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
#include <string.h>
#include <ctype.h>

#ifdef AIX
#include <memory.h>
#endif
#include "tabdiff.h"
/************************************************************************
 * Statement Preparation
 ************************************************************************
 * What exactly happens rather depends on the SQL. With INGRES, different
 * SQL statements require different call sequences.
 ************************************************************************
 * Parse is a NO-OP with INGRES?
 ************************************************************************
 */
int dbms_parse(dyn)
struct dyn_con * dyn;
{
    dyn->ret_status = 0;
    dyn->con->ret_status = 0;
    return 1;
}
/************************************************************************
 * DML Statement Execution
 ************************************************************************
 * With INGRES, the bind is a two step process.
 * - Step one, you say what the data types will be.
 * - Step two, you provide the values.
 * In contrast, ORACLE and Sybase do both at the same time.
 * So, to maximise consistency, we leave the dbms_bind() function empty,
 * and do it here, using the E2SQLDA's provided.
 */
int dbms_exec(dyn)
struct dyn_con * dyn;
{
int ret_arr_size = (dyn->cur_ind) ? (dyn->cur_ind) : 1;
short int **i;
int *l;
short int * u;
char ** v;
char ** s;
short int *c, *t;
short int *p, *a;
unsigned short int **r, **o;
int cnt, rows;
char *x;

IIAPI_WAITPARM      waitParm = { -1 };

#ifdef DEBUG
    fprintf(stderr, "Execing %s ....\n",
              ( dyn->statement != NULL) ? dyn->statement :"");
    fflush(stderr);
#endif
/*
 * Decide which type of SQL statement we have. We may need to go off and call
 * other routines instead.
 */
    memset((char *) &dyn->queryParm, 0, sizeof(dyn->queryParm));
    memset((char *) &dyn->getQInfoParm, 0, sizeof(dyn->getQInfoParm));
    memset((char *) &dyn->setDescrParm, 0, sizeof(dyn->setDescrParm));
    memset((char *) &dyn->putParmParm, 0, sizeof(dyn->putParmParm));
    dyn->ret_status = 0;
    dyn->con->ret_status = 0;
    for (x = dyn->statement; *x == ' '; x++);
    if (!strncasecmp(x,"commit",6))
    {
        dbms_commit(dyn->con);
        return 1;
    }
    else
    if (!strncasecmp(x,"rollback",7))
    {
        dbms_roll(dyn->con);
        return 1;
    }
    else
    if (!strncasecmp(x,"set autocommit",14))
    {
        memset((char *) &dyn->autoparm, 0, sizeof(dyn->autoparm));
        dyn->autoparm.ac_genParm.gp_closure  = NULL;
        dyn->autoparm.ac_genParm.gp_callback = NULL;
        if (!strncasecmp(x + 14," on",3))
        {
            dyn->autoparm.ac_connHandle = dyn->con->connHandle;
            dyn->autoparm.ac_tranHandle = NULL;
            dyn->con->auto_state = 1;
        }
        else
        {
            dyn->autoparm.ac_connHandle = NULL;
            dyn->autoparm.ac_tranHandle = dyn->con->tranHandle;
            dyn->con->auto_state = 0;
        }
        IIapi_autocommit( &dyn->autoparm );

        while( dyn->autoparm.ac_genParm.gp_completed == FALSE )
           IIapi_wait( &waitParm );

        dyn->con->genParm = dyn->autoparm.ac_genParm;
        dyn->stmtHandle = NULL;
        dbms_error(dyn->con);
        if (dyn->con->errorCode != 0)
        {
            if (dyn->statement != (char *) NULL && dyn->statement[0] != '\0')
                fprintf(stderr, "dbms_exec %d (%s) problem\n", __LINE__,
                             dyn->statement);
            dbms_cancel(dyn);
        }
        else
            dyn->con->tranHandle = dyn->autoparm.ac_tranHandle;
        return 1;
    }
    if (*x == '\0')
    {
        dyn->queryType =  IIAPI_QT_EXEC_REPEAT_QUERY;
        dyn->queryParm.qy_queryType = dyn->queryType;
    }
    else
    if (!strncasecmp(x,"define",6))
    {
        dyn->queryType =  IIAPI_QT_DEF_REPEAT_QUERY;
        dyn->queryParm.qy_queryType = dyn->queryType;
        dyn->queryParm.qy_genParm.gp_callback = NULL;
        dyn->queryParm.qy_genParm.gp_closure = NULL;
        dyn->queryParm.qy_connHandle = dyn->con->connHandle;
        dyn->queryParm.qy_queryText =  strchr(dyn->statement + 16,'s') + 2;
        dyn->queryParm.qy_parameters = TRUE;
        dyn->queryParm.qy_tranHandle =  dyn->con->tranHandle;
        dyn->queryParm.qy_stmtHandle = NULL;
    
        IIapi_query( &dyn->queryParm );
    
        while( dyn->queryParm.qy_genParm.gp_completed == FALSE )
            IIapi_wait( &waitParm );
    
        dyn->con->genParm = dyn->queryParm.qy_genParm;
        dbms_error(dyn->con);
        if (dyn->con->errorCode != 0)
        {
            if (dyn->statement != (char *) NULL && dyn->statement[0] != '\0')
                fprintf(stderr, "dbms_exec %d (%s) problem\n", __LINE__,
                             dyn->statement);
            dbms_cancel(dyn);
            return 1;
        }
        else
        {
            dyn->con->tranHandle = dyn->queryParm.qy_tranHandle;
            dyn->stmtHandle = dyn->queryParm.qy_stmtHandle;
        }
        cnt = 3 + ((dyn->bdp != NULL && dyn->bdp->F > 0) ? dyn->bdp->F : 0);
        dyn->setDescrParm.sd_genParm.gp_callback = NULL;
        dyn->setDescrParm.sd_genParm.gp_closure = NULL;
        dyn->setDescrParm.sd_stmtHandle = dyn->stmtHandle;
        dyn->setDescrParm.sd_descriptorCount = cnt;
        dyn->setDescrParm.sd_descriptor = (IIAPI_DESCRIPTOR *) calloc(
                 sizeof(IIAPI_DESCRIPTOR), cnt);
        dyn->putParmParm.pp_genParm.gp_callback = NULL;
        dyn->putParmParm.pp_genParm.gp_closure = NULL;
        dyn->putParmParm.pp_stmtHandle = dyn->stmtHandle;
        dyn->putParmParm.pp_parmCount = cnt;
        dyn->putParmParm.pp_moreSegments = 0;
        dyn->putParmParm.pp_parmData =  (IIAPI_DATAVALUE *) calloc(
                 sizeof(IIAPI_DATAVALUE), cnt);

        dyn->setDescrParm.sd_descriptor[0].ds_dataType = IIAPI_INT_TYPE;
        dyn->setDescrParm.sd_descriptor[0].ds_nullable = FALSE;
        dyn->setDescrParm.sd_descriptor[0].ds_length = 4;
        dyn->setDescrParm.sd_descriptor[0].ds_precision = 0;
        dyn->setDescrParm.sd_descriptor[0].ds_scale = 0;
        dyn->setDescrParm.sd_descriptor[0].ds_columnType =
                                IIAPI_COL_SVCPARM;
        dyn->setDescrParm.sd_descriptor[0].ds_columnName = NULL;
        dyn->putParmParm.pp_parmData[0].dv_null = FALSE;
        dyn->putParmParm.pp_parmData[0].dv_length = 4;
        dyn->putParmParm.pp_parmData[0].dv_value  = &dyn->repti1; 
        dyn->setDescrParm.sd_descriptor[1].ds_dataType = IIAPI_INT_TYPE;
        dyn->setDescrParm.sd_descriptor[1].ds_nullable = FALSE;
        dyn->setDescrParm.sd_descriptor[1].ds_length = 4;
        dyn->setDescrParm.sd_descriptor[1].ds_precision = 0;
        dyn->setDescrParm.sd_descriptor[1].ds_scale = 0;
        dyn->setDescrParm.sd_descriptor[1].ds_columnType =
                                IIAPI_COL_SVCPARM;
        dyn->setDescrParm.sd_descriptor[1].ds_columnName = NULL;
        dyn->putParmParm.pp_parmData[1].dv_null = FALSE;
        dyn->putParmParm.pp_parmData[1].dv_length = 4;
        dyn->putParmParm.pp_parmData[1].dv_value  = &dyn->repti2; 
        dyn->setDescrParm.sd_descriptor[2].ds_dataType = IIAPI_CHA_TYPE;
        dyn->setDescrParm.sd_descriptor[2].ds_nullable = FALSE;
        dyn->setDescrParm.sd_descriptor[2].ds_length = 64;
        dyn->setDescrParm.sd_descriptor[2].ds_precision = 0;
        dyn->setDescrParm.sd_descriptor[2].ds_scale = 0;
        dyn->setDescrParm.sd_descriptor[2].ds_columnType =
                                IIAPI_COL_SVCPARM;
        dyn->setDescrParm.sd_descriptor[2].ds_columnName = NULL;
        dyn->putParmParm.pp_parmData[2].dv_null = FALSE;
        dyn->putParmParm.pp_parmData[2].dv_length = 64;
        dyn->putParmParm.pp_parmData[2].dv_value  = dyn->reptid; 
        if (cnt > 3)
        {
            for (cnt = 3,
                 u = dyn->bdp->U,
                 v = dyn->bdp->V,
                 s = dyn->bdp->S,
                 i = dyn->bdp->I,
                 t = dyn->bdp->T,
                 l = dyn->bdp->L,
                 p = dyn->bdp->P,
                 a = dyn->bdp->A;

                     cnt < dyn->setDescrParm.sd_descriptorCount;

                          cnt++, s++, i++, t++, l++, p++, a++, u++, v++)
            {
                dyn->setDescrParm.sd_descriptor[cnt].ds_dataType = *t;
                dyn->setDescrParm.sd_descriptor[cnt].ds_nullable = *u;
                dyn->setDescrParm.sd_descriptor[cnt].ds_length = *l;
                dyn->setDescrParm.sd_descriptor[cnt].ds_precision = *p;
                dyn->setDescrParm.sd_descriptor[cnt].ds_scale = *a;
                dyn->setDescrParm.sd_descriptor[cnt].ds_columnType = IIAPI_COL_QPARM;
                dyn->setDescrParm.sd_descriptor[cnt].ds_columnName = *s;
                dyn->putParmParm.pp_parmData[cnt].dv_null = **i;
                dyn->putParmParm.pp_parmData[cnt].dv_length = *l;
                dyn->putParmParm.pp_parmData[cnt].dv_value = *v;
            }
        }
/*
 * Set the data types
 */
        IIapi_setDescriptor( &dyn->setDescrParm );
        while( dyn->setDescrParm.sd_genParm.gp_completed == FALSE )
            IIapi_wait( &waitParm );
/*
 * Set the data values
 */
        IIapi_putParms( &dyn->putParmParm );
        while( dyn->putParmParm.pp_genParm.gp_completed == FALSE )
            IIapi_wait( &waitParm );
        dyn_note(dyn, dyn->setDescrParm.sd_descriptor);
        dyn_note(dyn, dyn->putParmParm.pp_parmData);
        return 1;
    }
    else
    if (!strncasecmp(x,"prepare",7))
        dyn->queryType =  IIAPI_QT_OPEN;
    else
    if (!strncasecmp(x,"delete",6) && dyn->cursorHandle != NULL)
        dyn->queryType =  IIAPI_QT_CURSOR_DELETE;
    else
    if (!strncasecmp(x,"update",6) && dyn->cursorHandle != NULL)
        dyn->queryType =  IIAPI_QT_CURSOR_UPDATE;
    else
    if (!strncasecmp(x,"execute",7))
        dyn->queryType =  IIAPI_QT_EXEC_PROCEDURE;  /* Not sure we want this */
    else
        dyn->queryType =  IIAPI_QT_QUERY;
/*
 * Now execute
 */
    for (rows = 0; rows < ret_arr_size; rows++ )
    {
/*
 *  Execute (repeated) query.
 */
        dyn->queryParm.qy_genParm.gp_callback = NULL;
        dyn->queryParm.qy_genParm.gp_closure = NULL;
        dyn->queryParm.qy_connHandle = dyn->con->connHandle;
        dyn->queryParm.qy_queryText =
                          (dyn->queryType == IIAPI_QT_EXEC_REPEAT_QUERY)
                                 ? NULL : dyn->statement;
        dyn->queryParm.qy_parameters = 
                ((dyn->queryType == IIAPI_QT_EXEC_REPEAT_QUERY) ||
                  (dyn->bdp != NULL && dyn->bdp->F > 0)) ? TRUE : FALSE;
        dyn->queryParm.qy_tranHandle = dyn->con->tranHandle;
        dyn->queryParm.qy_stmtHandle = NULL;

        IIapi_query( &dyn->queryParm );

        while( dyn->queryParm.qy_genParm.gp_completed == FALSE )
            IIapi_wait( &waitParm );

        dyn->con->genParm = dyn->queryParm.qy_genParm;
        dbms_error(dyn->con);
        if (dyn->con->errorCode != 0)
        {
            if (dyn->statement != (char *) NULL && dyn->statement[0] != '\0')
                fprintf(stderr, "dbms_exec %d (%s) problem\n", __LINE__,
                             dyn->statement);
            dbms_cancel(dyn);
            return 1;
        }
        dyn->stmtHandle = dyn->queryParm.qy_stmtHandle;
        dyn->con->tranHandle = dyn->queryParm.qy_tranHandle;
/*
 *  Apply query parameters.
 */
        if ( dyn->queryParm.qy_parameters == TRUE)
        {
            dyn->setDescrParm.sd_genParm.gp_callback = NULL;
            dyn->setDescrParm.sd_genParm.gp_closure = NULL;
            dyn->setDescrParm.sd_stmtHandle = dyn->stmtHandle;
            dyn->setDescrParm.sd_descriptorCount = 
                 (dyn->queryType == IIAPI_QT_EXEC_REPEAT_QUERY ) ?
                       (1 + ((dyn->bdp == NULL) ? 0 : (dyn->bdp->F)))
                          : dyn->bdp->F;
            dyn->setDescrParm.sd_descriptor = (IIAPI_DESCRIPTOR *) calloc(
                sizeof(IIAPI_DESCRIPTOR), dyn->setDescrParm.sd_descriptorCount);
            dyn->putParmParm.pp_genParm.gp_callback = NULL;
            dyn->putParmParm.pp_genParm.gp_closure = NULL;
            dyn->putParmParm.pp_stmtHandle = dyn->stmtHandle;
            dyn->putParmParm.pp_parmCount = 
                 (dyn->queryType == IIAPI_QT_EXEC_REPEAT_QUERY ) ?
                       (1 + ((dyn->bdp == NULL) ? 0 : (dyn->bdp->F)))
                       : dyn->bdp->F;
            dyn->putParmParm.pp_parmData =  (IIAPI_DATAVALUE *) calloc(
                 sizeof(IIAPI_DATAVALUE), dyn->setDescrParm.sd_descriptorCount);
            dyn->putParmParm.pp_moreSegments = 0;
    
            if (dyn->queryType == IIAPI_QT_EXEC_REPEAT_QUERY )
            {
                if (dyn->reptHandle == NULL)
                {
                    fprintf(stderr,
                     "dbms_exec repeated query no handle %d (%s) problem\n",
                             __LINE__, tbuf);
                    dbms_cancel(dyn);
                    return 1;
                }
                else
                {
                    dyn->setDescrParm.sd_descriptor[0].ds_dataType = IIAPI_HNDL_TYPE;
                    dyn->setDescrParm.sd_descriptor[0].ds_nullable = FALSE;
                    dyn->setDescrParm.sd_descriptor[0].ds_length = sizeof( II_PTR );
                    dyn->setDescrParm.sd_descriptor[0].ds_precision = 0;
                    dyn->setDescrParm.sd_descriptor[0].ds_scale = 0;
                    dyn->setDescrParm.sd_descriptor[0].ds_columnType =
                                IIAPI_COL_SVCPARM;
                    dyn->setDescrParm.sd_descriptor[0].ds_columnName = NULL;
                    dyn->putParmParm.pp_parmData[0].dv_null = FALSE;
                    dyn->putParmParm.pp_parmData[0].dv_length = sizeof( II_PTR );
                    dyn->putParmParm.pp_parmData[0].dv_value  = &(dyn->reptHandle);
                    cnt = 1;
                }
            }
            else
                cnt = 0;

            if (dyn->bdp != NULL)
            for (
                 u = dyn->bdp->U,
                 v = dyn->bdp->V,
                 s = dyn->bdp->S,
                 i = dyn->bdp->I,
                 t = dyn->bdp->T,
                 l = dyn->bdp->L,
                 p = dyn->bdp->P,
                 a = dyn->bdp->A;

                     cnt <  dyn->setDescrParm.sd_descriptorCount;

                          cnt++, s++, i++, t++, l++, p++, a++, u++, v++)
            {
                dyn->setDescrParm.sd_descriptor[cnt].ds_dataType = *t;
                dyn->setDescrParm.sd_descriptor[cnt].ds_nullable = *u;
                dyn->setDescrParm.sd_descriptor[cnt].ds_length = *l;
                dyn->setDescrParm.sd_descriptor[cnt].ds_precision = *p;
                dyn->setDescrParm.sd_descriptor[cnt].ds_scale = *a;
                dyn->setDescrParm.sd_descriptor[cnt].ds_columnType = IIAPI_COL_QPARM;
                dyn->setDescrParm.sd_descriptor[cnt].ds_columnName = *s;
                dyn->putParmParm.pp_parmData[cnt].dv_null = **i;
                dyn->putParmParm.pp_parmData[cnt].dv_length = *l;
                dyn->putParmParm.pp_parmData[cnt].dv_value = *v;
            }

            IIapi_setDescriptor( &dyn->setDescrParm );

            while( dyn->setDescrParm.sd_genParm.gp_completed == FALSE )
                IIapi_wait( &waitParm );

            IIapi_putParms( &dyn->putParmParm );

            while( dyn->putParmParm.pp_genParm.gp_completed == FALSE )
                    IIapi_wait( &waitParm );
        }
/*
 *  Free resources.
 */
        if (rows != (ret_arr_size - 1))
            dbms_stat_close(dyn);
    }
/*
 *  Get a descriptor if there is one.
 */
    dyn->getDescrParm.gd_genParm.gp_callback = NULL;
    dyn->getDescrParm.gd_genParm.gp_closure = NULL;
    dyn->getDescrParm.gd_stmtHandle = dyn->stmtHandle;
    dyn->getDescrParm.gd_descriptorCount = 0;
    dyn->getDescrParm.gd_descriptor = NULL;

    IIapi_getDescriptor( &(dyn->getDescrParm) );

    while( dyn->getDescrParm.gd_genParm.gp_completed == FALSE )
        IIapi_wait( &waitParm );

    dyn->con->genParm = dyn->getDescrParm.gd_genParm;
    dbms_error(dyn->con);
    if (dyn->con->errorCode != 0)
    {
        if (dyn->statement != (char *) NULL && dyn->statement[0] != '\0')
            fprintf(stderr, "dbms_exec %d (%s) problem\n", __LINE__,
                             dyn->statement);
        dbms_cancel(dyn);
        return 1;
    }
    if ( dyn->getDescrParm.gd_descriptorCount > 0 )
    {
        dyn->getColParm.gc_genParm.gp_callback = NULL;
        dyn->getColParm.gc_genParm.gp_closure = NULL;
        dyn->getColParm.gc_rowCount = get_sd_size() ;
        dyn->getColParm.gc_columnCount = dyn->getDescrParm.gd_descriptorCount;
        dyn->getColParm.gc_rowsReturned = 0;
        dyn->getColParm.gc_columnData =  
           (IIAPI_DATAVALUE *) calloc(sizeof(IIAPI_DATAVALUE)
                      * dyn->getDescrParm.gd_descriptorCount,
                       get_sd_size());
        dyn->getColParm.gc_stmtHandle = dyn->stmtHandle;
        dyn->getColParm.gc_moreSegments = 0;
        dyn->is_sel = 1;
    }
    else
        dyn->is_sel = 0;
    dyn->ret_status = 0;
    dyn->con->ret_status = 0;
    dbms_error(dyn->con);
    if (dyn->con->errorCode != 0)
    {
        if (dyn->statement != (char *) NULL && dyn->statement[0] != '\0')
            fprintf(stderr, "dbms_exec %d (%s) problem\n", __LINE__,
                             dyn->statement);
        dbms_cancel(dyn);
    }
/*
 * I am not sure how long this data needs to stay in scope ...
 */
    if (dyn->setDescrParm.sd_descriptor != NULL)
    {
        dyn_note(dyn, dyn->setDescrParm.sd_descriptor);
        dyn_note(dyn, dyn->putParmParm.pp_parmData);
    }
    return 1;
}
void dbms_error(con)
struct sess_con * con;
{
IIAPI_GETEINFOPARM  getErrParm;
char                type[33];

    memset((char *) &getErrParm, 0, sizeof(getErrParm));
    fprintf( stderr, "\tcon->gp_status = %s\n",
               (con->genParm.gp_status == IIAPI_ST_SUCCESS) ?
                        "IIAPI_ST_SUCCESS" :
               (con->genParm.gp_status == IIAPI_ST_MESSAGE) ?
                        "IIAPI_ST_MESSAGE" :
               (con->genParm.gp_status == IIAPI_ST_WARNING) ?
                        "IIAPI_ST_WARNING" :
               (con->genParm.gp_status == IIAPI_ST_NO_DATA) ?
                        "IIAPI_ST_NO_DATA" :
               (con->genParm.gp_status == IIAPI_ST_ERROR)   ?
                        "IIAPI_ST_ERROR"   :
               (con->genParm.gp_status == IIAPI_ST_FAILURE) ?
                        "IIAPI_ST_FAILURE" :
               (con->genParm.gp_status == IIAPI_ST_NOT_INITIALIZED) ?
                        "IIAPI_ST_NOT_INITIALIZED" :
               (con->genParm.gp_status == IIAPI_ST_INVALID_HANDLE) ?
                        "IIAPI_ST_INVALID_HANDLE"  :
               (con->genParm.gp_status == IIAPI_ST_OUT_OF_MEMORY) ?
                        "IIAPI_ST_OUT_OF_MEMORY"   :
              "(unknown status)" );
    if ( ! con->genParm.gp_errorHandle )
    {
        con->errorCode = 0;
        return;
    }
    getErrParm.ge_errorHandle = con->genParm.gp_errorHandle;
    for (;;)
    {
        IIapi_getErrorInfo( &getErrParm );
        if ( getErrParm.ge_status == IIAPI_ST_NO_DATA )
            break;
/*        if ( getErrParm.ge_status != IIAPI_ST_SUCCESS )
            break;
 */

        switch( getErrParm.ge_type )
        {
        case IIAPI_GE_ERROR  :
            strcpy( type, "ERROR" );
            break;
        case IIAPI_GE_WARNING :
            strcpy( type, "WARNING" );
            break;
        case IIAPI_GE_MESSAGE :
            strcpy(type, "USER MESSAGE");
            break;
        default:
            sprintf( type, "unknown error type: %d", getErrParm.ge_type);
            break;
        }
        fprintf( stderr, "\tError Info: %s '%s' 0x%x: %s\n",
                   type, getErrParm.ge_SQLSTATE, getErrParm.ge_errorCode,
                   getErrParm.ge_message ? getErrParm.ge_message : "NULL" );
        con->errorCode = getErrParm.ge_errorCode;
    }
    return;
}
void dbms_roll(con)
struct sess_con * con;
{
IIAPI_ROLLBACKPARM  rollbackParm;
IIAPI_WAITPARM      waitParm = { -1 };

    memset((char *) &rollbackParm, 0, sizeof(rollbackParm));
    rollbackParm.rb_genParm.gp_callback = NULL;
    rollbackParm.rb_genParm.gp_closure = NULL;
    rollbackParm.rb_tranHandle = con->tranHandle;
    rollbackParm.rb_savePointHandle = NULL;

    IIapi_rollback( &rollbackParm );

    while( rollbackParm.rb_genParm.gp_completed == FALSE )
        IIapi_wait( &waitParm );

    con->genParm = rollbackParm.rb_genParm;
    dbms_error(con);
    if (con->errorCode == 0)
        con->tranHandle = NULL;
    return;
}
int dbms_disconnect(con)
struct sess_con * con;
{
IIAPI_DISCONNPARM   disconnParm;
IIAPI_WAITPARM      waitParm = { -1 };

    memset((char *) &disconnParm, 0, sizeof(disconnParm));
    disconnParm.dc_genParm.gp_callback = NULL;
    disconnParm.dc_genParm.gp_closure = NULL;
    disconnParm.dc_connHandle = con->connHandle;

    IIapi_disconnect( &disconnParm );

    while( disconnParm.dc_genParm.gp_completed == FALSE )
        IIapi_wait( &waitParm );

    con->genParm = disconnParm.dc_genParm;
/*
 * We don't do this, because we have multiple connections, but we ought to
 * when we finish completely ....
 *  IIAPI_TERMPARM  termParm;
 *
 *  printf( "IIdemo_term: shutting down API\n" );
 *  IIapi_terminate( &termParm );
 */
    dbms_error(con);
    if (con->errorCode == 0)
    {
        con->connHandle = NULL;
        return 1;
    }
    else
        return 0;
}
/**********************************************************************
 * A no-op for INGRES
 */
int dbms_open(d)
struct dyn_con * d;
{
    return 1;
}

/************************************************************************
 * Prematurely cancel a query before fetching everything
 */
int dbms_cancel(d)
struct dyn_con * d;
{
IIAPI_CANCELPARM     cancelParm;
IIAPI_WAITPARM      waitParm = { -1 };

    memset((char *) &cancelParm, 0, sizeof(cancelParm));
    cancelParm.cn_genParm.gp_callback = NULL;
    cancelParm.cn_genParm.gp_closure = NULL;
    cancelParm.cn_stmtHandle = d->stmtHandle;

    IIapi_cancel(&cancelParm );

    while( cancelParm.cn_genParm.gp_completed == FALSE )
        IIapi_wait( &waitParm);
    d->con->genParm = cancelParm.cn_genParm;
    dbms_error(d->con);
    return 1;
}
/************************************************************************
 * Free resources
 */
int dbms_close(d)
struct dyn_con * d;
{
IIAPI_CLOSEPARM     closeParm;
IIAPI_WAITPARM      waitParm = { -1 };

#ifdef DEBUG
    fputs( "Closing  ....\n",stderr);
    fflush(stderr);
#endif
    if ( d->stmtHandle == NULL)
        return 1;
    memset((char *) &closeParm, 0, sizeof(closeParm));
    if ( d->getColParm.gc_columnData != NULL)
    {
        free(d->getColParm.gc_columnData);
        d->getColParm.gc_columnData = NULL;
    }
    closeParm.cl_genParm.gp_callback = NULL;
    closeParm.cl_genParm.gp_closure = NULL;
    closeParm.cl_stmtHandle = d->stmtHandle;

    IIapi_close( &closeParm );

    while( closeParm.cl_genParm.gp_completed == FALSE )
        IIapi_wait( &waitParm );

    d->con->genParm = closeParm.cl_genParm;
    if (con->genParm.gp_status == IIAPI_ST_SUCCESS)
        d->stmtHandle = NULL;
    return 1;
}
/************************************************************************
 * Free resources
 */
int dbms_stat_close(d)
struct dyn_con * d;
{
IIAPI_CLOSEPARM     closeParm;
IIAPI_WAITPARM      waitParm = { -1 };
IIAPI_GETQINFOPARM  getQInfoParm;
    if (d->stmtHandle == NULL)
    {
        dyn_forget(d);
        d->head_bv = NULL;
        d->tail_bv = NULL;
        return 1;
    }
#ifdef DEBUG
    fputs( "Closing statement ....\n",stderr);
    fflush(stderr);
#endif
    memset((char *) &closeParm, 0, sizeof(closeParm));
    memset((char *) &getQInfoParm, 0, sizeof(getQInfoParm));
/*
 *  Get results of executing statement.
 */
    getQInfoParm.gq_genParm.gp_callback = NULL;
    getQInfoParm.gq_genParm.gp_closure = NULL;
    getQInfoParm.gq_stmtHandle = d->stmtHandle;
    IIapi_getQueryInfo( &getQInfoParm );

    while( getQInfoParm.gq_genParm.gp_completed == FALSE )
        IIapi_wait( &waitParm );

    if ( getQInfoParm.gq_mask & IIAPI_GQ_REPEAT_QUERY_ID )
        d->reptHandle = getQInfoParm.gq_repeatQueryHandle;
    d->con->genParm = getQInfoParm.gq_genParm;
    dbms_error(con);
    dbms_close(d);
    if (d->con->errorCode != 0)
    {
        if (d->statement != (char *) NULL && d->statement[0] != '\0')
                fprintf(stderr, "dbms_stat_close %d (%s) problem\n", __LINE__,
                             d->statement);
        dbms_cancel(d);
        dbms_close(d);
    }
    dyn_forget(d);
    d->head_bv = NULL;
    d->tail_bv = NULL;
    return 1;
}
/********************************************************************
 * Process the Bind variables - A NO-OP for Ingres
 */
int dbms_bind(d, s, c, v, l, a, p, t, i, o, r)
struct dyn_con * d;
char * s;
short int c;
char * v;
int l;
short int p;
short int a;
short int t;
short int *i;
unsigned short int *o;
unsigned short int *r;
{
    return 1;
}
/*********************************************
 * Pick up trace messages
 */
static void trace_cb(tp)
IIAPI_TRACEPARM *tp;
{
    fflush(stderr);
    printf("%x:%x:%.*s\n",
        tp->tr_envHandle,
        tp->tr_connHandle,
        tp->tr_length,
        tp->tr_message);
    fflush(stdout);
    return;
}
/*********************************************
 * Attach to the Database. Return a pointer to a structure holding
 * the login data area. Blank the input user/password. 
 */
int dbms_connect(x)
struct sess_con * x;
{
/*
 * Process the command line arguments
 */
static int ini_ed;
IIAPI_WAITPARM      waitParm = { -1 };
static IIAPI_INITPARM  initParm;
IIAPI_CONNPARM      connParm;

#ifdef DEBUG
    fprintf(stderr, "Connecting %s ....\n", x->uid_pwd);
    fflush(stderr);
#endif
    memset((char *) &connParm, 0, sizeof(connParm));
    if (!ini_ed)
    {
        IIAPI_SETENVPRMPARM spp;
        memset((char *) &spp, 0, sizeof(spp));
        ini_ed = 1;
        memset((char *) &initParm, 0, sizeof(initParm));
        initParm.in_version = IIAPI_VERSION_4;
        initParm.in_timeout = -1;
        IIapi_initialize( &initParm );
        con->genParm.gp_status = initParm.in_status;
        dbms_error(con);
        if (initParm.in_status != IIAPI_ST_SUCCESS)
        {
            fputs("Initialize failed.\n", stderr);;
            fflush(stderr);
            return 0;
        }
        spp.se_envHandle = initParm.in_envHandle;
        spp.se_paramID = IIAPI_EP_TRACE_FUNC;
        spp.se_paramValue = trace_cb;
        IIapi_setEnvParam(&spp);
        fprintf( stderr, "spp.se_status = %d (%s)\n", spp.se_status,
               (spp.se_status == IIAPI_ST_SUCCESS) ?
                        "IIAPI_ST_SUCCESS" :
               (spp.se_status == IIAPI_ST_MESSAGE) ?
                        "IIAPI_ST_MESSAGE" :
               (spp.se_status == IIAPI_ST_WARNING) ?
                        "IIAPI_ST_WARNING" :
               (spp.se_status == IIAPI_ST_NO_DATA) ?
                        "IIAPI_ST_NO_DATA" :
               (spp.se_status == IIAPI_ST_ERROR)   ?
                        "IIAPI_ST_ERROR"   :
               (spp.se_status == IIAPI_ST_FAILURE) ?
                        "IIAPI_ST_FAILURE" :
               (spp.se_status == IIAPI_ST_NOT_INITIALIZED) ?
                        "IIAPI_ST_NOT_INITIALIZED" :
               (spp.se_status == IIAPI_ST_INVALID_HANDLE) ?
                        "IIAPI_ST_INVALID_HANDLE"  :
               (spp.se_status == IIAPI_ST_OUT_OF_MEMORY) ?
                        "IIAPI_ST_OUT_OF_MEMORY"   :
              "Unknown status" );
    }
/*
 * Split up the database name, user name, password
 */
    if ((connParm.co_target = nextasc(x->uid_pwd,'/','\\')) != (char *) NULL)
        connParm.co_target = strdup(connParm.co_target);
    else
        connParm.co_target = NULL;
    if ((connParm.co_username = nextasc(NULL, '/','\\')) != (char *) NULL)
        connParm.co_username = strdup(connParm.co_username);
    else
        connParm.co_username = NULL;
    if ((connParm.co_password = nextasc(NULL, '/','\\')) != (char *) NULL)
        connParm.co_password = strdup(connParm.co_password);
    else
        connParm.co_password = NULL;
#ifdef DEBUG
    fprintf(stderr,"db:%s user:%s pwd:%s\n",
        (connParm.co_target == NULL) ? "" : connParm.co_target,
        (connParm.co_username == NULL) ? "" : connParm.co_username,
        (connParm.co_password == NULL) ? "" : connParm.co_password);
#endif

    connParm.co_genParm.gp_callback = NULL;
    connParm.co_genParm.gp_closure = NULL;
    connParm.co_type = IIAPI_CT_SQL;
    connParm.co_connHandle = initParm.in_envHandle;
    connParm.co_tranHandle = NULL;
    connParm.co_timeout = -1;

    IIapi_connect( &connParm );

    while( connParm.co_genParm.gp_completed == FALSE )
        IIapi_wait( &waitParm );

    con->connHandle = connParm.co_connHandle;
    con->genParm = connParm.co_genParm;
    dbms_error(con);
    if (con->genParm.gp_status != IIAPI_ST_SUCCESS)
    {
        fputs("Connection failed.\n", stderr);;
        fflush(stderr);
        return 0;
    }
    fputs("Connected.\n", stderr);;
    fflush(stderr);
    return 1;
}
/***********************************************************************
 * Describe the select list variables.
 */
int dbms_desc(d,cnt,bv)
struct dyn_con * d;
int cnt;
struct bv *bv;
{
    if (cnt <= d->getDescrParm.gd_descriptorCount)
    {
        bv->dbsize = d->getDescrParm.gd_descriptor[cnt - 1].ds_length;
        bv->dbtype = d->getDescrParm.gd_descriptor[cnt - 1].ds_dataType;
        bv->nullok = d->getDescrParm.gd_descriptor[cnt - 1].ds_nullable;
        bv->prec = d->getDescrParm.gd_descriptor[cnt - 1].ds_precision;
        bv->scale = d->getDescrParm.gd_descriptor[cnt - 1].ds_scale;
        bv->dsize = bv->dbsize;
        bv->bname = d->getDescrParm.gd_descriptor[cnt - 1].ds_columnName;
        bv->blen = (bv->bname == NULL) ? 0 : strlen(bv->bname);
        return 0;
    }
    else
        return 1;
}
/*
 * Now execute the define itself
 */
int dbms_define(d,cnt,v,l,t,i,o,r)
struct dyn_con * d;
int cnt;
char * v;
int l;
short int t;
short int *i;
unsigned short int *o;
unsigned short int *r;
{
    if (cnt <= d->getDescrParm.gd_descriptorCount)
    {
    int rows;
    int arr_size = d->sdp->F * d->sdp->arr;

        for (rows = 0; rows < arr_size; rows += d->sdp->F)
        {
            d->getColParm.gc_columnData[cnt - 1 + rows].dv_value = v;
            v += l;
        }
    }
    return 1;
}
/************************************************************************
 * Handle an array fetch with the dynamic variables
 */
int dbms_fetch(dyn)
struct dyn_con *dyn;
{
IIAPI_WAITPARM      waitParm = { -1 };
int cnt;
int rows;
int arr_size;

#ifdef DEBUG
    fputs( "Fetching statement ....\n",stderr);
    fflush(stderr);
#endif
    memset((char *) &dyn->getQInfoParm, 0, sizeof(dyn->getQInfoParm));
    if (dyn->sdp == (E2SQLDA *) NULL)
    {
        dbms_stat_close(dyn);
        dyn->ret_status = 1403;
        dyn->con->ret_status = 1403;
        return 1;
    }
    IIapi_getColumns( &(dyn->getColParm) );

    while ( dyn->getColParm.gc_genParm.gp_completed == FALSE )
        IIapi_wait( &waitParm );
    if ( dyn->getQInfoParm.gq_mask & IIAPI_GQ_REPEAT_QUERY_ID )
        dyn->reptHandle = dyn->getQInfoParm.gq_repeatQueryHandle;
    dyn->con->genParm = dyn->getColParm.gc_genParm;
    dbms_error(dyn->con);
    if ( dyn->getColParm.gc_genParm.gp_status < IIAPI_ST_NO_DATA )
    {
        arr_size = dyn->sdp->F * dyn->getColParm.gc_rowsReturned;
        for (cnt = 0; cnt < dyn->getDescrParm.gd_descriptorCount; cnt++)
        {
        short int *i = dyn->sdp->I[cnt];
        short int *o =  dyn->sdp->O[cnt];

            for (rows = 0; rows < arr_size; rows += dyn->sdp->F)
            {
                *i++ = dyn->getColParm.gc_columnData[cnt + rows ].dv_null;
                *o++ = dyn->getColParm.gc_columnData[cnt + rows].dv_length;
            }
        }
        dyn->so_far += dyn->getColParm.gc_rowsReturned;
    }
    else
    {
/*
 *  Free resources.
 */
        dbms_stat_close(dyn);
        dyn->ret_status = 1403;
        dyn->con->ret_status = 1403;
    }
    return 1;
}
void dbms_commit(sess)
struct sess_con * sess;
{
IIAPI_COMMITPARM    commitParm;
IIAPI_WAITPARM      waitParm = { -1 };

#ifdef DEBUG
    fputs( "Committing ....\n",stderr);
    fflush(stderr);
#endif
    memset((char *) &commitParm, 0, sizeof(commitParm));
    commitParm.cm_genParm.gp_callback = NULL;
    commitParm.cm_genParm.gp_closure = NULL;
    commitParm.cm_tranHandle = sess->tranHandle;

    IIapi_commit( &commitParm );

    while ( commitParm.cm_genParm.gp_completed == FALSE )
        IIapi_wait( &waitParm );
    sess->genParm = commitParm.cm_genParm;
    dbms_error(sess);
    if (sess->errorCode == 0)
        sess->tranHandle = NULL;
    return;
}
