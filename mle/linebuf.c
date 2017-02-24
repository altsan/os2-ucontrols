#define INCL_DOSERRORS
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "linebuf.h"


// ---------------------------------------------------------------------------
// CONSTANTS
//

#define LB_INITIAL_SIZE         256     // initial size of the line-offset buffer
#define LB_DEFAULT_INC          128     // default increment size


// ---------------------------------------------------------------------------
// MACROS
//

// Return the actual buffer address of the requested logical index
#define INDEX2ACTUAL( buf, pos ) \
  (( (pos) < (buf).ulSp1Len ) ? (pos) : ((buf).ulGapLen + (pos) ))

// Return the total number of items
#define ITEMCOUNT( buf )     ( (buf).ulSp1Len + (buf).ulSp2Len )

// Return the buffer position following the last character
#define ITEMEND( buf ) \
  ( (buf).ulSp1Len + (buf).ulGapLen + (buf).ulSp2Len )




/* ------------------------------------------------------------------------- *
 * LineBuffer_Clear()                                                        *
 *                                                                           *
 * Zeroizes the current buffer contents, starting from the indicated offset. *
 * The offset becomes the new gap starting position, although the gap size   *
 * is left unchanged.                                                        *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   PLBOBUFFER pBuf      : Pointer to buffer object                         *
 *   ULONG      ulPosition: The starting position from which to clear        *
 *                                                                           *
 * RETURNS: BOOL                                                             *
 * ------------------------------------------------------------------------- */
BOOL LineBuffer_Clear( PLBOBUFFER pBuf, ULONG ulPosition )
{
    if ( ulPosition != pBuf->ulSp1Len )
        if ( ! LineBuffer_MoveGap( pBuf, ulPosition )) return FALSE;

    memset( pBuf->pulItems + ulPosition, 0,
            pBuf->ulSize - ( ulPosition * sizeof( ULONG )));
    pBuf->ulSp2Len = 0;
    return TRUE;
}


/* ------------------------------------------------------------------------- *
 * LineBuffer_Count()                                                        *
 *                                                                           *
 * Returns the number of items currently in the buffer.                      *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   PLBOBUFFER pBuf      : Pointer to buffer object                         *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 * ------------------------------------------------------------------------- */
ULONG LineBuffer_Count( PLBOBUFFER pBuf )
{
    return ( ITEMCOUNT( *pBuf ));
}


/* ------------------------------------------------------------------------- *
 * LineBuffer_Expand()                                                       *
 *                                                                           *
 * Increases the size of the line index offset buffer, to the requested size *
 * if one is specified, or else by a default increment.  The current buffer  *
 * contents are preserved.                                                   *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   PLBOBUFFER pBuf       : Pointer to buffer object                        *
 *   ULONG      ulRequested: Requested new buffer size (in ULONG items)      *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   The new buffer size (in ULONGs).                                        *
 * ------------------------------------------------------------------------- */
ULONG LineBuffer_Expand( PLBOBUFFER pBuf, ULONG ulRequested )
{
    ULONG  ulCurrentSize,
           ulNewSize;
    PULONG pulNew;
    APIRET rc;

    ulCurrentSize = pBuf->ulSize;
    if ( ulRequested && ( ulRequested <= ulCurrentSize ))
        return ( ulCurrentSize );

    if (( !pBuf->pulItems ) || ( !ulCurrentSize ))
        if ( ! LineBuffer_Init( pBuf, 0 )) return 0;

    ulNewSize = ulRequested ? ulRequested : ( ulCurrentSize + LB_DEFAULT_INC );
    rc = DosAllocMem( (PPVOID) &pulNew, ulNewSize * sizeof( ULONG ),
                      PAG_READ | PAG_WRITE | PAG_COMMIT );
    if ( rc == NO_ERROR ) {
        memcpy( pulNew, pBuf->pulItems, pBuf->ulSize * sizeof( ULONG ));
        DosFreeMem( pBuf->pulItems );
        pBuf->pulItems = pulNew;
    }
//    pBuf->pulItems = (PULONG) realloc( pBuf->pulItems, ulNewSize * sizeof( ULONG ));
//    if ( pBuf->pulItems == NULL ) return 0;

    pBuf->ulSize = ulNewSize;

//printf("Expanded buffer to %u bytes\n", ulNewSize );

    return ( ulNewSize );
}


