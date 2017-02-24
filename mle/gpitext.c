/*****************************************************************************
 * gpitext.c                                                                 *
 * GPI-based text rendering routines.                                        *
 *                                                                           *
 * In principle, all GPI-specific logic should be separated into this file   *
 * and gpiutil.c in order to faciliate its possible replacement in the       *
 * future (with something like Pango+FreeType).                              *
 *                                                                           *
 * To render 'Unicode' text under GPI, we have to support two different      *
 * rendering modes.  The first is 'true' Unicode mode, in which the GPI      *
 * codepage is 1200 and all text is rendered directly as UCS-2.              *
 * Unfortunately, GPI's Unicode rendering is broken under some particular    *
 * situations, notably when PM_AssociateFont is active and the current       *
 * text font is non-Unicode.  For such cases, we will have a fallback mode   *
 * known as 'codepage' mode.  In this case, the text is analyzed to see      *
 * what OS/2 codepage(s) will support the characters it contains.  It will   *
 * then be broken up into sequences that are rendered in different GPI       *
 * codepages as appropriate.  This mode is not yet implemented.              *
 *                                                                           *
 *****************************************************************************/

#define INCL_GPI
#define INCL_WIN
#include <os2.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unidef.h>
#include "byteparse.h"
#include "gpitext.h"

#include "debug.h"


// ----------------------------------------------------------------------------
// MACROS
//

/* The following macros are used to identify characters that require special
 * width-calculation logic in fixed-width fonts.  These are only used in
 * UCS-2 display mode, as we have to work around what appears to be a bug in
 * GPI's positioning calculations.  (In codepage mode GPI works correctly, so
 * this is unnecessary.)  See the FixedWidthIncrements() notes for details.
 */

// Characters which should be treated as double-width in a dedicated CJK font
#define IS_CJK_DOUBLEWIDTH( c ) (((( c >= 0x0400 ) && ( c <= 0x044F )) || \
                                   ( c == 0x00A8 ) || ( c == 0x00B4 )  || \
                                  (( c >= 0x2010 ) && ( c <= 0x203B )) || \
                                  (( c >= 0x2100 ) && ( c <= 0xA6FF )) || \
                                  (( c >= 0xAC00 ) && ( c <= 0xFF5F )))? 1: 0 )

// Characters which should be treated as double-width in a normal fixed-width font
#define IS_DOUBLEWIDTH( c )     (((( c >= 0x1100 ) && ( c <= 0x11FF )) || \
                                  (( c >= 0x2E80 ) && ( c <= 0xA4CF )) || \
                                  (( c >= 0xAC00 ) && ( c <= 0xD7AF )) || \
                                  (( c >= 0xF900 ) && ( c <= 0xFAFF )) || \
                                  (( c >= 0xFE30 ) && ( c <= 0xFE4F )) || \
                                  (( c >= 0xFF00 ) && ( c <= 0xFF5F )))? 1: 0 )

// Special characters which are classified as displayable but have zero width
#define IS_ZEROWIDTH( c )       (((( c >= 0x200B ) && ( c <= 0x200F )))? 1: 0 )

// Tab characters have their own special width
#define IS_TABCHAR( c )         (( c == 0x9 )? 1: 0 )


/* ------------------------------------------------------------------------- *
 * DrawTabbedUnicodeText                                                     *
 *                                                                           *
 * Draw the specified Unicode text inside the viewport from the specified    *
 * start position, allowing for included tab characters. On return, the      *
 * position will be set to the point after the last drawn character (where   *
 * the next character should be drawn if it exists).                         *
 *                                                                           *
 * In order to handle tab characters, this function internally splits the    *
 * text sequence up into tabless sub-sequences, and adds the necessary tab   *
 * offsets between each one.  This should be transparent to the caller.      *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HPS         hps      : Handle of the current presentation space    (I)  *
 *   PPOINTL     pptl     : Position of the next character to be drawn  (IO) *
 *   RECTL       rcl      : Text clipping rectangle/viewport area       (I)  *
 *   FONTMETRICS fm       : The current font metrics                    (I)  *
 *   ULONG       ulTabSize: The distance in pels between two tab stops  (I)  *
 *   UniChar    *puszText : Text buffer offset of the first character   (I)  *
 *   ULONG       ulChars  : Number of characters to draw                (I)  *
 *                                                                           *
 * RETURNS: LONG                                                             *
 *   The return code from GpiCharStringPosAt.                                *
 * ------------------------------------------------------------------------- */
