#define INCL_DOSERRORS
#define INCL_DOSMISC
#define INCL_DOSNLS
#define INCL_GPI
#define INCL_WIN
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uconv.h>
#include <unidef.h>
#include "byteparse.h"
#include "gpitext.h"
#include "gpiutil.h"
#include "linebuf.h"
#include "textctl.h"
#include "textseq.h"

#include "debug.h"


// ***************************************************************************
// PRIVATE DECLARATIONS
// ***************************************************************************

// ----------------------------------------------------------------------------
// CONSTANTS

// Maximum string length...
#define CHARSTRING_MAXZ         5       // ...of a multibyte glyph string
#define CPNAME_MAXZ             12      // ...of a ULS codepage name
#define CPSPEC_MAXZ             32      // ...of a ULS codepage specifier
#define CPDESC_MAXZ             128     // ...of a codepage description in the GUI

#define LB_INITIAL_SIZE         128     // initial size of the line-offset buffer

#define REFLOW_SEGMENT_LENGTH   1024    // split reflow into strings of this length


// ----------------------------------------------------------------------------
// MACROS


// Shortcuts for getting rectangle dimensions
#define RECTL_WIDTH( r )    ((ULONG)max(r.xRight - r.xLeft, 0))
#define RECTL_HEIGHT( r )   ((ULONG)max(r.yTop - r.yBottom, 0))


// ----------------------------------------------------------------------------
// TYPEDEFS

// Character- or character-range-specific text attributes (linked list item)
typedef struct _UMLE_Text_Attrs {
    ULONG  ulStart,     // starting byte offset of the text to which attributes apply
           ulLength;    // length (in bytes) of the text to which attributes apply
    USHORT usCP,        // codepage which this text should use when in codepage mode
           fsAttr;      // text styling and BiDi attributes
    RGB    rgbFG,       // text foreground colour
           rbgBG;       // text background colour

    // pointer to next item in list
    struct _UMLE_Text_Attrs *pNext;
} UMTEXTATTRS, *PUMTEXTATTRS;


// Internal state data for the Unicode MLE control
typedef struct _UMLE_Private_Data {
    UMLECTLDATA ctldata;            // public window control data
    ULONG       id;                 // our control ID
    ULONG       flStyle;            // our style flags
    HAB         hab;                // anchor-block handle
    HWND        hwndSBV, hwndSBH;   // scrollbar handles
    RECTL       rclView;            // viewport (displayable text) rectangle
    ULONG       ulLinesTotal,       // total number of lines of text required
                ulLinesVisible,     // number of lines that can be displayed
                ulUnitHeight,       // height of a line of text (or one vertical scroll unit)
                ulUnitWidth,        // width of one horizontal scroll unit
                ulOffsetY,          // v.scroll position (unit offset from top)
                ulOffsetX,          // h.scroll position (unit offset from left)
                ulColsTotal,        // width of the longest line in scroll units (not used when wrap is on)
                ulColsVisible;      // number of columns (h.scroll units) that can be displayed
    LONG        lDPI;               // current (vertical) font DPI
    FONTMETRICS fm;                 // current font metrics     } // these may someday move into UMTEXTATTRS, along with a saved charset ID
    FATTRS      fattrs;             // current font attributes  } // "
    USHORT      usConvCP,           // conversion codepage for imported/exported text (0 indicates process CP)
                usDispCP;           // default display codepage (in codepage mode this may be overridden by text attributes)
    BYTE        dbcs[ 12 ];         // default DBCS information vector (byte-ranges)
    ULONG       ulTabSize,          // current horizontal tab width
                ulLongest;          // line-buffer offset of the longest line in the text
    LBOBUFFER   breaks;             // buffer of line-break byte offsets
    EDITORTEXT  text;               // text buffer
} UMLEPDATA, *PUMLEPDATA;


// ----------------------------------------------------------------------------
// PRIVATE FUNCTION PROTOTYPES

ULONG            DrawEditorText( HWND hwnd, HPS hps, PPOINTL pptl, PUMLEPDATA pCtl );
ULONG            DrawUnicodeTextSequence( HWND hwnd, HPS hps, PPOINTL pptl, PUMLEPDATA pCtl, ULONG ulStart, ULONG ulLength );
ULONG            DrawCodepageTextSequence( HWND hwnd, HPS hps, PPOINTL pptl, PUMLEPDATA pCtl, ULONG ulStart, ULONG ulLength, ULONG ulCP );
ULONG            EnumerateLines( HPS hps, PUMLEPDATA pPrivate, ULONG cbStart );
LONG             GetLineExtent( HPS hps, PUMLEPDATA pPrivate, ULONG cbStart, ULONG cbLength );
ULONG            InsertText( HWND hwnd, PSZ pszText, USHORT usCP, USHORT fsAttr );
ULONG            ReflowEditorText( HPS hps, PUMLEPDATA pPrivate, POINTL ptl, ULONG cbStart  );
ULONG            ReflowUnicodeTextSequence( HPS hps, PPOINTL pptl, PUMLEPDATA pCtl, UniChar *psuText, ULONG cbOffset );
MRESULT EXPENTRY UMLEWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );
void             UpdateFont( HWND hwnd, PUMLEPDATA pPrivate );



// ***************************************************************************
// PUBLIC FUNCTIONS
// ***************************************************************************


/* ------------------------------------------------------------------------- *
 * UWinRegisterMLE                                                           *
 *                                                                           *
 * Registers the WC_UMLE control.                                            *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HAB hab: Application's anchor-block handle.                             *
 *                                                                           *
 * RETURNS: BOOL                                                             *
 *   Return code from WinRegisterClass.                                      *
 * ------------------------------------------------------------------------- */
BOOL UWinRegisterMLE( HAB hab )
{
    return WinRegisterClass( hab, WC_UMLE, UMLEWndProc,
                             CS_SIZEREDRAW, sizeof( PVOID ));
}



// ***************************************************************************
// PRIVATE FUNCTIONS
// ***************************************************************************

/* ------------------------------------------------------------------------- *
 * Text editor widget window procedure .                                     *
 * ------------------------------------------------------------------------- */