/* ------------------------------------------------------------------------- *
 * LineBuffer_Free()                                                         *
 *                                                                           *
 * Frees the free buffer memory and zeroes the rest of the data structure.   *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   PLBOBUFFER pBuf      : Pointer to buffer object                         *
 *                                                                           *
 * RETURNS: N/A                                                              *
 * ------------------------------------------------------------------------- */
void LineBuffer_Free( PLBOBUFFER pBuf )
{
    if ( pBuf->ulSize ) DosFreeMem( pBuf->pulItems );
    memset( pBuf, 0, sizeof( LBOBUFFER ));
}


/* ------------------------------------------------------------------------- *
 * LineBuffer_FindPosition                                                   *
 *                                                                           *
 * Uses a binary search algorithm to locate the proper (sorted) position in  *
 * the line buffer for the specified value.                                  *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   PLBOBUFFER pBuf    : Pointer to buffer object                           *
 *   ULONG      ulStart : First position of current search range             *
 *   ULONG      ulEnd   : Last position of current search range              *
 *   ULONG      ulValue : Value whose position is sought                     *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   The sorted position where the indicated value should be placed.         *
 * ------------------------------------------------------------------------- */
ULONG LineBuffer_FindPosition( PLBOBUFFER pBuf, ULONG ulStart, ULONG ulEnd, ULONG ulValue )
{
    ULONG ulCurrent,
          ulMid;

    if ( ulStart > ulEnd )
        return ulStart;

    ulMid = ulStart + (( ulEnd - ulStart ) / 2 );
    ulCurrent = LineBuffer_ItemAt( pBuf, ulMid );
    if ( ulCurrent > ulValue )
        return ( !ulMid ? ulMid :
                 LineBuffer_FindPosition( pBuf, ulStart, ulMid-1, ulValue ));
    else if ( ulCurrent < ulValue )
        return LineBuffer_FindPosition( pBuf, ulMid+1, ulEnd, ulValue );
    else
        return ulMid;
}


/* ------------------------------------------------------------------------- *
 * LineBuffer_Init()                                                         *
 *                                                                           *
 * Allocates the line-break offset buffer.                                   *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   PLBOBUFFER pBuf     : Pointer to buffer object                          *
 *   ULONG      ulInitial: Requested initial buffer size (in ULONGs)         *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   The size (in ULONGs) of the newly-created buffer.                       *
 * ------------------------------------------------------------------------- */
ULONG LineBuffer_Init( PLBOBUFFER pBuf, ULONG ulInitial )
{
    APIRET rc;
    if ( !ulInitial ) ulInitial = LB_INITIAL_SIZE;
    rc = DosAllocMem( (PPVOID) &(pBuf->pulItems), ulInitial * sizeof( ULONG ),
                      PAG_READ | PAG_WRITE | PAG_COMMIT );
    if ( rc != NO_ERROR) return 0;
    //pBuf->pulItems = (PULONG) calloc( ulInitial, sizeof( ULONG ));
    //if ( pBuf->pulItems == NULL ) return 0;
    pBuf->ulSize   = ulInitial;
    pBuf->ulSp1Len = 0;
    pBuf->ulSp2Len = 0;
    pBuf->ulGapLen = ulInitial / 2;

    return ulInitial;
}


/* ------------------------------------------------------------------------- *
 * LineBuffer_Insert()                                                       *
 *                                                                           *
 * Insert a new line address into the line buffer at the proper place.  Note *
 * that an item may be either inserted into the middle of the buffer, or     *
 * placed IMMEDIATELY after the last item.                                   *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   PLBOBUFFER pBuf      : Pointer to buffer object                         *
 *   ULONG      cbValue   : The new value to add                             *
 *   ULONG      ulPosition: The new value's logical position in the buffer   *
 *                                                                           *
 * RETURNS: BOOL                                                             *
 * ------------------------------------------------------------------------- */