LONG DrawTabbedUnicodeText( HPS         hps,
                            PPOINTL     pptl,
                            RECTL       rcl,
                            FONTMETRICS fm,
                            ULONG       ulTabSize,
                            UniChar     *puszText,
                            ULONG       ulChars    )
{
    LONG  alInc[ UCS_MAX_RENDER ],  // Character-position offsets for current section
          lRC;                      // GPI return code
    ULONG ulStart,                  // Starting offset of the current section in UniChars
          ulDraw,                   // Number of UniChars to draw in the current section
          ulTabPos,                 // The current tab stop position
          ulMinTabSize,             // The minimum allowable tab size
          i;

    ulStart  = 0;
    ulTabPos = 0;
    do {
        // Start off assuming we draw all remaining characters in the requested sequence
        ulDraw = ulChars - ulStart;
        // Now adjust this number to the position of the next tab
        for ( i = ulStart; i < ulChars; i++ ) {
            if ( IS_TABCHAR( puszText[ i ] )) {
                ulDraw = i - ulStart;
                break;
            }
        }
        if ( FixedWidthIncrements( puszText+ulStart, ulDraw, alInc, fm )) {
            ulMinTabSize = fm.lAveCharWidth;
            lRC = GpiCharStringPosAt( hps, pptl, &rcl, CHS_CLIP | CHS_VECTOR,
                                      UPOS_TO_BYTEOFF( ulDraw ),
                                      (PCHAR)(puszText + ulStart), alInc );
        }
        else {
            ulMinTabSize = max( 1, (USHORT)( fm.lEmInc / 5 ));
            lRC = GpiCharStringPosAt( hps, pptl, &rcl, CHS_CLIP,
                                      UPOS_TO_BYTEOFF( ulDraw ),
                                      (PCHAR)(puszText + ulStart), NULL );
        }
        if ( lRC != GPI_OK ) break;

        GpiQueryCurrentPosition( hps, pptl );

        // Now add the extra space required for the tab, if any
        if ( ulDraw < ( ulChars - ulStart )) {
            while ( ulTabPos <= ( pptl->x + ulMinTabSize ))
                ulTabPos += ulTabSize;
            DEBUG_PRINTF("[DrawTabbedUnicodeText]  -- adjusting %u to tab position: %u (tab size %u)\n", pptl->x, ulTabPos, ulTabSize );
            pptl->x = ulTabPos;
        }

        // Set the new starting point to the character after the tab
        ulStart += ( ulDraw + 1 );

        // Repeat until all characters are processed
    } while ( ulStart < ulChars );

    return lRC;
}