MRESULT EXPENTRY UMLEWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    //PWNDPARAMS  pwp;              // pointer to window parameters
    PUMLEPDATA  pPrivate;         // pointer to private control data
    HPS         hps;              // control's presentation space
    RECTL       rcl;              // control's window area
    POINTL      ptl;              // current drawing position
    SWP         swp;              // used to query window sizes
    COUNTRYCODE cc;               // used by DosQueryDBCSEnv
    LONG        lBG,              // background colour presentation parameter
                lMarginX,         // margin around text clipping rectangle
                lMarginY;         // ...
    LONG        lScrlVx, lScrlVy, // vertical (left) scrollbar position
                lScrlVw, lScrlVh, // vertical (left) scrollbar size
                lScrlHx, lScrlHy, // horizontal (bottom) scrollbar position
                lScrlHw, lScrlHh, // horizontal (bottom) scrollbar size
                lInc;             // scrollbar change size
    SHORT       sMaxH, sMaxV;     // scrollbar maximum ranges
    ULONG       ulRC;


    switch( msg ) {


        /* MLM_INSERT: Differs from the standard MLE version in that mp2 may
         * contain the following:
         *   USHORT usCodepage: Source codepage of inserted text (default: current)
         *   USHORT fsAttr:     Styling and BiDi attributes of inserted text
         */
        case MLM_INSERT:
            ulRC = InsertText( hwnd, (PSZ) mp1, SHORT1FROMMP( mp2 ), SHORT2FROMMP( mp2 ));
            WinInvalidateRect( hwnd, NULL, FALSE );
            return (MRESULT) ulRC;


        case WM_CHAR:
            break;


        case WM_CREATE:
            // Initialize the private control data structure
            if ( !( pPrivate = (PUMLEPDATA) calloc( 1, sizeof( UMLEPDATA ))))
                return (MRESULT) TRUE;
            WinSetWindowPtr( hwnd, 0, pPrivate );

            if ( mp1 ) {
                memcpy( &(pPrivate->ctldata), mp1, ((PUMLECTLDATA)mp1)->cbCtlData );
            }
            pPrivate->id = ((PCREATESTRUCT)mp2)->id;
            pPrivate->flStyle = ((PCREATESTRUCT)mp2)->flStyle;
            pPrivate->hab = WinQueryAnchorBlock( hwnd );
            pPrivate->lDPI = GetCurrentDPI( hwnd );
            pPrivate->usDispCP = 1200;
            //pPrivate->usDispCP = 850;

            // Get the DBCS information vector
            cc.country = 0;
            cc.codepage = 0;
            DosQueryDBCSEnv( sizeof( pPrivate->dbcs ), &cc, pPrivate->dbcs );

            DEBUG_START();
            DEBUG_PRINTF("[UMLEWndProc] WM_CREATE\n");

            // Create the text object and line buffer
            TextCreate( &(pPrivate->text) );
            TextInitContents( pPrivate->text, NULL, 0 );
            LineBuffer_Init( &(pPrivate->breaks), LB_INITIAL_SIZE );

            // Set the initial font
            UpdateFont( hwnd, pPrivate );

            // Create the scrollbars if requested
            if ( pPrivate->flStyle & MLS_HSCROLL ) {
                pPrivate->hwndSBH = WinCreateWindow( hwnd, WC_SCROLLBAR, NULL,
                                                     SBS_HORZ | SBS_AUTOTRACK,
                                                     0, 0, 0, 0, hwnd, HWND_TOP,
                                                     -1, NULL, NULL );
                WinSendMsg( pPrivate->hwndSBH, SBM_SETTHUMBSIZE,
                            MPFROM2SHORT( 1, 1 ), MPVOID );
                WinSendMsg( pPrivate->hwndSBH, SBM_SETSCROLLBAR,
                            MPVOID, MPFROM2SHORT( 0, 1 ));
            }
            if ( pPrivate->flStyle & MLS_VSCROLL ) {
                pPrivate->hwndSBV = WinCreateWindow( hwnd, WC_SCROLLBAR, NULL,
                                                     SBS_VERT | SBS_AUTOTRACK,
                                                     0, 0, 0, 0, hwnd, HWND_TOP,
                                                     -1, NULL, NULL );
                WinSendMsg( pPrivate->hwndSBV, SBM_SETTHUMBSIZE,
                            MPFROM2SHORT( 1, 1 ), MPVOID );
                WinSendMsg( pPrivate->hwndSBV, SBM_SETSCROLLBAR,
                            MPVOID, MPFROM2SHORT( 0, 1 ));
            }
            pPrivate->ulOffsetY = 0;

            return (MRESULT) FALSE;


        case WM_DESTROY:
            if (( pPrivate = WinQueryWindowPtr( hwnd, 0 )) != NULL ) {
                // free any allocated fields of pPrivate
                LineBuffer_Free( &(pPrivate->breaks) );
                if ( pPrivate->text )
                    TextFree( &(pPrivate->text) );
                if ( pPrivate->hwndSBH != NULLHANDLE )
                    WinDestroyWindow( pPrivate->hwndSBH );
                if ( pPrivate->hwndSBV != NULLHANDLE )
                    WinDestroyWindow( pPrivate->hwndSBV );
                free( pPrivate );
            }

            DEBUG_END;

            break;


        case WM_PAINT:
            pPrivate = WinQueryWindowPtr( hwnd, 0 );
            hps = WinBeginPaint( hwnd, NULLHANDLE, NULLHANDLE );
            WinQueryWindowRect( hwnd, &rcl );

            // Adjust our painting region to accomodate the border & scrollbars
            if ( pPrivate->flStyle & MLS_BORDER ) {
                DrawNice3DBorder( hps, rcl );
                WinInflateRect( hps, &rcl, -2, -2 );
            }

            if ( pPrivate->hwndSBH ) {
                WinQueryWindowPos( pPrivate->hwndSBH, &swp );
                if ( pPrivate->hwndSBH ) {
                    RECTL rclCorner;
                    rclCorner.xRight  = rcl.xRight;
                    rclCorner.yBottom = rcl.yBottom;
                    rclCorner.xLeft   = rcl.xRight - SB_SIZE - 2;
                    rclCorner.yTop    = rcl.yBottom + swp.cy;
                    WinFillRect( hps, &rclCorner, SYSCLR_DIALOGBACKGROUND );
                }
                rcl.yBottom += swp.cy - 1;
            }
            if ( pPrivate->hwndSBV ) {
                WinQueryWindowPos( pPrivate->hwndSBV, &swp );
                rcl.xRight -= swp.cx - 1;
            }

            // Switch to RGB colour mode
            GpiCreateLogColorTable( hps, LCOL_RESET, LCOLF_RGB, 0, 0, NULL );

            // Fill in the window with the proper background colour
            if ( ! WinQueryPresParam( hwnd, PP_BACKGROUNDCOLOR,
                                      PP_BACKGROUNDCOLORINDEX, NULL,
                                      sizeof( lBG ), &lBG, QPF_ID2COLORINDEX ))
                lBG = SYSCLR_WINDOW;
            WinFillRect( hps, &rcl, lBG );

            // Now paint the text
            DrawEditorText( hwnd, hps, &ptl, pPrivate );
            WinEndPaint( hps );

            return (MRESULT) 0;


        case WM_PRESPARAMCHANGED:
            pPrivate = WinQueryWindowPtr( hwnd, 0 );
            switch ( (ULONG) mp1 ) {

                case PP_BACKGROUNDCOLOR:
                case PP_BACKGROUNDCOLORINDEX:
                case PP_FOREGROUNDCOLOR:
                case PP_FOREGROUNDCOLORINDEX:
                    WinInvalidateRect(hwnd, NULL, FALSE);
                    break;

                case PP_FONTNAMESIZE:
                    UpdateFont( hwnd, pPrivate );
                    break;

            }
            break;


        case WM_SIZE:
            pPrivate = WinQueryWindowPtr( hwnd, 0 );
            if ( !pPrivate ) break;
            WinQueryWindowRect( hwnd, &rcl );

            // Update the scrollbar size & position
            lScrlVw = ( pPrivate->hwndSBV ) ? SB_SIZE : 0;
            lScrlHh = ( pPrivate->hwndSBH ) ? SB_SIZE : 0;
            lScrlVx = RECTL_WIDTH( rcl ) - lScrlVw - 1;
            lScrlVy = 1 + lScrlHh;
            lScrlVh = RECTL_HEIGHT( rcl ) - 1 - lScrlVy;
            lScrlHx = 1;
            lScrlHy = 1;
            lScrlHw = RECTL_WIDTH( rcl ) - 3 - lScrlVw;
            if ( pPrivate->hwndSBV ) {
                WinSetWindowPos( pPrivate->hwndSBV, NULLHANDLE,
                                 lScrlVx, lScrlVy, lScrlVw, lScrlVh,
                                 SWP_MOVE | SWP_SIZE | SWP_SHOW );
            }
            if ( pPrivate->hwndSBH ) {
                WinSetWindowPos( pPrivate->hwndSBH, NULLHANDLE,
                                 lScrlHx, lScrlHy, lScrlHw, lScrlHh,
                                 SWP_MOVE | SWP_SIZE | SWP_SHOW );
            }

            // Recalculate the text viewport dimensions
            lMarginX = pPrivate->fm.lAveCharWidth / 2;
            lMarginY = pPrivate->fm.lExternalLeading;
            WinInflateRect( hps, &rcl, -4, -4 );
            pPrivate->rclView.xLeft   = rcl.xLeft + lMarginX;
            pPrivate->rclView.yBottom = rcl.yBottom + lMarginY + lScrlHh;
            pPrivate->rclView.xRight  = rcl.xRight - lMarginX - lScrlVw;
            pPrivate->rclView.yTop    = rcl.yTop - lMarginY - 2;
            pPrivate->ulLinesVisible  = RECTL_HEIGHT( pPrivate->rclView ) / pPrivate->ulUnitHeight;
            pPrivate->ulColsVisible   = RECTL_WIDTH( pPrivate->rclView ) / pPrivate->ulUnitWidth;

            // Reflow the text if wrapping is enabled and the window width changed
            if (( pPrivate->flStyle & MLS_WORDWRAP ) &&
                ( SHORT1FROMMP( mp2 ) != SHORT1FROMMP( mp1 )))
            {
                hps = WinGetPS( hwnd );
                SetFontFromAttrs( hps, pPrivate->lDPI, &(pPrivate->fattrs), pPrivate->fm );
                //SetFontFromPP( hps, hwnd, pPrivate->lDPI, &(pPrivate->fattrs) );
                ptl.x = pPrivate->rclView.xLeft;
                ptl.y = pPrivate->rclView.yTop - pPrivate->ulUnitHeight;
                pPrivate->ulLinesTotal = ReflowEditorText( hps, pPrivate, ptl, 0 );
                WinReleasePS( hps );
            }

            // Now we can recalculate the scrollbar settings
            if ( pPrivate->hwndSBV ) {
                if ( pPrivate->ulLinesTotal <= pPrivate->ulLinesVisible ) {
                    pPrivate->ulOffsetY = 0;
                    WinSendMsg( pPrivate->hwndSBV, SBM_SETTHUMBSIZE, MPFROM2SHORT( 1, 1 ), 0 );
                    WinSendMsg( pPrivate->hwndSBV, SBM_SETSCROLLBAR, 0, MPFROM2SHORT( 0, 1 ));
                }
                else {
                    sMaxV = max( 0, (LONG)( pPrivate->ulLinesTotal - pPrivate->ulLinesVisible ));
                    WinSendMsg( pPrivate->hwndSBV, SBM_SETTHUMBSIZE,
                                MPFROM2SHORT( pPrivate->ulLinesVisible, pPrivate->ulLinesTotal ), 0 );
                    WinSendMsg( pPrivate->hwndSBV, SBM_SETSCROLLBAR,
                                MPFROMSHORT( pPrivate->ulOffsetY ),
                                MPFROM2SHORT( 0, sMaxV ));
                }
            }
            if ( pPrivate->hwndSBH ) {
                if ( pPrivate->ulColsTotal <= pPrivate->ulColsVisible ) {
                    pPrivate->ulOffsetX = 0;
                    WinSendMsg( pPrivate->hwndSBH, SBM_SETTHUMBSIZE, MPFROM2SHORT( 1, 1 ), 0 );
                    WinSendMsg( pPrivate->hwndSBH, SBM_SETSCROLLBAR, 0, MPFROM2SHORT( 0, 1 ));
                }
                else {
                    sMaxH = max( 0, (LONG)( pPrivate->ulColsTotal - pPrivate->ulColsVisible ));
                    WinSendMsg( pPrivate->hwndSBH, SBM_SETTHUMBSIZE,
                                MPFROM2SHORT( pPrivate->ulColsVisible, pPrivate->ulColsTotal ), 0 );
                    WinSendMsg( pPrivate->hwndSBH, SBM_SETSCROLLBAR,
                                MPFROMSHORT( pPrivate->ulOffsetX ),
                                MPFROM2SHORT( 0, sMaxH ));
                }
            }

            WinInvalidateRect( hwnd, NULL, FALSE );
            break;


        case WM_HSCROLL:
            pPrivate = WinQueryWindowPtr( hwnd, 0 );
            if ( pPrivate->ulColsTotal < pPrivate->ulColsVisible )
                break;
            switch ( SHORT2FROMMP( mp2 )) {
                case SB_LINELEFT:
                    lInc = -1;
                    break;
                case SB_LINERIGHT:
                    lInc = 1;
                    break;
                case SB_PAGELEFT:
                    lInc = min( -1, (LONG)(-(pPrivate->ulColsVisible)) + 1 );
                    break;
                case SB_PAGERIGHT:
                    lInc = max(  1, (LONG)(pPrivate->ulColsVisible) - 1 );
                    break;
                case SB_SLIDERPOSITION:
                case SB_SLIDERTRACK:
                    lInc = SHORT1FROMMP( mp2 ) - pPrivate->ulOffsetX;
                    break;
                default:
                    return WinDefWindowProc( hwnd, msg, mp1, mp2 );
            }
            sMaxH = max( 0, (LONG)(pPrivate->ulColsTotal - pPrivate->ulColsVisible) );
            if ((LONG)( pPrivate->ulOffsetX + lInc ) < 0 )
                lInc = -(pPrivate->ulOffsetX);
            else if (( pPrivate->ulOffsetX + lInc ) > sMaxH )
                lInc = sMaxH - pPrivate->ulOffsetX;
            pPrivate->ulOffsetX += lInc;
            if ( lInc != 0 ) {
                rcl = pPrivate->rclView;
                rcl.xRight += 1;
                rcl.yBottom -= 1;
                WinSendMsg( pPrivate->hwndSBH, SBM_SETPOS,
                            MPFROM2SHORT( pPrivate->ulOffsetX, 0 ), 0 );
#if 1
                WinScrollWindow( hwnd, -(lInc * pPrivate->ulUnitWidth), 0,
                                 &rcl, &rcl, NULLHANDLE, NULL, SW_INVALIDATERGN );
#else
                WinInvalidateRect( hwnd, &rcl, FALSE );
#endif
            }
            break;


        case WM_VSCROLL:
            pPrivate = WinQueryWindowPtr( hwnd, 0 );
            if ( pPrivate->ulLinesTotal < pPrivate->ulLinesVisible )
                break;
            switch ( SHORT2FROMMP( mp2 )) {
                case SB_LINEUP:
                    lInc = -1;
                    break;
                case SB_LINEDOWN:
                    lInc = 1;
                    break;
                case SB_PAGEUP:
                    lInc = min( -1, (LONG)(-(pPrivate->ulLinesVisible)) + 1 );
                    break;
                case SB_PAGEDOWN:
                    lInc = max(  1, (LONG)(pPrivate->ulLinesVisible) - 1 );
                    break;
                case SB_SLIDERPOSITION:
                case SB_SLIDERTRACK:
                    lInc = SHORT1FROMMP( mp2 ) - pPrivate->ulOffsetY;
                    break;
                default:
                    return WinDefWindowProc( hwnd, msg, mp1, mp2 );
            }
            sMaxV = max( 0, (LONG)(pPrivate->ulLinesTotal - pPrivate->ulLinesVisible) );
            if ((LONG)( pPrivate->ulOffsetY + lInc ) < 0 )
                lInc = -(pPrivate->ulOffsetY);
            else if (( pPrivate->ulOffsetY + lInc ) > sMaxV )
                lInc = sMaxV - pPrivate->ulOffsetY;
            pPrivate->ulOffsetY += lInc;
            if ( lInc != 0 ) {
                rcl = pPrivate->rclView;
                WinSendMsg( pPrivate->hwndSBV, SBM_SETPOS,
                            MPFROM2SHORT( pPrivate->ulOffsetY, 0 ), 0 );
                WinScrollWindow( hwnd, 0, lInc * pPrivate->ulUnitHeight,
                                 &rcl, &rcl, NULLHANDLE, NULL, SW_INVALIDATERGN );
            }
            break;


   }

    return ( WinDefWindowProc( hwnd, msg, mp1, mp2 ));
}