BOOL LineBuffer_Insert( PLBOBUFFER pBuf, ULONG cbValue, ULONG ulPosition )
{
    if ( !pBuf->pulItems || !pBuf->ulSize )
        if ( ! LineBuffer_Init( pBuf, 0 )) return FALSE;
    if ( ulPosition > ITEMCOUNT( *pBuf ))  return FALSE;
    else if (( INDEX2ACTUAL( *pBuf, ulPosition ) == ITEMEND( *pBuf )) &&
             ( ITEMEND( *pBuf ) < pBuf->ulSize ) && pBuf->ulSp2Len )
    {
        /* A cheap but useful optimization: if the insert position is at the
         * end of the array, and there's still room in the buffer, then don't
         * bother to move the gap there - just append the new value to the
         * post-gap span.
         */
        pBuf->pulItems[ ITEMEND( *pBuf ) ] = cbValue;
        pBuf->ulSp2Len++;
    }
    else {
        if ( ulPosition != pBuf->ulSp1Len )
            if ( ! LineBuffer_MoveGap( pBuf, ulPosition )) return FALSE;
        else if ( INDEX2ACTUAL( *pBuf, ulPosition ) >= pBuf->ulSize )
            if ( ! LineBuffer_Expand( pBuf, 0 )) return FALSE;
        if ( pBuf->ulGapLen < 2 )
            if ( ! LineBuffer_OpenGap( pBuf, 0 )) return FALSE;

        pBuf->pulItems[ ulPosition ] = cbValue;
        pBuf->ulSp1Len++;
        pBuf->ulGapLen--;
    }
    return TRUE;
}


/* ------------------------------------------------------------------------- *
 * LineBuffer_ItemAt()                                                       *
 *                                                                           *
 * Returns the item at the given logical position.  If the position is       *
 * invalid, LB_INVALID_POSITION is returned.                                 *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   PLBOBUFFER pBuf      : Pointer to buffer object                         *
 *   ULONG      ulPosition: The logical position to be queried               *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 * ------------------------------------------------------------------------- */
ULONG LineBuffer_ItemAt( PLBOBUFFER pBuf, ULONG ulPosition )
{
    ULONG ulAddr;
    if ( ulPosition >= ITEMCOUNT( *pBuf )) return LB_INVALID_POSITION;
    ulAddr = INDEX2ACTUAL( *pBuf, ulPosition );
    return ( pBuf->pulItems[ ulAddr ] );
}


/* ------------------------------------------------------------------------- *
 * LineBuffer_MoveGap()                                                      *
 *                                                                           *
 * Moves the current position of the gap within the line buffer.             *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   PLBOBUFFER pBuf      : Pointer to buffer object                         *
 *   ULONG     ulPosition: Requested new buffer size                         *
 *                                                                           *
 * RETURNS: BOOL                                                             *
 *   TRUE on success, FALSE on error                                         *
 * ------------------------------------------------------------------------- */
BOOL LineBuffer_MoveGap( PLBOBUFFER pBuf, ULONG ulPosition )
{
    ULONG ulGapStart,   // New starting position of the gap
          ulShift,      // Number of items to shift
          ulOldSp1Len,  // Old length of the pre-gap section
          ulSource,     // Starting source index of items to move
          ulTarget;     // Starting destination index of items to move
    LONG i;


    if ( ulPosition > pBuf->ulSize ) return FALSE;

    ulGapStart = ulPosition;
    ulOldSp1Len = pBuf->ulSp1Len;
//printf("\nMoving gap position to %u\n", ulPosition );

    // shift gap to the right
    if ( ulGapStart > ulOldSp1Len ) {
        ulShift  = ulGapStart - ulOldSp1Len;
        ulSource = ulOldSp1Len + pBuf->ulGapLen;   // (old) start of post-gap text
        ulTarget = ulOldSp1Len;                    // (old) start of gap
        for ( i = 0; i < ulShift; i++ ) {
            pBuf->pulItems[ ulTarget+i ] = pBuf->pulItems[ ulSource+i ];
            pBuf->pulItems[ ulSource+i ] = 0;
        }
        pBuf->ulSp1Len = ulGapStart;
        pBuf->ulSp2Len -= ulShift;
    }
    // shift gap to the left
    else if ( ulGapStart < ulOldSp1Len ) {
        ulShift  = ulOldSp1Len - ulGapStart;
        ulSource = ulGapStart;                     // (new) start of gap
        ulTarget = ulGapStart + pBuf->ulGapLen;    // (new) start of post-gap text
        for ( i = ulShift-1; i >= 0; i-- ) {
            pBuf->pulItems[ ulTarget+i ] = pBuf->pulItems[ ulSource+i ];
            pBuf->pulItems[ ulSource+i ] = 0;
        }
        pBuf->ulSp1Len = ulGapStart;
        pBuf->ulSp2Len += ulShift;
    }

    return TRUE;
}


