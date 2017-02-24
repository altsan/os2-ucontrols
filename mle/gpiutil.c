#define INCL_GPI
#define INCL_WIN
#include <os2.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unidef.h>
#include "gpiutil.h"

#include "debug.h"


/* ------------------------------------------------------------------------- *
 * DrawNice3DBorder                                                          *
 *                                                                           *
 * Draw an attractive MLE-style 3D border around a control.                  *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HPS hps  : Handle of the control's presentation space.                  *
 *   RECTL rcl: Rectangle defining the boundaries of the control.            *
 *                                                                           *
 * RETURNS: N/A                                                              *
 * ------------------------------------------------------------------------- */
void DrawNice3DBorder( HPS hps, RECTL rcl )
{
    POINTL ptl;

    GpiSetColor( hps, SYSCLR_BUTTONLIGHT );
    ptl.x = rcl.xLeft;
    ptl.y = rcl.yBottom;
    GpiMove( hps, &ptl );
    ptl.x = rcl.xRight - 1;
    GpiLine( hps, &ptl );
    ptl.y = rcl.yTop - 1;
    GpiLine( hps, &ptl );
    GpiSetColor( hps, SYSCLR_DIALOGBACKGROUND );
    ptl.x = rcl.xLeft;
    ptl.y = rcl.yBottom + 1;
    GpiMove( hps, &ptl );
    ptl.x = rcl.xRight - 2;
    GpiLine( hps, &ptl );
    ptl.y = rcl.yTop;
    GpiLine( hps, &ptl );
    GpiSetColor( hps, SYSCLR_BUTTONDARK );
    ptl.x = rcl.xLeft;
    ptl.y = rcl.yBottom + 1;
    GpiMove( hps, &ptl );
    ptl.y = rcl.yTop - 1;
    GpiLine( hps, &ptl );
    ptl.x = rcl.xRight - 1;
    GpiLine( hps, &ptl );
    GpiSetColor( hps, CLR_BLACK );
    ptl.x = rcl.xLeft + 1;
    ptl.y = rcl.yBottom + 2;
    GpiMove( hps, &ptl );
    ptl.y = rcl.yTop - 2;
    GpiLine( hps, &ptl );
    ptl.x = rcl.xRight - 3;
    GpiLine( hps, &ptl );
}


/* ------------------------------------------------------------------------- *
 * GetCurrentDPI                                                             *
 *                                                                           *
 * Queries the current vertical font resolution (a.k.a. DPI).                *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HWND hwnd: Window handle of control.                                    *
 *                                                                           *
 * RETURNS: LONG                                                             *
 *   Current screen DPI setting.                                             *
 * ------------------------------------------------------------------------- */
LONG GetCurrentDPI( HWND hwnd )
{
    HDC  hdc;           // device-context handle
    LONG lCap,          // value from DevQueryCaps
         lDPI;          // returned DPI value

    hdc = WinOpenWindowDC( hwnd );
    if ( DevQueryCaps( hdc, CAPS_VERTICAL_FONT_RES, 1, &lCap ))
        lDPI = lCap;
    if ( !lDPI )
        lDPI = 96;

    return lDPI;
}


/* ------------------------------------------------------------------------- *
 * GetImageFont                                                              *
 *                                                                           *
 * Look for a bitmap (a.k.a. image or raster) font under the specified font  *
 * name.  This is necessary because PM sees every distinct point size of a   *
 * bitmap font as a separate font (albeit with a shared family name).        *
 *                                                                           *
 * This function specifically looks for bitmap rather than outline (vector)  *
 * fonts; if no bitmap versions of the requested font name are found, we     *
 * return FALSE.  (The caller will presumably try to treat pszFontFace as an *
 * outline font in such a case; whether it wants to verify its existence or  *
 * not before doing so is its own problem.)                                  *
 *                                                                           *
 * If an exact match is found, the font attributes structure will be         *
 * populated with the appropriate font information, and the function will    *
 * return TRUE.  Otherwise, FALSE will be returned (and the contents of      *
 * pfAttrs will not be altered).                                             *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HPS     hps    : Handle of the current presentation space.         (I)  *
 *   PSZ     pszFont: Face name of the font being requested.            (I)  *
 *   LONG    lPts   : The requested point size (multiple of 10).        (I)  *
 *   LONG    lDPI   : The requested device resolution (DPI).            (I)  *
 *   PFATTRS pfAttrs: Pointer to font attributes structure.             (IO) *
 *                                                                           *
 * RETURNS: BOOL                                                             *
 *   TRUE if a suitable bitmap font was found; FALSE otherwise.              *
 * ------------------------------------------------------------------------- */