/* ------------------------------------------------------------------------- *
 * FitTextWidth                                                              *
 *                                                                           *
 * Determine how many of a string's bytes will, when rendered as characters, *
 * fall within the given on-screen horizontal area, assuming the string is   *
 * to be rendered starting from the given position (and using the currently- *
 * active font attributes).  The string is not assumed to be null terminated *
 * and may theoretically contain any byte values including nulls.            *
 *                                                                           *
 * The number of bytes will always be truncated to the first line-breaking   *
 * character found.                                                          *
 *                                                                           *
 * The pptl parameter describes the current position relative to the text    *
 * origin; a new line will always start with pptl->x = 0 (we always assume   *
 * left-to-right text direction for the purpose of fitting text).  Only the  *
 * x coordinate is significant to this routine.                              *
 *                                                                           *
 * lWidth represents the available width before the pptl->x parameter is     *
 * taken into account.  The effective width is thus lWidth - pptl->x.        *
 *                                                                           *
 * Due to the limitations of the GPI string routines, this function has to   *
 * split the string into 512-byte segments and query the length of each one, *
 * while incrementing the total fitted-byte count as it goes.  This process  *
 * should be transparent to the caller.                                      *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HPS         hps        : The current presentation space.           (I)  *
 *   PCHAR       pchText    : The string as a sequence of bytes.        (I)  *
 *   ULONG       cbText     : The length of pchText, in bytes.          (I)  *
 *   LONG        lWidth     : The total width, in pels.                 (I)  *
 *   PPOINTL     pptl       : The starting point for the string.        (I)  *
 *   ULONG       ulTabSize  : The number of pels between two tab stops. (I)  *
 *   FONTMETRICS fm         : The current font metrics.                 (I)  *
 *   ULONG       ulCodepage : The codepage in which pchText is encoded  (I)  *
 *   PBYTE       pDBCS      : The leading byte-ranges for the codepage  (I)  *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   The number of bytes that fit within the given width.                    *
 * ------------------------------------------------------------------------- */
ULONG FitTextWidth( HPS         hps,
                    PCHAR       pchText,
                    ULONG       cbText,
                    LONG        lWidth,
                    PPOINTL     pptl,
                    ULONG       ulTabSize,
                    FONTMETRICS fm,
                    ULONG       ulCodepage,
                    PBYTE       pDBCS       )
{
    PCHAR   pchStart;            // pointer to start of current segment
    ULONG   cbChars,             // total # of bytes that fit
            cbStart,             // starting offset of the current segment
            cbSegLen,            // # of bytes in the current segment
            cbSegFit,            // # of bytes that fit in the current segment
            cbTestFit;           // # of bytes to test the width of
    LONG    lTextWidth,          // queried width of a text sequence
            lTotal;              // actual total width of our fitted string
    BOOL    fRC;


    lWidth -= pptl->x;

    if ( !cbText || !pchText || ( lWidth < 1 ))
        return 0;

    fRC = GpiSetCp( hps, ulCodepage );
    if ( !fRC )
        DEBUG_PRINTF("[FitTextWidth] Failed to set codepage: 0x%X\n", WinGetLastError( WinQueryAnchorBlock( HWND_DESKTOP )));

    cbChars  = 0;
    cbStart  = 0;
    lTotal   = 0;
    while ( cbStart < cbText ) {
        pchStart = pchText + cbStart;
        cbSegLen = NextLineBreak( pchStart,
                                  min( cbText - cbStart, CB_MAX_RENDER ),
                                  ulCodepage );
        if ( !cbSegLen ) break;

        DEBUG_PRINTF("[FitTextWidth] Fitting %u bytes (from %u) to width %d: ", cbSegLen, cbStart, lWidth );
#ifdef DEBUG_LOG
        if ( ulCodepage == 1200 ) DumpUnicodeString( dbg, (UniChar *)(pchStart), BYTEOFF_TO_UPOS( cbSegLen ));
#endif

        if (( lTotal = QueryTextWidth( hps, pchStart, cbSegLen,
                                       fm, ulTabSize, ulCodepage )) > lWidth )
        {
            DEBUG_PRINTF("[FitTextWidth] Available width exceeded (%u), trying substrings\n", lTotal );

            if ( ulCodepage == 1200 ) {
                /* For fixed-length encodings, we can save time by scanning from
                 * an optimally-calculated point in the string.
                 */
                cbTestFit = lWidth / fm.lAveCharWidth;
                if ( cbSegLen < cbTestFit )
                    cbTestFit = cbSegLen - sizeof( UniChar );
                cbSegFit = cbTestFit;
                lTextWidth = QueryTextWidth( hps, pchStart, cbTestFit,
                                             fm, ulTabSize, ulCodepage );
                if ( lTextWidth > lWidth ) {
                    while (( cbTestFit > 1 ) &&
                           (( lTextWidth = QueryTextWidth( hps, pchStart, cbTestFit, fm,
                                                           ulTabSize, ulCodepage ))
                              > lWidth ))
                    {
                        cbSegFit = cbTestFit;
                        cbTestFit -= sizeof( UniChar );
                    }
                }
                else {
                    while (( cbTestFit < cbSegLen ) &&
                           (( lTextWidth = QueryTextWidth( hps, pchStart, cbTestFit, fm,
                                                           ulTabSize, ulCodepage ))
                              < lWidth ))
                    {
                        cbSegFit = cbTestFit;
                        cbTestFit += sizeof( UniChar );
                    }
                }
                lTotal = lTextWidth;
            }
            else {
                /* For variable-length encodings, scanning forwards from the
                 * start of the string is the only way to be sure of hitting
                 * all the character boundaries.
                 */
                cbTestFit = NextCharSize( pchStart, 0, cbSegLen, ulCodepage, pDBCS );
                cbSegFit = cbTestFit;
                while (( cbTestFit < cbSegLen ) &&
                       (( lTextWidth = QueryTextWidth( hps, pchStart, cbTestFit, fm,
                                                       ulTabSize, ulCodepage ))
                          < lWidth ))
                {
                    cbSegFit = cbTestFit;
                    cbTestFit += NextCharSize( pchStart, cbSegFit, cbSegLen,
                                               ulCodepage, pDBCS );
                    lTotal = lTextWidth;
                }
            }
        }
        else cbSegFit = cbSegLen;

        cbChars += cbSegFit;

        DEBUG_PRINTF("[FitTextWidth] - Fitted %u bytes, width %u pels: ", cbChars, lTotal );
#ifdef DEBUG_LOG
        if ( ulCodepage == 1200 ) DumpUnicodeString( dbg, (UniChar *)(pchStart), BYTEOFF_TO_UPOS( cbChars ));
#endif

        if ( cbSegFit < cbSegLen )
            break;              // stopped due to width or end of line reached
        cbStart = cbChars;
    }

    return cbChars;
}