/* ------------------------------------------------------------------------- *
 * DrawEditorText                                                            *
 *                                                                           *
 * (Re)paint the visible portion of the editor text within the viewport.     *
 * The position (pptl) always starts at the upper left; on return it will be *
 * set to the point after the last drawn character (where the next character *
 * would be drawn if it existed).                                            *
 *                                                                           *
 * This function currently supports only left-to-right text direction.       *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HWND         hwnd: Control's window handle                     (I)      *
 *   HPS          hps : Handle of the current presentation space    (I)      *
 *   PPOINTL      pptl: Position of the next character to be drawn  (O)      *
 *   PUMLEPDATA   pCtl: Private control data                        (I)      *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   Number of characters drawn.                                             *
 * ------------------------------------------------------------------------- */
ULONG DrawEditorText( HWND hwnd, HPS hps, PPOINTL pptl, PUMLEPDATA pCtl )
{
    ULONG ulTotal,      // Total length of the text (in UniChars or bytes)
          ulOffset,     // Starting text offset
          ulLength;     // Length of the current text sequence to draw


    // Set the starting point to the viewport top minus the scroll offsets
    pptl->x = pCtl->rclView.xLeft - ( pCtl->ulUnitWidth * pCtl->ulOffsetX );
    pptl->y = pCtl->rclView.yTop - pCtl->ulUnitHeight +
              ( pCtl->ulUnitHeight * pCtl->ulOffsetY );

    SetFontFromPP( hps, hwnd, pCtl->lDPI, &(pCtl->fattrs) );

    // Unicode display mode
    if ( pCtl->usDispCP == 1200 ) {
        ulTotal  = BYTEOFF_TO_UPOS( TextLength( pCtl->text ));
        ulOffset = 0;
        ulLength = ulTotal;

        // TODO break this up into ranges defined by the attribute list
        ulOffset += DrawUnicodeTextSequence( hwnd, hps, pptl, pCtl, ulOffset, ulLength );
    }

    // Codepage display mode (TODO)
    else {
        ulTotal = TextLength( pCtl->text );
        ulOffset = 0;
        ulLength = ulTotal;

        // TODO break this up into ranges defined by the attribute list
        ulOffset += DrawCodepageTextSequence( hwnd, hps, pptl, pCtl, ulOffset, ulLength, pCtl->usDispCP );
    }

    return ulOffset;
}


