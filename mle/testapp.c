#define INCL_DOSERRORS
#define INCL_DOSMISC
#define INCL_DOSRESOURCES
#define INCL_GPI
#define INCL_WIN
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "textctl.h"


#define WINDOWCLASS     "UniEditor"
#define ID_EDITOR       100

#define TEST_TEXT       "The quick brown\tfox jumps over the lazy\tdog. これは日本語のテキストです。\nWoven silk pyjamas—exchanged for blue quartz?"

#define DEFAULT_FONT    "10.Times New Roman MT 30"
//#define DEFAULT_FONT    "10.Kochi Mincho"
//#define DEFAULT_FONT    "10.Courier New"

HWND editor = NULLHANDLE;


MRESULT EXPENTRY MainWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );


/* ------------------------------------------------------------------------- *
 * main program                                                              *
 * ------------------------------------------------------------------------- */
int main( void )
{
    HAB       hab;                    // anchor block handle
    HMQ       hmq;                    // message queue handle
    HWND      hwndFrame = 0,          // window handle
              hwndClient = 0;         // client area handle
    QMSG      qmsg;                   // message queue
    CHAR      szError[ 256 ];         // error message buffer
    ULONG     flStyle = FCF_TITLEBAR | FCF_SYSMENU | FCF_MINMAX | FCF_SIZEBORDER |
                        FCF_ICON | FCF_AUTOICON | FCF_TASKLIST;
    BOOL      fInitFailure = FALSE;


    hab = WinInitialize( 0 );
    if ( hab == NULLHANDLE ) {
        sprintf( szError, "WinInitialize() failed.");
        fInitFailure = TRUE;
    }
    else {
        hmq = WinCreateMsgQueue( hab, 0 );
        if ( hmq == NULLHANDLE ) {
            sprintf( szError, "Unable to create message queue:\nWinGetLastError() = 0x%X\n", WinGetLastError(hab) );
            fInitFailure = TRUE;
        }
    }

    if ( !fInitFailure && !UWinRegisterMLE( hab )) {
        sprintf( szError, "Failed to register class %s:\nWinGetLastError() = 0x%X\n", WC_UMLE, WinGetLastError(hab) );
        fInitFailure = TRUE;
    }

    if (( !fInitFailure ) &&
        ( !WinRegisterClass( hab, WINDOWCLASS, MainWndProc, CS_SIZEREDRAW, 0 )))
    {
        sprintf( szError, "Failed to register class %s:\nWinGetLastError() = 0x%X\n", WINDOWCLASS, WinGetLastError(hab) );
        fInitFailure = TRUE;
    }
    if ( !fInitFailure ) {
        hwndFrame = WinCreateStdWindow( HWND_DESKTOP, 0L, &flStyle,
                                        WINDOWCLASS, "Test Editor Control", 0L,
                                        NULLHANDLE, 1, &hwndClient );
        if (( hwndFrame == NULLHANDLE ) || ( hwndClient == NULLHANDLE )) {
            sprintf( szError, "Failed to create application window:\nWinGetLastError() = 0x%X\n", WinGetLastError(hab) );
            fInitFailure = TRUE;
        }
    }

    if ( fInitFailure ) {
        WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, szError, "Program Initialization Error", 0, MB_CANCEL | MB_ERROR );
    } else {
        editor = WinCreateWindow( hwndClient, WC_UMLE, "", WS_VISIBLE | WS_GROUP | MLS_BORDER | MLS_VSCROLL | MLS_WORDWRAP,
                                  1, 1, 0, 0, hwndClient, HWND_TOP, ID_EDITOR, NULL, NULL );
        WinSetPresParam( editor, PP_FONTNAMESIZE, strlen( DEFAULT_FONT ), (PVOID) DEFAULT_FONT );
        WinSendMsg( editor, MLM_INSERT, MPFROMP( TEST_TEXT ), MPFROM2SHORT( 1208, 0 ));
        WinSetWindowPos( hwndFrame, HWND_TOP, 100, 100, 600, 400, SWP_SHOW | SWP_MOVE | SWP_SIZE | SWP_ACTIVATE );
        WinSetFocus( HWND_DESKTOP, editor );

        // Now run the main program message loop
        while ( WinGetMsg( hab, &qmsg, 0, 0, 0 )) WinDispatchMsg( hab, &qmsg );
    }

    // Clean up and exit
    WinDestroyWindow( hwndFrame );
    WinDestroyMsgQueue( hmq );
    WinTerminate( hab );

    return ( 0 );
}


/* ------------------------------------------------------------------------- *
 * Window procedure for the main client window.                              *
 * ------------------------------------------------------------------------- */
MRESULT EXPENTRY MainWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    POINTL  ptl;
    RECTL   rcl;
    HPS     hps;


    switch( msg ) {

        case WM_CREATE:
            //WinShowWindow( hwnd, TRUE );
            return (MRESULT) FALSE;


        case WM_PAINT:
            WinQueryWindowRect( hwnd, &rcl );
            if (( hps = WinBeginPaint( hwnd, NULLHANDLE, NULLHANDLE )) != NULLHANDLE ) {

                GpiBeginPath( hps, 1L );
                ptl.x = rcl.xLeft;
                ptl.y = rcl.yBottom;
                GpiMove( hps, &ptl );
                ptl.x = rcl.xRight;
                ptl.y = rcl.yTop;
                GpiBox( hps, DRO_OUTLINE, &ptl, 0, 0 );
                ptl.x = rcl.xLeft + 1;
                ptl.y = rcl.yBottom + 1;
                GpiMove( hps, &ptl );
                ptl.x = rcl.xRight - 2;
                ptl.y = rcl.yTop - 10;
                GpiBox( hps, DRO_OUTLINE, &ptl, 0, 0 );
                GpiEndPath( hps );
                GpiSetColor( hps, SYSCLR_DIALOGBACKGROUND );
                GpiFillPath( hps, 1L, FPATH_ALTERNATE );

                //WinFillRect( hps, &rcl, SYSCLR_DIALOGBACKGROUND );
                WinEndPaint( hps );
            }
            break;

        case WM_SIZE:
            if ( !editor ) break;
            WinQueryWindowRect( hwnd, &rcl );
            WinSetWindowPos( editor, HWND_TOP, 1, 1, rcl.xRight - 2, rcl.yTop - 10, SWP_SIZE );

            break;

        case WM_CLOSE:
            WinPostMsg( hwnd, WM_QUIT, 0, 0 );
            return (MRESULT) 0;

    } // end event handlers

    return ( WinDefWindowProc( hwnd, msg, mp1, mp2 ));

}