/* ------------------------------------------------------------------------- *
 * FixedWidthIncrements                                                      *
 *                                                                           *
 * Determine whether a character-increments vector (CHS_VECTOR) is required  *
 * for drawing the specified string in the current (fixed-width) font; and,  *
 * if so, generates the appropriate vector.  This should only be used when   *
 * the string is being rendered in the UCS-2 codepage (1200).                *
 *                                                                           *
 * This is used as a workaround for a 'quirk' of GPI that fixed-width fonts  *
 * end up double-spaced when drawn or queried with Gpi(Query)CharString*.    *
 * We use the increments vector to override GPI's automatic placement with   *
 * corrected positions.                                                      *
 *                                                                           *
 * There are two types of fixed-width font we have to deal with: "pure"      *
 * fixed-width fonts where each character truly shares the same increment    *
 * (except for special zero-width characters, or glyphs substituted in using *
 * the PM DBCS gluph-association feature); and fixed-width-plus-CJK fonts    *
 * which report as monospaced but which render CJK characters (and a few     *
 * others) at double-width.                                                  *
 *                                                                           *
 * So we need to analyze each character in the string and categorize it:     *
 *  1. Normal character: the increment should be lAveCharWidth.              *
 *  2. Double-width CJK character: the increment should be lMaxCharInc if    *
 *     it is larger than lAveCharWidth, or 2*lAveCharWidth otherwise.        *
 *  3. Special zero-width character: the increment should be 0.              *
 *                                                                           *
 * Category 2 gives us some trouble because the affected characters may be   *
 * different depending on whether the font is a specialized CJK font or not. *
 * Such fonts can be identified by having an lMaxCharInc greater than the    *
 * lAveCharWidth in their font metrics; these fonts usually have many Greek, *
 * Cyrillic, mathemetical and symbol characters in double-width.  For        *
 * "regular" fixed-width fonts which either happen to include a limited set  *
 * of CJK fonts, or which have CJK glyphs substituted into them via          *
 * PM_AssociateFont, the extra-wide characters should be limited to the      *
 * actual CJK character ranges.                                              *
 *                                                                           *
 * Whew.  It might actually be simpler to just render fixed-width fonts in   *
 * codepage mode instead of UCS-2.                                           *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   Unichar *psuText: The string as a sequence of UniChars.                 *
 *   ULONG   ulLength: The length of psuText, in characters.                 *
 *   PLONG   alInc   : The vector of increments, must be >= 2*ulLength items.*
 *   FONTMETRICS fm  : The current font metrics.                             *
 *                                                                           *
 * RETURNS: BOOL                                                             *
 *   TRUE if an increments vector is required; FALSE if not                  *
 * ------------------------------------------------------------------------- */