/* ------------------------------------------------------------------------- *
 * DrawUnicodeTextSequence                                                   *
 *                                                                           *
 * Draw a given text sequence inside the viewport, from the specified start  *
 * position.  (A 'sequence' is a subset of the editor text which has a       *
 * common set of style/color/BiDi attributes.)  On return, the position will *
 * be set to the point after the last drawn character (where the next        *
 * character would be drawn if it existed).                                  *
 *                                                                           *
 * Due to the limitations of GpiCharStringPosAt, this function has to split  *
 * the text sequence into 512-byte segments if necessary, drawing each one   *
 * in turn.  This process should be transparent to the caller.               *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HWND       hwnd    : Control's window handle                     (I)    *
 *   HPS        hps     : Handle of the current presentation space    (I)    *
 *   PPOINTL    pptl    : Position of the next character to be drawn  (O)    *
 *   PUMLEPDATA pCtl    : Private control data                        (I)    *
 *   ULONG      ulStart : Text buffer offset of the first character   (I)    *
 *   ULONG      ulLength: Number of characters to draw                (I)    *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   The starting offset plus the number of characters drawn.  This is       *
 *   equivalent to the text buffer position (in UniChars) immediately after  *
 *   that of the last character drawn.                                       *
 * ------------------------------------------------------------------------- */
ULONG DrawUnicodeTextSequence( HWND hwnd, HPS hps, PPOINTL pptl, PUMLEPDATA pCtl, ULONG ulStart, ULONG ulLength )
{
    UniChar    suText[ UCS_MAX_RENDER+1 ];  // Text of current segment
    LONG       lRC;          // GPI return code
    LONG       lFG;          // Current foreground (text) colour
    ULONG      ulBreakIdx;   // Buffer index of the next line-break offset
    ULONG      ulChars,      // Number of characters in the current line
               cbChars,      // Number of bytes returned from TextSequence()
               ulStrip,      // Number of trailing characters to strip
               ulDraw,       // Number of UniChars to draw in the current iteration
               ulBreakPos;   // Character offset of the next line-break
    PLBOBUFFER pLB;          // Pointer to line-break offsets buffer
    BOOL       fLineBreak;   // Does the current segment end in a line-break?


    // TODO set the configured attributes for this sequence
    if ( ! WinQueryPresParam( hwnd, PP_FOREGROUNDCOLOR, PP_FOREGROUNDCOLORINDEX,
                              NULL, sizeof( lFG ), &lFG, QPF_ID2COLORINDEX ))
        lFG = SYSCLR_WINDOWTEXT;
    GpiSetColor( hps, lFG );
    GpiSetTextAlignment( hps, TA_LEFT, TA_BOTTOM );
    GpiSetCp( hps, 1200 );

    ulBreakIdx = 0;
    pLB = &(pCtl->breaks);
    while (( ulStart < ulLength )) {
        fLineBreak = TRUE;          // assume this by default

        // Locate the next line break (hard or soft) after the current position
        ulBreakPos = BYTEOFF_TO_UPOS( LineBuffer_ItemAt( pLB, ulBreakIdx ));
        if ( ulBreakPos == LB_INVALID_POSITION ) {
            fLineBreak = FALSE;
            ulChars = ulLength - ulStart;
        }
        else ulChars = ulBreakPos - ulStart;

        /* Now see how many characters to draw - either until the next wrap
         * point, or a maximum of UCS_MAX_RENDER (because GpiCharStringPosAt
         * can't accept more than that in one call).
         */
        do {
            if ( ulChars > UCS_MAX_RENDER ) {
                ulDraw = UCS_MAX_RENDER;
                fLineBreak = FALSE;
            }
            else ulDraw = ulChars;
            ulStrip = 0;

            // Retrieve the calculated character range from the text buffer
            cbChars = TextSequence( pCtl->text, (PCH) suText,
                                    UPOS_TO_BYTEOFF( ulStart ),
                                    UPOS_TO_BYTEOFF( ulDraw ));
            if ( !cbChars ) return ulStart;
            ulDraw = BYTEOFF_TO_UPOS( cbChars );
            suText[ ulDraw ] = 0;

            // Don't display any trailing newlines or spaces following the break
            if ( fLineBreak ) {
                ULONG c = ulDraw;
                while ( c && SKIP_WRAPPED_CHAR( suText[ c-1 ] ))
                    c--;
                ulStrip = ulDraw - c;
            }

            // Finally, draw the indicated number of characters
            if ( pptl->y <= (pCtl->rclView.yTop + pCtl->ulUnitHeight )) {
#if 0
                LONG  alInc[ UCS_MAX_RENDER ] = {0};
                if ( FixedWidthIncrements( suText, ulDraw - ulStrip, alInc, pCtl->fm ))
                    lRC = GpiCharStringPosAt( hps, pptl, &pCtl->rclView, CHS_CLIP | CHS_VECTOR,
                                              UPOS_TO_BYTEOFF( ulDraw - ulStrip ),
                                              (PCH) suText, alInc );
                else
//                    lRC = GpiCharStringPosAt( hps, pptl, &pCtl->rclView, CHS_CLIP,
//                                              UPOS_TO_BYTEOFF( ulDraw - ulStrip ),
//                                              (PCH) suText, NULL );
//                    lRC = GpiTabbedCharStringAt( hps, pptl, &pCtl->rclView, CHS_CLIP, UPOS_TO_BYTEOFF( ulDraw - ulStrip ), (PCH) suText, 1, &(pCtl->ulTabSize), 0 );
#endif
                    lRC = DrawTabbedUnicodeText( hps, pptl, pCtl->rclView, pCtl->fm, pCtl->ulTabSize, suText, ulDraw - ulStrip );
                if ( lRC != GPI_OK ) return ulStart;
            }
            ulStart += ulDraw;

        } while ( ulStart < ulChars );

        // If we're not finished, move to the next line and repeat
        if (( ulStart < ulLength ) && ( pptl->y >= pCtl->rclView.yBottom )) {
            pptl->x = pCtl->rclView.xLeft - ( pCtl->ulUnitWidth * pCtl->ulOffsetX );
            pptl->y -= pCtl->ulUnitHeight;
            ulBreakIdx += 1;
        }
    }

    return ulStart;
}