/* ------------------------------------------------------------------------- *
 * LineBuffer_OpenGap()                                                      *
 *                                                                           *
 * Opens up the gap by shifting the post-gap items back until the gap is     *
 * either 50% of the total buffer, or the minimum required size plus a       *
 * standard increment.  To be called whenever the gap is about to fill up.   *
 * If there is insufficient space in the buffer for this, the buffer itself  *
 * is expanded (and the gap size retargeted to 50% of the new buffer size).  *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   PLBOBUFFER pBuf      : Pointer to buffer object                         *
 *   ULONG      ulRequired: Minimum required gap size (in ULONGs)            *
 *                                                                           *
 * RETURNS: BOOL                                                             *
 *   TRUE on success, FALSE on error                                         *
 * ------------------------------------------------------------------------- */
BOOL LineBuffer_OpenGap( PLBOBUFFER pBuf, ULONG ulRequired )
{
    ULONG ulNewGap,     // New gap length
          ulSource,     // Starting source index of bytes to move
          ulTarget;     // Starting destination index of bytes to move
    LONG  i;

    ulNewGap = pBuf->ulSize / 2;
    while ( ulNewGap < ( ulRequired + LB_DEFAULT_INC )) ulNewGap *= 2;
    if ( pBuf->ulGapLen >= ulNewGap ) return TRUE;    // nothing to do

    if (( ulNewGap + ITEMCOUNT( *pBuf )) > pBuf->ulSize ) {
        if ( ! LineBuffer_Expand( pBuf, ulRequired )) return FALSE;
        ulNewGap = pBuf->ulSize / 2;
    }
//printf("Expanding gap to %u bytes\n", ulNewGap );

    ulSource = pBuf->ulSp1Len + pBuf->ulGapLen;   // old gap end position
    ulTarget = pBuf->ulSp1Len + ulNewGap;         // new gap end position
    for ( i = pBuf->ulSp2Len-1; i >= 0; i-- ) {
        pBuf->pulItems[ ulTarget+i ] = pBuf->pulItems[ ulSource+i ];
        pBuf->pulItems[ ulSource+i ] = 0;
    }

    pBuf->ulGapLen = ulNewGap;
    return TRUE;
}


/* ------------------------------------------------------------------------- *
 * LineBuffer_Dump()                                                         *
 *                                                                           *
 * Debugging function.  Dumps the contents of the line buffer in a human-    *
 * readable format to the specified file (which must be either a previously- *
 * opened file handle, or one of the standard handles like stdout.           *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   FILE      *f  : Handle of the file to dump the output to                *
 *   LBOBUFFER pBuf: Buffer object                                           *
 *   BOOL      all : If TRUE, shows the entire allocated buffer (gap & all)  *
 *                   If FALSE, only shows the items within the buffer.       *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   The number of items shown.                                              *
 * ------------------------------------------------------------------------- */
ULONG LineBuffer_Dump( FILE *f, LBOBUFFER buffer, BOOL all )
{
    ULONG count,
          i;

    if ( all ) {
        count = buffer.ulSize;
        fprintf( f, "Line breaks: [%2u/%2u]: ", ITEMCOUNT( buffer ), count );
        for ( i = 0; i < count; i++ ) {
            fprintf( f, "|%u", buffer.pulItems[ i ] );
        }
        fprintf( f, "|\n                     Gap position: %u, size: %u\n\n", buffer.ulSp1Len, buffer.ulGapLen );
    }
    else {
        count = ITEMCOUNT( buffer );
        fprintf( f, "Line breaks: [%2u]: ", count );
        for ( i = 0; i < count; i++ ) {
            fprintf( f, "|%u", all? buffer.pulItems[ i ]: LineBuffer_ItemAt( &buffer, i ));
        }
        fprintf(f, "|\n");
    }

    return count;
}