BOOL FixedWidthIncrements( UniChar *psuText, ULONG ulLength, PLONG alInc, FONTMETRICS fm )
{
    UniChar uc;
    ULONG   ulTotal,
            i;
    BOOL    fHybridCJK;       // Is font a hybrid fixed-width/CJK font?

    // This routine is only necessary for fixed-width fonts
    if ( !( fm.fsType & FM_TYPE_FIXED )) return FALSE;

    fHybridCJK = ( fm.lMaxCharInc > fm.lAveCharWidth ) ? TRUE : FALSE;
    ulTotal = 0;
    for ( i = 0; i < UPOS_TO_BYTEOFF( ulLength ); i++ ) {
        // Various macros do most of the heavy lifting here...
        uc = psuText[ BYTEOFF_TO_UPOS( i ) ];
        if ( fHybridCJK && IS_CJK_DOUBLEWIDTH( uc ))
            alInc[ i ] = fm.lMaxCharInc;
        else if ( IS_DOUBLEWIDTH( uc ))
            alInc[ i ] = 2 * fm.lAveCharWidth;
        else if ( IS_ZEROWIDTH( uc ))
            alInc[ i ] = 0;
        else alInc[ i ] = fm.lAveCharWidth;
        alInc[ i+1 ] = alInc[ i ];
        ulTotal += alInc[ i ];
    }

//    DEBUG_PRINTF("[FixedWidthIncrements] %s detected.\n", fHybridCJK? "Mixed CJK font": "Normal fixed-width font");
//    DEBUG_PRINTF("[FixedWidthIncrements] Calculated increments for %u characters (%u bytes): total width %u.\n", i, ulLength, ulTotal );

    return TRUE;
}


/* ------------------------------------------------------------------------- *
 * QueryTextWidth                                                            *
 *                                                                           *
 * Determine the number of horizontal pels that a string will occupy, when   *
 * rendered as characters using the currently-active font attributes.  The   *
 * string may be either codepage or UCS-2 encoded; in either case it is      *
 * length-delimited in bytes.  The string should not contain line-breaking   *
 * characters; it is up to the caller to ensure this.                        *
 *                                                                           *
 * Due to tab character handling, the calculated width will be dependent on  *
 * the starting position.  The current GPI drawing position is assumed.      *
 *                                                                           *
 * Due to the limitations of GpiQueryCharStringPosAt, this function has to   *
 * split the string into 512-byte segments and query the length of each one, *
 * incrementing the total fitted-byte count as it goes.  This process should *
 * be transparent to the caller.                                             *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HPS         hps      : The current presentation space.              (I) *
 *   PCHAR       pchText  : The string as a sequence of bytes.           (I) *
 *   ULONG       cbText   : Total length of the string in bytes.         (I) *
 *   FONTMETRICS fm       : The current font metrics.                    (I) *
 *   ULONG       ulTabSize: The number of pels between tab stops.        (I) *
 *   ULONG       usCP     : The codepage in which pchText is encoded.    (I) *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   The horizontal space required by the string (in pels).                  *
 * ------------------------------------------------------------------------- */