/* ------------------------------------------------------------------------- *
 * DrawCodepageTextSequence                                                  *
 *                                                                           *
 * Draw a given text sequence inside the viewport, from the specified start  *
 * position.  (A 'sequence' is a subset of the editor text which has a       *
 * common set of codepage/style/color/BiDi attributes.)  On return, the      *
 * position will be set to the point after the last drawn character (where   *
 * the next character would be drawn if it existed).                         *
 *                                                                           *
 * Due to the limitations of GpiCharStringPosAt, this function has to split  *
 * the text sequence into 512-byte segments if necessary, drawing each one   *
 * in turn.  This process should be transparent to the caller.               *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HWND       hwnd    : Control's window handle                     (I)    *
 *   HPS        hps     : Handle of the current presentation space    (I)    *
 *   PPOINTL    pptl    : Position of the next character to be drawn  (O)    *
 *   PUMLEPDATA pCtl    : Private control data                        (I)    *
 *   ULONG      ulStart : Text buffer offset of the first character   (I)    *
 *   ULONG      ulLength: Number of characters to draw                (I)    *
 *   ULONG      ulCP    : Codepage to render the text in              (I)    *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   The starting offset plus the number of characters drawn.  This is       *
 *   equivalent to the text buffer position (in UniChars) immediately after  *
 *   that of the last character drawn.                                       *
 * ------------------------------------------------------------------------- */
ULONG DrawCodepageTextSequence( HWND hwnd, HPS hps, PPOINTL pptl, PUMLEPDATA pCtl, ULONG ulStart, ULONG ulLength, ULONG ulCP )
{
    UCHAR      szText[ CB_MAX_RENDER+1 ];  // Text of current segment
    LONG       lRC;          // GPI return code
    LONG       lFG;          // Current foreground (text) colour
    ULONG      ulBreakIdx;   // Buffer index of the next line-break offset
    ULONG      ulChars,      // Number of characters in the current line
               cbChars,      // Number of bytes returned from TextSequence()
               ulStrip,      // Number of trailing characters to strip
               ulDraw,       // Number of chars to draw in the current iteration
               ulBreakPos;   // Character offset of the next line-break
    PLBOBUFFER pLB;          // Pointer to line-break offsets buffer
    BOOL       fLineBreak;   // Does the current segment end in a line-break?


    // TODO set the configured attributes for this sequence
    if ( ! WinQueryPresParam( hwnd, PP_FOREGROUNDCOLOR, PP_FOREGROUNDCOLORINDEX,
                              NULL, sizeof( lFG ), &lFG, QPF_ID2COLORINDEX ))
        lFG = SYSCLR_WINDOWTEXT;
    GpiSetColor( hps, lFG );
    GpiSetTextAlignment( hps, TA_LEFT, TA_BOTTOM );
    GpiSetCp( hps, ulCP );

    ulBreakIdx = 0;
    pLB = &(pCtl->breaks);
    while (( ulStart < ulLength )) {
        fLineBreak = TRUE;          // assume this by default

        // Locate the next line break (hard or soft) after the current position
        ulBreakPos = LineBuffer_ItemAt( pLB, ulBreakIdx );
        if ( ulBreakPos == LB_INVALID_POSITION ) {
            fLineBreak = FALSE;
            ulChars = ulLength - ulStart;
        }
        else ulChars = ulBreakPos - ulStart;

        /* Now see how many characters to draw - either until the next wrap
         * point, or a maximum of CB_MAX_RENDER (because GpiCharStringPosAt
         * can't accept more than that in one call).
         */
        do {
            if ( ulChars > CB_MAX_RENDER ) {
                ulDraw = CB_MAX_RENDER;
                fLineBreak = FALSE;
            }
            else ulDraw = ulChars;
            ulStrip = 0;

            // Retrieve the calculated character range from the text buffer
            cbChars = TextSequence( pCtl->text, (PCH) szText, ulStart, ulDraw );
            if ( !cbChars ) return ulStart;
            ulDraw = cbChars;
            szText[ ulDraw ] = 0;

            // Don't display any trailing newlines or spaces following the break
            if ( fLineBreak ) {
                ULONG c = ulDraw;
                while ( c && SKIP_WRAPPED_CHAR( szText[ c-1 ] ))
                    c--;
                ulStrip = ulDraw - c;
            }

            // Finally, draw the indicated number of characters
            if ( pptl->y <= (pCtl->rclView.yTop + pCtl->ulUnitHeight )) {
                lRC = GpiCharStringPosAt( hps, pptl, &pCtl->rclView, CHS_CLIP,
                                          ulDraw - ulStrip, (PCH) szText, NULL );
                lRC = GpiTabbedCharStringAt( hps, pptl, &pCtl->rclView, CHS_CLIP, ulDraw - ulStrip,
                                             (PCH) szText, 1, &(pCtl->ulTabSize), 0 );
                if ( lRC != GPI_OK ) return ulStart;
            }
            ulStart += ulDraw;

        } while ( ulStart < ulChars );

        // If we're not finished, move to the next line and repeat
        if (( ulStart < ulLength ) && ( pptl->y >= pCtl->rclView.yBottom )) {
            pptl->x = pCtl->rclView.xLeft - ( pCtl->ulUnitWidth * pCtl->ulOffsetX );
            pptl->y -= pCtl->ulUnitHeight;
            ulBreakIdx += 1;
        }
    }

    return ulStart;
}


/* ------------------------------------------------------------------------- *
 * EnumerateLines                                                            *
 *                                                                           *
 * Parses the editor text from the specified offset to the end, looking for  *
 * hard line breaks and saving their positions in the line offset buffer.    *
 * Also makes a note of the longest line (display-wise).                     *
 *                                                                           *
 * This function does not take word wrapping (a.k.a. soft line breaks) into  *
 * account, and therefore is not useful if MLS_WORDWRAP is set.  Use the     *
 * ReflowEditorText() function to recalculate all hard and soft line breaks. *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HPS        hps     : Handle of the current presentation space      (I)  *
 *   PUMLEPDATA pPrivate: Private control data                          (I)  *
 *   ULONG      cbStart : Starting byte offset in the editor text       (I)  *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   Number of lines found (i.e. one plus the number of hard line breaks).   *
 * ------------------------------------------------------------------------- */
ULONG EnumerateLines( HPS hps, PUMLEPDATA pPrivate, ULONG cbStart )
{
    PLBOBUFFER pLB;          // Pointer to line-break offsets buffer
    ULONG      ulTotal,      // Total length of the text (in UniChars)
               ulStart,      // Starting UniChar offset
               ulBreakIdx,   // Current index in the line buffer
               ulLongestIdx, // Index of the longest line
               i;
    LONG       lWidth,       // Display width of current line
               lLongest;     // Display width of longest line
    UniChar    uc;


    // Clear all stored line-breaks after the current starting offset
    pLB     = &(pPrivate->breaks);
    ulBreakIdx = LineBuffer_FindPosition( pLB, 0, pLB->ulSize-1, cbStart );
    LineBuffer_Clear( pLB, ulBreakIdx );

    // The text in the buffer is always UCS-2
    ulTotal  = BYTEOFF_TO_UPOS( TextLength( pPrivate->text ));
    if ( !ulTotal ) return 0;

    ulStart  = BYTEOFF_TO_UPOS( cbStart );
    lLongest = 0;
    for ( i = ulStart; i < ulTotal; i++ ) {
        uc = TextWCharAt( pPrivate->text, i );
        if ( NEWLINE_CHAR( uc )) {

            // End-of-line found, save the position in the line buffer
            if (( uc == 0xD ) &&
                ((i+1) < ulTotal ) && ( TextWCharAt( pPrivate->text, i+1 ) == 0xA ))
                i++;
            LineBuffer_Insert( pLB, UPOS_TO_BYTEOFF( i+1 ), ulBreakIdx );

            // Now calculate the line's display-width
            lWidth = GetLineExtent( hps, pPrivate,
                                    ulBreakIdx ?
                                        LineBuffer_ItemAt( pLB, ulBreakIdx-1 ) :
                                        cbStart,
                                    UPOS_TO_BYTEOFF( i - ulStart ));
            if ( lWidth > lLongest ) {
                lLongest = lWidth;
                ulLongestIdx = ulBreakIdx ? ( ulBreakIdx - 1 ) : 0;
            }
            ulBreakIdx++;
        }
    }

    DEBUG_PRINTF("Enumerated %u lines (%u characters total)\n", ulBreakIdx, ulTotal );
    DEBUG_PRINTF(" - (longest is line %u (%u pels)\n", ulLongestIdx, lLongest );

    pPrivate->ulLongest = ulLongestIdx;
    pPrivate->ulColsTotal = pPrivate->ulUnitWidth ?
                                (ULONG) ( lLongest / pPrivate->ulUnitWidth ) : 0;

    return ( LineBuffer_Count( &(pPrivate->breaks) ) + 1 );
}