BOOL GetImageFont( HPS hps, PSZ pszFont, LONG lPts, LONG lDPI, PFATTRS pfAttrs )
{
    PFONTMETRICS pfm;               // array of FONTMETRICS objects
    LONG         i,                 // loop index
                 cFonts    = 0,     // number of fonts found
                 cCount    = 0;     // number of fonts to return
    BOOL         fFound    = FALSE; // did we find a suitable bitmap font?


    // Find the specific fonts which match the given face name
    cFonts = GpiQueryFonts( hps, QF_PUBLIC, pszFont, &cCount, 0, NULL );
    if ( cFonts < 1 ) return FALSE;
    if ( DosAllocMem( (PPVOID) &pfm, ( cFonts * sizeof( FONTMETRICS )),
                        PAG_COMMIT | PAG_READ | PAG_WRITE ) != NO_ERROR )
        return FALSE;
    GpiQueryFonts( hps, QF_PUBLIC, pszFont,
                   &cFonts, sizeof( FONTMETRICS ), pfm );

    // Look for a bitmap font that matches the requested size and resolution
    for ( i = 0; ( i < cFonts) && !fFound; i++ ) {
        if ( pfm[i].fsDefn & FM_DEFN_OUTLINE ) continue;

        if (( pfm[i].sNominalPointSize == lPts ) &&
            ( pfm[i].sYDeviceRes == lDPI ))
        {
            strcpy( pfAttrs->szFacename, pfm[i].szFacename );
            pfAttrs->lMatch          = pfm[i].lMatch;
            pfAttrs->idRegistry      = pfm[i].idRegistry;
            pfAttrs->lMaxBaselineExt = pfm[i].lMaxBaselineExt;
            pfAttrs->lAveCharWidth   = pfm[i].lAveCharWidth;
            pfAttrs->fsType          = pfm[i].fsType;
            fFound = TRUE;
            break;
        }
    }

    DosFreeMem( pfm );
    return ( fFound );
}


/* ------------------------------------------------------------------------- *
 * SetFontFromPP                                                             *
 *                                                                           *
 * Sets the active GPI font from the current font presentation parameter.    *
 * It will always use the logical font ID 1.                                 *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HPS      hps : Current presentation space handle.                  (I)  *
 *   HWND     hwnd: Window handle of control.                           (I)  *
 *   LONG     lDPI: Current screen DPI setting.                         (I)  *
 *   PFATTRS  pfa : Pointer to returned font attributes.                (O)  *
 *                                                                           *
 * RETURNS: BOOL                                                             *
 *   TRUE on success, FALSE on error.                                        *
 * ------------------------------------------------------------------------- */
BOOL SetFontFromPP( HPS hps, HWND hwnd, LONG lDPI, PFATTRS pfa )
{
    FATTRS  fa = {0};                     // required font attributes
    SIZEF   sfCell;                       // character cell size
    CHAR    szFont[ FACESIZE+5 ] = {0};   // current font pres.param
    PSZ     pszFontName;                  // requested font family
    ULONG   ulPts;                        // requested font size
    APIRET  rc;

    // Get the font name from the presentation parameter
    rc = WinQueryPresParam( hwnd, PP_FONTNAMESIZE,
                            0L, NULL, sizeof( szFont ), szFont, 0L );
    if ( !rc )
        return FALSE;

    // Define the requested font attributes
    fa.usRecordLength = sizeof( FATTRS );
    fa.fsType         = FATTR_TYPE_MBCS;
    fa.fsFontUse      = FATTR_FONTUSE_NOMIX;
    pszFontName = strchr( szFont, '.') + 1;
    strcpy( fa.szFacename, pszFontName );
    if ( sscanf( szFont, "%d.", &ulPts ) != 1 )
        ulPts = 10;

    // If this is a bitmap font, look up the proper name+size
    if ( ! GetImageFont( hps, pszFontName, (LONG)(ulPts*10), lDPI, &fa ))
    {
        /* No bitmap font found.  Assume it's an outline font and scale it to
         * the requested point size according to the current DPI setting.
         */
        double dScale = ( (double) lDPI / 72 );
        ulPts = ulPts * dScale;
        sfCell.cy = MAKEFIXED( ulPts, 0 );
        sfCell.cx = sfCell.cy;
        GpiSetCharBox( hps, &sfCell );
    }

    // Make the font active
    if ( GpiCreateLogFont( hps, NULL, 1L, &fa ) == GPI_ERROR )
        return FALSE;
    if ( GpiSetCharSet( hps, 1L ) == GPI_ERROR )
        return FALSE;

    if ( pfa ) memcpy( pfa, &fa, fa.usRecordLength );
    return TRUE;
}


/* ------------------------------------------------------------------------- *
 * SetFontFromAttrs                                                          *
 *                                                                           *
 * Sets the active GPI font from the specified font attributes structure.    *
 * It will always use the logical font ID 1.                                 *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HPS         hps : Current presentation space handle.               (I)  *
 *   LONG        lDPI: Current screen DPI setting.                      (I)  *
 *   PFATTRS     pfa : Pointer to font attributes.                      (I)  *
 *   FONTMETRICS fm  : Font metrics.                                    (I)  *
 *                                                                           *
 * RETURNS: BOOL                                                             *
 *   TRUE on success, FALSE on error.                                        *
 * ------------------------------------------------------------------------- */
BOOL SetFontFromAttrs( HPS hps, LONG lDPI, PFATTRS pfa, FONTMETRICS fm )
{
    SIZEF   sfCell;                       // character cell size
    ULONG   ulPts;                        // requested font size
    APIRET  rc;

    /* If this is an outline font, set the requested point size according to
     * the current DPI setting.
     */
    if ( fm.fsDefn & FM_DEFN_OUTLINE ) {
        double dScale = ( (double) lDPI / 72 );
        if ( sscanf( pfa->szFacename, "%d.", &ulPts ) != 1 ) ulPts = 10;
        ulPts = ulPts * dScale;
        sfCell.cy = MAKEFIXED( ulPts, 0 );
        sfCell.cx = sfCell.cy;
        GpiSetCharBox( hps, &sfCell );
    }

    // Make the font active
    if ( GpiCreateLogFont( hps, NULL, 1L, pfa ) == GPI_ERROR )
        return FALSE;
    if ( GpiSetCharSet( hps, 1L ) == GPI_ERROR )
        return FALSE;

    return TRUE;
}