ULONG QueryTextWidth( HPS hps, PCHAR pchText, ULONG cbText, FONTMETRICS fm, ULONG ulTabSize, ULONG usCP )
{
    POINTL  ptlInit, ptl;        // drawing positions (initial and current)
    LONG    alInc[ CB_MAX_RENDER ] = {0},  // array of character increments per segment
            lExtent;             // extent (width) of current text segment
    ULONG   ulWidth,             // total calculated width
            cbStart,             // starting offset of the current segment
            cbCurrent,           // # of bytes in the current segment
            i;


    if ( !pchText || !cbText ) return 0;

    cbStart = 0;
    ulWidth = 0;
    GpiQueryCurrentPosition( hps, &ptlInit );
    ptl = ptlInit;
    while ( cbStart < cbText ) {
        cbCurrent = min( cbText - cbStart, CB_MAX_RENDER );

        // Determine the position of each character in the string
#if 0
        if (( usCP == 1200 ) &&
            ( FixedWidthIncrements( (UniChar *)( pchText + cbStart ),
                                    BYTEOFF_TO_UPOS( cbCurrent ), alInc, fm )))
        {
            // doesn't handle tabs
            for ( i = 0; i < cbCurrent; i += 2 ) ulWidth += alInc[ i ];
        }
#else
        if ( usCP == 1200 ) {
            lExtent = QueryUnicodeTextWidth( hps, (UniChar *)(pchText + cbStart),
                                             BYTEOFF_TO_UPOS( cbCurrent ), fm, ulTabSize );
            ulWidth += lExtent;
            ptl.x += lExtent;
        }
#endif
        else {
            lExtent = GpiQueryTabbedTextExtent( hps, cbCurrent, pchText + cbStart, 1, &ulTabSize );
            ulWidth += lExtent;
            ptl.x += lExtent;
        }
        GpiMove( hps, &ptl );
        cbStart += CB_MAX_RENDER;
    }
    GpiMove( hps, &ptlInit );        // restore the original position
    return ( ulWidth );
}


/* ------------------------------------------------------------------------- *
 * QueryUnicodeTextWidth                                                     *
 *                                                                           *
 * Determine the number of horizontal pels that a Unicode string will occupy *
 * when rendered as characters using the currently-active font attributes.   *
 * The string must be UCS-2 encoded, and length-delimited in UniChars.  The  *
 * string may contain tabs, but should not contain line-breaking characters. *
 *                                                                           *
 * In order to handle tab characters, this function internally splits the    *
 * text sequence up into tabless sub-sequences, and adds the necessary tab   *
 * offsets between each one.  This should be transparent to the caller.      *
 *                                                                           *
 * This function basically replaces GpiQueryTabbedTextExtent() for UCS-2     *
 * strings, as the latter function does not seem to be reliable under the    *
 * Unicode codepage.                                                         *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   HPS         hps      : The current presentation space.              (I) *
 *   UniChar    *puszText : The string as a sequence of UniChars.        (I) *
 *   ULONG       ulChars  : Total length of the string in UniChars.      (I) *
 *   FONTMETRICS fm       : The current font metrics.                    (I) *
 *   ULONG       ulTabSize: The number of pels between tab stops.        (I) *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   The horizontal space required by the string (in pels).                  *
 * ------------------------------------------------------------------------- */