/* ------------------------------------------------------------------------- *
 * GetLineExtent                                                             *
 *                                                                           *
 * Determine the display-width of the specified text range, up to the given  *
 * length.  The text is assumed to contain no line-breaks.                   *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HPS        hps     : Handle of the current presentation space      (I)  *
 *   PUMLEPDATA pPrivate: Private control data                          (I)  *
 *   ULONG      cbStart : Starting byte offset in the editor text       (I)  *
 *   ULONG      cbLength: Number of bytes to measure                    (I)  *
 *                                                                           *
 * RETURNS: LONG                                                             *
 *   The number of horizontal pels required to display the given string.     *
 * ------------------------------------------------------------------------- */
LONG GetLineExtent( HPS hps, PUMLEPDATA pPrivate, ULONG cbStart, ULONG cbLength )
{
    CHAR  szText[ CB_MAX_RENDER + 2 ];  // Buffer for holding text being checked
    ULONG cbRemaining,                  // Number of bytes not yet processed
          cbChars;                      // Number of bytes to process
    LONG  lExtent;                      // Calculated display width


    lExtent = 0;
    do {
        cbRemaining = cbLength - cbStart;
        cbChars = ( cbRemaining > CB_MAX_RENDER )? CB_MAX_RENDER: cbRemaining;
        cbChars = TextSequence( pPrivate->text, szText, cbStart, cbChars );
        if ( !cbChars ) break;
        szText[ cbChars ] = 0;
        szText[ cbChars+1 ] = 0;    // In case string is UCS-encoded

        // Truncate the checked characters at the first line-break
        if ( pPrivate->usDispCP == 1200 ) {
            UniChar auLineBreaks[] = { 0xA, 0xB, 0xC, 0xD, 0x2028, 0x2029 };
            ULONG ulStop = UniStrcspn( (UniChar *) szText, auLineBreaks );
            cbChars = UPOS_TO_BYTEOFF( ulStop );
        }
        else {
            CHAR acLineBreaks[] = { 0xA, 0xB, 0xC, 0xD };
            cbChars = strcspn( szText, acLineBreaks );
        }

        // Check the width of this segment
        // TODO get attribute codepage if not in UCS-2 mode
        GpiSetCp( hps, (ULONG) pPrivate->usDispCP );
        lExtent += QueryTextWidth( hps, szText, cbChars, pPrivate->fm, pPrivate->ulTabSize, pPrivate->usDispCP );
        cbStart += cbChars;

    } while ( cbStart < cbLength );

    return lExtent;
}


/* ------------------------------------------------------------------------- *
 * InsertText                                                                *
 *                                                                           *
 * Inserts codepage-formatted text into the edit window at the current       *
 * insertion point.  Called by the MLM_INSERT message handler.               *
 *                                                                           *
 * All text, no matter what its source codepage, and no matter what the      *
 * current display codepage, is stored in the text buffer as UCS-2.  It may  *
 * or may not be converted to back to the original codepage, or to another   *
 * codepage entirely, at display time.                                       *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HWND   hwnd   : Control's window handle                                 *
 *   PSZ    pszText: Multi-byte text to insert                               *
 *   USHORT usCP   : Codepage in which pszText is encoded                    *
 *   USHORT fsAttr : Additional text attributes (style, direction)           *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   Number of characters inserted.                                          *
 * ------------------------------------------------------------------------- */
ULONG InsertText( HWND hwnd, PSZ pszText, USHORT usCP, USHORT fsAttr )
{
    UconvObject uconv;
    PUMLEPDATA  pCtl;
    HPS         hps;
    IPT         ipt;
    UniChar     uniCP[ CPSPEC_MAXZ ] = {0},
                *pout,
                *psuText;
    PSZ         pin;
    size_t      stIn, stOut, stSub;
    ULONG       cbOffset,
                aulCP[ 3 ] = {0},
                pcbCP,
                ulRC;
    APIRET      rc;


    pCtl = WinQueryWindowPtr( hwnd, 0 );
    if ( !pCtl || !pszText ) return 0;

    // Query the current process codepage
    rc = DosQueryCp( sizeof( aulCP ), aulCP, &pcbCP );

    ipt = pCtl->ctldata.iptCursor;
    if ( ipt > TextLength( pCtl->text )) ipt = 0;

    /* Given the IPT, determine the byte offset within the text buffer.
     *
     * At the moment, we assume that the IPT is a UCS-2 offset; this MAY
     * change once we implement positioning/navigation.  (To elaborate: I
     * still have to decide how an IPT will relate to the text as rendered,
     * which may be any codepage or even a combination, vs. the text as
     * stored in the buffer, which is always UCS-2.)
     */
    cbOffset = UPOS_TO_BYTEOFF( ipt );

    // Convert the source text into UCS-2
    if ( usCP )
        ulRC = UniMapCpToUcsCp( (ULONG) usCP, uniCP, CPNAME_MAXZ );
    // TODO should eventually implement a cache of previously-used UconvObjects
    UniStrncat( uniCP, L"@map=crlf,path=no", CPSPEC_MAXZ-1 );
    ulRC = UniCreateUconvObject( uniCP, &uconv );
    if ( ulRC != ULS_SUCCESS ) return 0;

    stIn  = strlen( pszText );
    stOut = stIn * 4;
    stSub = 0;
    rc = DosAllocMem( (PPVOID) &psuText,
                      ( stOut + 1 ) * sizeof( UniChar ),
                      PAG_READ | PAG_WRITE | PAG_COMMIT );
    if ( rc != NO_ERROR ) {
        UniFreeUconvObject( uconv );
        return 0;
    }

    pin  = pszText;
    pout = psuText;
    ulRC = UniUconvToUcs( uconv, (PPVOID) &pin, &stIn, &pout, &stOut, &stSub );
    if ( ulRC == ULS_SUCCESS ) {

        // Text converted successfully, now insert it into the global buffer
        ulRC = TextInsert( pCtl->text, (PCH) psuText, UPOS_TO_BYTEOFF( ipt ),
                           UPOS_TO_BYTEOFF( UniStrlen( psuText )));
        if ( ulRC ) {
            // Update the line buffer
            hps = WinGetPS( hwnd );
            SetFontFromAttrs( hps, pCtl->lDPI, &(pCtl->fattrs), pCtl->fm );
            //SetFontFromPP( hps, hwnd, pCtl->lDPI, &(pCtl->fattrs) );
            if (( pCtl->flStyle & MLS_WORDWRAP ) && RECTL_WIDTH( pCtl->rclView )) {
                POINTL ptl;
                // TODO Only reflow the buffer contents from cbOffset (this will
                //      require deriving ptl from ipt... somehow)
                ptl.x = pCtl->rclView.xLeft;
                ptl.y = pCtl->rclView.yTop - pCtl->ulUnitHeight;
                pCtl->ulLinesTotal = ReflowEditorText( hps, pCtl, ptl, 0 );
            }
            else if ( RECTL_WIDTH( pCtl->rclView )) {
                // TODO A possible optimization here: if psuText contains no linebreaks,
                //      add some parameter telling EnumerateLines to only re-check the
                //      longest line by comparing the current line with the old longest.
                pCtl->ulLinesTotal = EnumerateLines( hps, pCtl, cbOffset );
            }
            WinReleasePS( hps );

            /* If the previous or following text attribues don't match the
             * current ones, create a new text-attribute object for the
             * inserted text.  TODO
             */
        }
    }
    else ulRC = 0;

    UniFreeUconvObject( uconv );
    DosFreeMem( psuText );

    return ( ulRC );
}


