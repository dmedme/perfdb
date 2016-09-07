/***************************************************************************
 * e2orant - E2 Systems ORACLE NT Logon code
 *
 * This file contains all the Windows bits. 32 bit only.
 */
static char * sccs_id="%W %D% %T% %E% %U%\n\
Copyright (c) E2 Systems Limited 1998";
#include <windows.h>
#include <windowsx.h>
#include <errno.h>
#include "tabdiff.h"
#include "e2orant.h"
/*
 * Variables for Communication between Call Back procedures and main control 
 */
static struct sess_con * tmp_con;  /* Used for dialogue communication */
static char szCaption[128];
struct sess_con * do_oracle_session(HWND hwnd, LPCSTR sz);
/****************************************************************************
 *  PURPOSE: puts out a message box, using a format and a numeric and
 *  string argument.
 */
static void ShowError(lpTitle, lpFmt, lpStr, ulNum)
LPSTR lpTitle;
LPSTR lpFmt;
LPSTR lpStr;
LONG ulNum;
{
char buf[128];
    (void) wsprintf( (LPSTR) &buf[0],lpFmt, lpStr, ulNum);
    if (InSendMessage())
        ReplyMessage(TRUE);
    (void) MessageBox((HWND) NULL,
            (LPCSTR) &buf[0], lpTitle,
               MB_TASKMODAL|MB_ICONSTOP|MB_OK|MB_TOPMOST|MB_SETFOREGROUND);
    return;
}
/**************************************************************************
 * Callback function for the routine that logs a session on to ORACLE
 */
BOOL CALLBACK
DlgOracleSession (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
char szOrUser[32];
char szOrPassword[32];
char szOrTNS[256];
char szLogin[378];
    if (InSendMessage())
        ReplyMessage(TRUE);

    switch (msg)
    {
    case WM_INITDIALOG:
        SetWindowText(hDlg, szCaption);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            (void) GetDlgItemText(hDlg, ID_ETORUSER,szOrUser,
                    sizeof(szOrUser) - 1);
            (void) GetDlgItemText(hDlg, ID_ETORPASS,szOrPassword,
                    sizeof(szOrPassword) - 1);
            (void) GetDlgItemText(hDlg, ID_ETORTNS,szOrTNS,
                    sizeof(szOrTNS) - 1);
#ifdef DEBUG
            ShowError(szName, "Length of szOrUser: %u",
                     strlen(szOrUser)); 
            ShowError(szName, "Length of szOrPassword: %u",
                     strlen(szOrPassword)); 
            ShowError(szName, "Length of szOrTNS: %u",
                     strlen(szOrTNS)); 
#endif
            wsprintf(szLogin,"%s/%s@%s",szOrUser,szOrPassword,szOrTNS);
            if ((tmp_con = dyn_connect(szLogin,"e2orant"))
                                          == (struct sess_con *) NULL)
            {
                (void) ShowError(szOrUser, "Failed to log on to database %s\n",
                       (LONG) &szOrTNS);
            }
            else
            {
                wsprintf(tmp_con->description,"%s@%s",szOrUser, szOrTNS);
                EndDialog(hDlg, TRUE);
            }
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, FALSE);
            return TRUE;

        case ID_HELP:
DoHelp:
#if 0
                    bHelpActive=WinHelp(hDlg, szHelpFile, HELP_CONTEXT,
                                    IDH_DLG_ORASESSION);
                    if (!bHelpActive)
                        MessageBox(hDlg, "Failed to start help", szName, MB_OK);
#else
            MessageBox(hDlg, "Insert your call to WinHelp() here.",
                        "badsort",  MB_OK);
#endif
            break;
        }
        break;
    default:
        break;
    }
    return FALSE;
}
/*
 * Sign-on to ORACLE
 */
struct sess_con * do_oracle_session(HWND hwnd, LPCSTR sz)
{
    strcpy(&szCaption[0], sz);
    if (DialogBox(NULL, MAKEINTRESOURCE(DLG_ORASESSION), 
                              hwnd, DlgOracleSession ))
        return tmp_con;
    else
        return (struct sess_con *) NULL;
}