ULONG QueryUnicodeTextWidth( HPS hps, UniChar *puszText, ULONG ulChars, FONTMETRICS fm, ULONG ulTabSize )
{
    LONG   alInc[ UCS_MAX_RENDER ];  // Character increments vector for current section
    POINTL ptl,                      // Current position
           aptl[ TXTBOX_COUNT ];     // Coordinates of text bounding box
    ULONG  ulStart,                  // Starting offset of the current section in UniChars
           ulSect,                   // Number of UniChars to draw in the current section
           ulWidth,                  // The total text width in pels
           ulTabPos,                 // The current tab stop position
           ulMinTabSize,             // The minimum allowable space a tab can occupy
           i;

    ulWidth  = 0;
    ulStart  = 0;
    ulTabPos = 0;

    DEBUG_PRINTF("[QueryUnicodeTextWidth] Entered QueryUnicodeTextWidth() for %u character string: ", ulChars );
#ifdef DEBUG_LOG
    DumpUnicodeString( dbg, puszText, ulChars );
#endif

    GpiQueryCurrentPosition( hps, &ptl );
    do {
        // Start with all remaining characters in the requested sequence
        ulSect = ulChars - ulStart;
        // Now adjust this number to the position of the next tab
        for ( i = ulStart; i < ulChars; i++ ) {
            if ( IS_TABCHAR( puszText[ i ] )) {
                ulSect = i - ulStart;
                //DEBUG_PRINTF("[QueryUnicodeTextWidth]  Tab found, segment length is %u\n", ulSect );
                break;
            }
        }
        // For fixed-width fonts, use the increments vector
        if ( FixedWidthIncrements( puszText + ulStart, ulSect, alInc, fm )) {
            ulMinTabSize = fm.lAveCharWidth;
            for ( i = 0; i < TXTBOX_COUNT; i++ ) aptl[ i ] = ptl;
            aptl[ TXTBOX_TOPLEFT ].y += fm.lMaxAscender;
            aptl[ TXTBOX_TOPRIGHT ].y = aptl[ TXTBOX_TOPLEFT ].y;
            for ( i = 0; i < UPOS_TO_BYTEOFF( ulSect ); i += 2 )
                aptl[ TXTBOX_BOTTOMRIGHT ].x += alInc[ i ];
            aptl[ TXTBOX_TOPRIGHT ].x = aptl[ TXTBOX_BOTTOMRIGHT ].x;
        }
        // Otherwise just query GPI directly
        else {
            ulMinTabSize = max( 1, (USHORT)( fm.lEmInc / 5 ));
            GpiQueryTextBox( hps, UPOS_TO_BYTEOFF( ulSect ),
                             (PCHAR)(puszText + ulStart), TXTBOX_COUNT, aptl );
        }

        // Add the extra space required for the tab, if any
        if ( ulSect < ( ulChars - ulStart )) {
            while ( ulTabPos <= ( aptl[ TXTBOX_BOTTOMRIGHT ].x + ulMinTabSize ))
                ulTabPos += ulTabSize;
            DEBUG_PRINTF("[QueryUnicodeTextWidth]  -- tab detected, adjusting (%u - %u) to tab position: %u (tab size %u)\n", aptl[ TXTBOX_BOTTOMLEFT ].x, aptl[ TXTBOX_BOTTOMRIGHT ].x, ulTabPos, ulTabSize );
            aptl[ TXTBOX_BOTTOMRIGHT ].x = ulTabPos;
        }

        ulWidth += ( aptl[ TXTBOX_BOTTOMRIGHT ].x - aptl[ TXTBOX_BOTTOMLEFT ].x );
        ptl.x += ( aptl[ TXTBOX_BOTTOMRIGHT ].x - aptl[ TXTBOX_BOTTOMLEFT ].x );

        DEBUG_PRINTF("[QueryUnicodeTextWidth]  -- segment bounds: (%u - %u), total width: %u\n", aptl[ TXTBOX_BOTTOMLEFT ].x, aptl[ TXTBOX_BOTTOMRIGHT ].x, ulWidth );

        // Set the new starting point to the character after the tab
        ulStart += ( ulSect + 1 );
        //DEBUG_PRINTF("[QueryUnicodeTextWidth]  Will start next segment at %u\n", ulStart );

        // Repeat until all characters are processed
    } while ( ulStart < ulChars );

    DEBUG_PRINTF("[QueryUnicodeTextWidth] Returning total string width %u\n", ulWidth );

    return ( ulWidth );
}