/* ------------------------------------------------------------------------- *
 * ReflowEditorText                                                          *
 *                                                                           *
 * Reflows all editor text, from the specified starting point to the end.    *
 * This recalculates the position of all line breaks and wrap points, and    *
 * populates the line offset buffer accordingly.  If MLS_WORDWRAP is set,    *
 * this needs to be done whenever text is added, when the control is resized *
 * horizontally, when the text font is changed, or when the MLS_WORDWRAP     *
 * style is activated.                                                       *
 *                                                                           *
 * This function should not be used when the MLS_WORDWRAP style is not set.  *
 * In that case, EnumerateLines() should be used instead.                    *
 *                                                                           *
 * Note that the text has to be read into a contiguous (UCS-2) string from   *
 * the buffer's own (non-contiguous) internal format, before we can process  *
 * it.  To avoid having to duplicate the entire buffer contents in memory    *
 * (which could be very expensive for large buffers), the text is processed  *
 * in consecutive segments of REFLOW_SEGMENT_LENGTH UniChars until the end   *
 * of the text is reached.                                                   *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HPS        hps     : Handle of the current presentation space      (I)  *
 *   PUMLEPDATA pPrivate: Private control data                          (I)  *
 *   POINTL     ptl     : Starting character's position in the window   (I)  *
 *   ULONG      cbStart : Starting byte offset within the editor text   (I)  *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   Number of lines found (i.e. one plus the number of line breaks).        *
 * ------------------------------------------------------------------------- */
ULONG ReflowEditorText( HPS hps, PUMLEPDATA pPrivate, POINTL ptl, ULONG cbStart )
{
    PLBOBUFFER pLB;          // Pointer to line-break offsets buffer
    UniChar    suText[ REFLOW_SEGMENT_LENGTH+1 ];
    ULONG      ulTotal,      // Total length of the text (in UniChars)
               ulStart,      // Starting offset of the current segment
               ulRemaining,  // Number of characters left to process
               cbChars,      // Number of bytes returned by TextSequence()
               ulChars;      // Number of characters in the current segment


    // Clear all stored line-breaks after the current starting offset
    pLB = &(pPrivate->breaks);
    LineBuffer_Clear( pLB,
                      LineBuffer_FindPosition( pLB, 0, pLB->ulSize-1, cbStart ));

    // The text in the buffer is always UCS-2, i.e. two bytes per character
    ulTotal = BYTEOFF_TO_UPOS( TextLength( pPrivate->text ));
    if ( !ulTotal ) return 0;

//#ifdef DEBUG_LOG
//        fprintf( dbg, "Reflowing %u characters\n", ulTotal );
//#endif

    ulStart = BYTEOFF_TO_UPOS( cbStart );
    do {
        ulRemaining = ulTotal - ulStart;
        ulChars = ( ulRemaining > REFLOW_SEGMENT_LENGTH ) ?
                    REFLOW_SEGMENT_LENGTH : ulRemaining;

        // Retrieve the current character range from the text buffer
        cbChars = TextSequence( pPrivate->text, (PCH) suText,
                                UPOS_TO_BYTEOFF( ulStart ),
                                UPOS_TO_BYTEOFF( ulChars ));
        ulChars = BYTEOFF_TO_UPOS( cbChars );
        if ( !ulChars ) break;

        // Now reflow this segment
        suText[ ulChars ] = 0;
        ReflowUnicodeTextSequence( hps, &ptl, pPrivate, suText, 0 );
        ulStart += ulChars;

    } while ( ulStart < ulTotal );

//#ifdef DEBUG_LOG
//    LineBuffer_Dump( dbg, pPrivate->breaks, FALSE );
//#endif

    return LineBuffer_Count( &(pPrivate->breaks) );
}


/* ------------------------------------------------------------------------- *
 * ReflowUnicodeTextSequence                                                 *
 *                                                                           *
 * Reflow a text sequence within the specified rectangle, starting from the  *
 * specified point, wrapping it as needed.  The position (pptl) is updated   *
 * to the point after the last drawn character where the theoretical next    *
 * character should be drawn, if it existed.                                 *
 *                                                                           *
 * The text must be UCS-2 encoded, and terminated by a null UniChar.         *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HPS        hps     : Handle of the current presentation space      (I)  *
 *   PPOINTL    pptl    : Position of the next character to be drawn  (I/O)  *
 *   PUMLEPDATA pCtl    : Private control data                          (I)  *
 *   UniChar    *psuText: The Unicode text sequence to reflow           (I)  *
 *   ULONG      cbOffset: Corresponding offset within the editor text   (I)  *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   Number of characters (UniChars) processed.                              *
 * ------------------------------------------------------------------------- */
//  TODO:
//  - optimize reflowing after the next hard linebreak (subsequent offsets
//    need only be incremented by the size of the added text)


ULONG ReflowUnicodeTextSequence( HPS hps, PPOINTL pptl, PUMLEPDATA pCtl, UniChar *psuText, ULONG cbOffset )
{
    PLBOBUFFER pLB;         // Pointer to line-break offsets buffer
    ULONG      ulStart,     // Starting character index within our text sequence
               ulTotal,     // Total length of the text sequence, in UniChars
               cbDraw,      // Byte-count returned by FitTextWidth
               ulDraw,      // Number of UniChars to draw before wrapping
               cbAbs,       // Absolute byte address of the wrap position
//               ulTabOffset, // Current tabstop position
               ulLBIdx;     // Position of current line-break in the buffer


    // Need to set the codepage for the GPI functions called in FitTextWidth
    GpiSetCp( hps, 1200 );
    // TODO need to set the font for the current attributes as well

    pLB         =  &(pCtl->breaks);
    ulLBIdx     = 0;
//    ulTabOffset = 0;
    ulStart     = 0;
    ulTotal     = UniStrlen( psuText );

    while (( ulStart < ulTotal )) {
        ulDraw = ulTotal - ulStart;

        // Find out how many characters fit on the current line
        cbDraw = FitTextWidth( hps, (PCH) (psuText + ulStart),
                               UPOS_TO_BYTEOFF( ulDraw ),
                               RECTL_WIDTH( pCtl->rclView ), pptl,
                               pCtl->ulTabSize, pCtl->fm,
                               pCtl->usDispCP, (PCH) (pCtl->dbcs) );
        ulDraw = BYTEOFF_TO_UPOS( cbDraw );

        // If no characters fit and we're at the start of the line, give up
        if ( !ulDraw && ( pptl->x == pCtl->rclView.xLeft ))
            return ulStart;

        /* Extend the length to include any trailing whitespace
         * (which won't be displayed at the end of a line anyway)
         */
        while ((( ulStart + ulDraw ) < ulTotal ) &&
               ( SKIP_WRAPPED_CHAR( psuText[ (ulStart+ulDraw) ] )))
            ulDraw++;

        // Try to end the line on a legally-wrappable character if possible
        if ((( ulStart + ulDraw ) < ulTotal ) &&
            (( !UCS_WRAPPABLE_AFTER( psuText[ (ulStart+ulDraw)-1 ] ) &&
               !UCS_WRAPPABLE_BEFORE( psuText[ (ulStart+ulDraw) ] )) ||
              UCS_WRAP_FORBIDDEN( psuText[ (ulStart+ulDraw)] )))
        {
            ulDraw = FindBreakPosition( (PCH) (psuText + ulStart),
                                         ulDraw - 1, 1200 ) + 1;
        }
        ulStart += ulDraw;
        cbAbs = cbOffset + UPOS_TO_BYTEOFF( ulStart );

        /* Add the calculated break point to the line-break index (checking
         * first to make sure the current value is not a duplicate).
         */
        if ( !ulLBIdx )
            ulLBIdx = LineBuffer_FindPosition( pLB, 0,
                                               LineBuffer_Count( pLB ) - 1,
                                               cbAbs );
        if ( !ulLBIdx || ( LineBuffer_ItemAt( pLB, ulLBIdx-1 ) != cbAbs ))
            LineBuffer_Insert( pLB, cbAbs, ulLBIdx++ );

        // If we're not finished, move to the next line and continue
        if ( ulStart < ulTotal ) {
            pptl->x = pCtl->rclView.xLeft;
            pptl->y -= pCtl->ulUnitHeight;
        }

    }

    return ulStart;
}


/* ------------------------------------------------------------------------- *
 * UpdateFont                                                                *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HWND       hwnd    : Window handle                                      *
 *   PUMLEPDATA pPrivate: Private control data                               *
 *                                                                           *
 * RETURNS: N/A                                                              *
 * ------------------------------------------------------------------------- */
void UpdateFont( HWND hwnd, PUMLEPDATA pPrivate )
{
    HPS    hps;
    CHAR   szFont[ FACESIZE+1 ];  // name of the current font
    LONG   lMargin;               // margin to allow around clipping area
    SHORT  sMax;                  // scrollbar max
    RECTL  rcl;                   // control's window area
    SWP    swp;                   // used to query the scrollbar sizes

    WinQueryPresParam( hwnd, PP_FONTNAMESIZE, 0,
                       NULL, FACESIZE+1, szFont, QPF_NOINHERIT );
    hps = WinGetPS( hwnd );

    SetFontFromPP( hps, hwnd, pPrivate->lDPI, &(pPrivate->fattrs) );
    GpiQueryFontMetrics( hps, sizeof(FONTMETRICS), &(pPrivate->fm) );
#ifdef UNICODE_ALWAYS
    pPrivate->usDispCP     = 1200;
#else
    pPrivate->usDispCP     = ( pPrivate->fm.fsType & FM_TYPE_UNICODE ) ? 1200 : 0;
#endif
    pPrivate->ulUnitHeight = pPrivate->fm.lMaxAscender + pPrivate->fm.lExternalLeading + 2;
    pPrivate->ulUnitWidth  = pPrivate->fm.lAveCharWidth;
    pPrivate->ulTabSize    = pPrivate->fm.lAveCharWidth * 8;

    // Update the font-dependent dimensions
    WinQueryWindowPos( pPrivate->hwndSBV, &swp );
    WinQueryWindowRect( hwnd, &rcl );
    WinInflateRect( hps, &rcl, -4, -4 );
    lMargin = pPrivate->fm.lAveCharWidth / 2;
    pPrivate->rclView.xLeft   = rcl.xLeft + lMargin;
    pPrivate->rclView.yBottom = rcl.yBottom + pPrivate->fm.lExternalLeading +
                                (( pPrivate->hwndSBH ) ? SB_SIZE : 0 );
    pPrivate->rclView.xRight  = rcl.xRight - lMargin -
                                (( pPrivate->hwndSBV ) ? SB_SIZE : 0 );
    pPrivate->rclView.yTop    = rcl.yTop - pPrivate->fm.lExternalLeading - 2;
    pPrivate->ulLinesVisible  = RECTL_HEIGHT( pPrivate->rclView ) / pPrivate->ulUnitHeight;
    pPrivate->ulColsVisible   = RECTL_WIDTH( pPrivate->rclView ) / pPrivate->ulUnitWidth;

    if ( pPrivate->flStyle & MLS_WORDWRAP ) {
        // Reflow the text
        POINTL ptl;
        ptl.x = pPrivate->rclView.xLeft;
        ptl.y = pPrivate->rclView.yTop - pPrivate->ulUnitHeight;
        pPrivate->ulLinesTotal = ReflowEditorText( hps, pPrivate, ptl, 0 );
    }
    else {
        // Recalculate the display width of the longest line
        // TODO It's probably better to recalculate all the line lengths
        LONG lWidth;
        ULONG ulLines,
              cbStart,
              cbLength;
        ulLines  = LineBuffer_Count( &(pPrivate->breaks) );
        cbStart  = ( pPrivate->ulLongest )?
                       LineBuffer_ItemAt( &(pPrivate->breaks),
                                          pPrivate->ulLongest - 1 ): 0;
        cbLength = ( pPrivate->ulLongest < ulLines )?
                       LineBuffer_ItemAt( &(pPrivate->breaks),
                                          pPrivate->ulLongest ):
                       TextLength( pPrivate->text );
        lWidth = GetLineExtent( hps, pPrivate, cbStart, cbLength );

        pPrivate->ulColsTotal = pPrivate->ulUnitWidth ?
                                (ULONG) ( lWidth / pPrivate->ulUnitWidth ) : 0;
    }

    // Recalculate the scrollbar bounds
    if ( pPrivate->hwndSBV ) {
        if ( pPrivate->ulLinesTotal <= pPrivate->ulLinesVisible ) {
            pPrivate->ulOffsetY = 0;
            WinSendMsg( pPrivate->hwndSBV, SBM_SETTHUMBSIZE, MPFROM2SHORT( 1, 1 ), 0 );
            WinSendMsg( pPrivate->hwndSBV, SBM_SETSCROLLBAR, 0, MPFROM2SHORT( 0, 1 ));
        }
        else {
            sMax = max( 0, (LONG)( pPrivate->ulLinesTotal - pPrivate->ulLinesVisible ));
            WinSendMsg( pPrivate->hwndSBV, SBM_SETTHUMBSIZE,
                        MPFROM2SHORT( pPrivate->ulLinesVisible, pPrivate->ulLinesTotal ), 0 );
            WinSendMsg( pPrivate->hwndSBV, SBM_SETSCROLLBAR,
                        MPFROMSHORT( pPrivate->ulOffsetY ),
                        MPFROM2SHORT( 0, sMax ));
        }
    }
    if ( pPrivate->hwndSBH ) {
        if ( pPrivate->ulColsTotal <= pPrivate->ulColsVisible ) {
            pPrivate->ulOffsetX = 0;
            WinSendMsg( pPrivate->hwndSBH, SBM_SETTHUMBSIZE, MPFROM2SHORT( 1, 1 ), 0 );
            WinSendMsg( pPrivate->hwndSBH, SBM_SETSCROLLBAR, 0, MPFROM2SHORT( 0, 1 ));
        }
        else {
            sMax = max( 0, (LONG)( pPrivate->ulColsTotal - pPrivate->ulColsVisible ));
            WinSendMsg( pPrivate->hwndSBH, SBM_SETTHUMBSIZE,
                        MPFROM2SHORT( pPrivate->ulColsVisible, pPrivate->ulColsTotal ), 0 );
            WinSendMsg( pPrivate->hwndSBH, SBM_SETSCROLLBAR,
                        MPFROMSHORT( pPrivate->ulOffsetX ),
                        MPFROM2SHORT( 0, sMax ));
        }
    }

    // Exclude the border from the update region as it hasn't changed
    if ( pPrivate->flStyle & MLS_BORDER )
        WinInflateRect( hps, &rcl, -2, -2 );

    WinReleasePS( hps );
    WinInvalidateRect( hwnd, &rcl, FALSE );
}



