/*****************************************************************************
 * textseq.c                                                                 *
 *                                                                           *
 * Implements a text editor data model.  The current implementation is a     *
 * 'gap' (or split) buffer, based on the theory described by Charles Crowley *
 * in his paper "Data Structures for Text Sequences" (1998, University of    *
 * New Mexico).                                                              *
 *                                                                           *
 * Note that we make no assumptions about how the buffer contents will be    *
 * interpreted.  Matters of character width (whether fixed-width or MBCS),   *
 * codepage, string terminators (NULLs or anything else) etc. are entirely   *
 * outside our scope here.  From our point of view, the buffer simply        *
 * contains a sequence of bytes with a specified length.  It will be up to   *
 * whatever program uses this model to decide what the bytes represent, and  *
 * how to deal with them accordingly.                                        *
 *                                                                           *
 * All references to 'characters' in this file refer to single bytes (char), *
 * with the sole exception of TextWCharAt() which uses the wchar_t type.     *
 *                                                                           *
 * This file attempts to be platform-agnostic in order to make it easier to  *
 * reuse in other applications.  Platform-specific types and APIs should be  *
 * encapsulated in #ifdef's when necessary.                                  *
 *                                                                           *
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <stddef.h>

#include "debug.h"

#ifdef __OS2__
#define INCL_DOSERRORS
#include <os2.h>

PVOID allocate_memory( size_t size )
{
    APIRET rc;
    PVOID  pObj;

    rc = DosAllocMem( (PPVOID) &pObj, size, PAG_READ | PAG_WRITE | PAG_COMMIT );
    if ( rc != NO_ERROR) return NULL;
    return pObj;
}

void free_memory( PVOID pObj )
{
    DosFreeMem( pObj );
}

#else

void *allocate_memory( size_t size )
{
    void *pObj;
    pObj = (void *) malloc( size );
    if ( pObj ) memset( pObj, 0, size );
    return pObj;
}

void free_memory( void *pObj )
{
    free( pObj );
}

#endif


// ---------------------------------------------------------------------------
// CONSTANTS
//
#define INITIAL_BUF_SIZE 8192   // default initial buffer size (including gap)

#define BUF_INC_DEFAULT  8192   // default buffer size increment
#define BUF_INC_MEDIUM  16384   // increment for when buffer > BUF_MAX_SMALL
#define BUF_INC_LARGE   32768   // incremement for when buffer > BUF_MAX_MEDIUM

#define BUF_MAX_SMALL   0xFFFF  // cutoff of a 'small' sized buffer
#define BUF_MAX_MEDIUM  0x3FFFF // cutoff of a 'medium' sized buffer


// ---------------------------------------------------------------------------
// TYPEDEFS
//

// The internal implementation (gap buffer) of the text data structure.
// (EDITORTEXT is the public type name to be used by external code.)
//
typedef struct _text_sequence {
    unsigned char *pchContents;     // The actual text buffer
    unsigned long ulSize,           // Total allocated buffer size
                  ulSp1Len,         // Length of text before the gap (span 1)
                  ulSp2Len,         // Length of text after the gap (span 2)
                  ulGapLen;         // Length of the gap
    /* ------------------------------------------------------------- *
     * NOTES: Total text length             == ulSp1Len + ulSp2Len   *
     *        Position of the pre-gap text  == 0                     *
     *        Position of the gap           == ulSp1Len              *
     *        Position of the post-gap text == ulSp1Len + ulGapLen   *
     * ------------------------------------------------------------- */
} TEXT, *PTEXT, *EDITORTEXT;



// ---------------------------------------------------------------------------
// MACROS
//

// Return the absolute buffer position of the requested character position
#define TEXTPOS2ABS( text, pos ) \
  (( (pos) < (text).ulSp1Len ) ? (pos) : ((text).ulGapLen + (pos) ))

// Return the length of the text
#define TEXTLEN( text )     ( (text).ulSp1Len + (text).ulSp2Len )

// Return the buffer position following the last character
#define TEXTEND( text ) \
  ( (text).ulSp1Len + (text).ulGapLen + (text).ulSp2Len )


// ---------------------------------------------------------------------------
// FUNCTION DECLARATIONS
//

// Internal
int ExpandBuffer( PTEXT pText, unsigned long cbRequested );
int MoveGap( PTEXT pText, unsigned long ulPosition );
int ResetGap( PTEXT pText, unsigned long cbRequired );

// Public
unsigned char TextByteAt( PTEXT pText, unsigned long ulPosition );
int           TextClearContents( PTEXT pText );
int           TextCreate( PTEXT *ppText );
int           TextDelete( PTEXT pText, unsigned long ulPosition, unsigned long ulLength );
int           TextDestroyContents( PTEXT pText );
int           TextFree( PTEXT *ppText );
int           TextInitContents( PTEXT pText, unsigned char *pchText, unsigned long cbText );
int           TextInsert( PTEXT pText, unsigned char *pch, unsigned long ulPosition, unsigned long ulLength );
unsigned long TextLength( PTEXT pText );
unsigned long TextSequence( PTEXT pText, unsigned char *pchText, unsigned long ulPosition, unsigned long ulLength );
wchar_t       TextWCharAt( PTEXT pText, unsigned long ulPosition );



// ===========================================================================
// INTERNAL FUNCTIONS
// ===========================================================================


/* ------------------------------------------------------------------------- *
 * ExpandBuffer()                                                            *
 *                                                                           *
 * Increases the size of the text buffer, to the requested size if one is    *
 * specified, or else by a default increment.  The contents and offsets of   *
 * the buffer are preserved as they were when the function was called.       *
 *                                                                           *
 * The size of the default increment varies depending on the total buffer    *
 * size.  For buffers up to 64 kB, the increment is 8 kB; if the buffer is   *
 * between 64 and 256 kB, the increment is 16 Kb; for larger buffers, the    *
 * increment will be 32 kB.                                                  *
 * ------------------------------------------------------------------------- */
int ExpandBuffer( PTEXT pText, unsigned long cbRequested )
{
    unsigned long ulCurrentSize,
                  ulDefInc,
                  ulNewSize;
    unsigned char *pchNew;


    ulCurrentSize = pText->ulSize;
    if ( cbRequested && ( cbRequested <= ulCurrentSize ))
        return 1;

    if (( ! pText->pchContents ) || ( ! ulCurrentSize ))
        if ( ! TextInitContents( pText, NULL, 0 )) return 0;

    if ( ulCurrentSize <= BUF_MAX_SMALL )       ulDefInc = BUF_INC_DEFAULT;
    else if ( ulCurrentSize <= BUF_MAX_MEDIUM ) ulDefInc = BUF_INC_MEDIUM;
    else                                        ulDefInc = BUF_INC_LARGE;

    ulNewSize = cbRequested ? ( cbRequested + ulDefInc ): ( ulCurrentSize + ulDefInc );

#ifdef DEBUG_LOG
fprintf(dbg, "Increasing buffer to %u bytes (%u requested)\n", ulNewSize, cbRequested );
#endif

    pchNew = allocate_memory( ulNewSize );
    if ( pchNew == NULL ) return 0;

    memcpy( pchNew, pText->pchContents, pText->ulSize );
    free_memory( pText->pchContents );
    pText->pchContents = pchNew;
    pText->ulSize = ulNewSize;
//    pText->pchContents = (unsigned char *) realloc( pText->pchContents, ulNewSize );
//    if ( pText->pchContents == NULL ) return 0;

    return 1;
}


/* ------------------------------------------------------------------------- *
 * MoveGap()                                                                 *
 *                                                                           *
 * Moves the current position of the gap within the text buffer.             *
 * ------------------------------------------------------------------------- */
int MoveGap( PTEXT pText, unsigned long ulPosition )
{
    unsigned long ulGapStart,   // New starting position of the gap
                  ulShift,      // Number of bytes to shift
                  ulOldSp1Len,  // Old length of the pre-gap text
                  ulSource,     // Starting source index of bytes to move
                  ulTarget;     // Starting destination index of bytes to move
    long i;

    if ( ulPosition > pText->ulSize ) return 0;

    ulGapStart  = ulPosition;
    ulOldSp1Len = pText->ulSp1Len;

//printf("Moving gap to %u\n", ulPosition );

    // shift gap to the right
    if ( ulGapStart > ulOldSp1Len ) {
        ulShift  = ulGapStart - ulOldSp1Len;
        ulSource = ulOldSp1Len + pText->ulGapLen;   // (old) start of post-gap text
        ulTarget = ulOldSp1Len;                     // (old) start of gap
        for ( i = 0; i < ulShift; i++ ) {
            pText->pchContents[ ulTarget+i ] = pText->pchContents[ ulSource+i ];
            pText->pchContents[ ulSource+i ] = 0;
        }
        pText->ulSp1Len = ulGapStart;
        pText->ulSp2Len -= ulShift;
    }
    // shift gap to the left
    else if ( ulGapStart < ulOldSp1Len ) {
        ulShift  = ulOldSp1Len - ulGapStart;
        ulSource = ulGapStart;                      // (new) start of gap
        ulTarget = ulGapStart + pText->ulGapLen;    // (new) start of post-gap text
        for ( i = ulShift-1; i >= 0; i-- ) {
            pText->pchContents[ ulTarget+i ] = pText->pchContents[ ulSource+i ];
            pText->pchContents[ ulSource+i ] = 0;
        }
        pText->ulSp1Len = ulGapStart;
        pText->ulSp2Len += ulShift;
    }

    return 1;
}


/* ------------------------------------------------------------------------- *
 * ResetGap()                                                                *
 *                                                                           *
 * Resets the gap by shifting the post-gap text back until the gap is either *
 * 50% of the total buffer, or the minimum required size plus a standard     *
 * increment.  To be called whenever the gap is about to fill up.  If there  *
 * is insufficient space in the buffer to shift the subsequent text in this  *
 * way, the buffer itself is expanded (and the gap size retargeted to 50% of *
 * the new buffer size).                                                     *
 * ------------------------------------------------------------------------- */
int ResetGap( PTEXT pText, unsigned long cbRequired )
{
    unsigned long cbNewGap,     // New gap length
                  ulSource,     // Starting source index of bytes to move
                  ulTarget;     // Starting destination index of bytes to move
    long i;

    cbNewGap = pText->ulSize / 2;
    while ( cbNewGap < ( cbRequired + BUF_INC_DEFAULT )) cbNewGap *= 2;
    if ( pText->ulGapLen >= cbNewGap ) return 1;    // nothing to do

    if (( cbNewGap + TEXTLEN( *pText )) > pText->ulSize ) {
        if ( ! ExpandBuffer( pText, 0 )) return 0;
        cbNewGap = pText->ulSize / 2;
    }

#ifdef DEBUG_LOG
fprintf(dbg, "Expanding gap to %u bytes (%u requested)\n", cbNewGap, cbRequired );
#endif

    ulSource = pText->ulSp1Len + pText->ulGapLen;   // old start of post-gap text
    ulTarget = pText->ulSp1Len + cbNewGap;          // new start of post-gap text
    for ( i = pText->ulSp2Len-1; i >= 0; i-- ) {
        pText->pchContents[ ulTarget+i ] = pText->pchContents[ ulSource+i ];
        pText->pchContents[ ulSource+i ] = 0;
    }

    pText->ulGapLen = cbNewGap;
    return 1;
}


// ===========================================================================
// PUBLIC FUNCTIONS
// ===========================================================================


/* ------------------------------------------------------------------------- *
 * TextByteAt                                                                *
 *                                                                           *
 * Returns the byte at the given text position.  If the position is invalid, *
 * 0 (a null-byte) is returned.                                              *
 * ------------------------------------------------------------------------- */
unsigned char TextByteAt( PTEXT pText, unsigned long ulPosition )
{
    if ( ulPosition >= TEXTLEN( *pText )) return 0;
    return ( pText->pchContents[ TEXTPOS2ABS(*pText, ulPosition) ] );
}


/* ------------------------------------------------------------------------- *
 * TextClearContents()                                                       *
 *                                                                           *
 * Erases the current text contents (but keeps the buffer allocated).        *
 * ------------------------------------------------------------------------- */
int TextClearContents( PTEXT pText )
{
    memset( pText->pchContents, 0, pText->ulSize );
    pText->ulSp1Len = 0;
    pText->ulSp2Len = 0;
    return 1;
}


/* ------------------------------------------------------------------------- *
 * TextCreate()                                                              *
 *                                                                           *
 * Creates a new text data structure.  The actual buffer is not allocated;   *
 * use TextInitContents() to allocate the buffer.  The data structure should *
 * be deallocated with TextFree() when no longer required.                   *
 * ------------------------------------------------------------------------- */
int TextCreate( PTEXT *ppText )
{
    PTEXT pText;
    pText = (PTEXT) allocate_memory( sizeof( TEXT ));
    if ( !pText ) return 0;
    *ppText = pText;
    return 1;
}


/* ------------------------------------------------------------------------- *
 * TextDelete                                                                *
 *                                                                           *
 * Deletes one or more bytes from the sequence, starting at the specified    *
 * position.                                                                 *
 * ------------------------------------------------------------------------- */
int TextDelete( PTEXT pText, unsigned long ulPosition, unsigned long ulLength )
{
    int i;

    // Ensure that the deleted bytes are always immediately following the gap
    if ( ulPosition != pText->ulSp1Len )
        if ( ! MoveGap( pText, ulPosition )) return 0;

    // Now zeroize the bytes
    for ( i = 0; i < ulLength; i++ )
        pText->pchContents[ TEXTPOS2ABS(*pText, ulPosition+i) ] = 0;
    pText->ulGapLen += ulLength;
    pText->ulSp2Len -= ulLength;

    return 1;
}


/* ------------------------------------------------------------------------- *
 * TextDestroyContents()                                                     *
 *                                                                           *
 * Destroys the text buffer and zeroes the rest of the data structure.       *
 * ------------------------------------------------------------------------- */
int TextDestroyContents( PTEXT pText )
{
    if ( pText->pchContents != NULL ) free_memory( pText->pchContents );
    pText->pchContents = NULL;
    pText->ulSize      = 0;
    pText->ulSp1Len    = 0;
    pText->ulSp2Len    = 0;
    pText->ulGapLen    = 0;
    return 1;
}


/* ------------------------------------------------------------------------- *
 * TextFree()                                                                *
 *                                                                           *
 * Frees the text data structure.  The buffer will be freed if necessary.    *
 * ------------------------------------------------------------------------- */
int TextFree( PTEXT *ppText )
{
    if ( !(*ppText) ) return 0;
    TextDestroyContents( *ppText );
    free_memory( *ppText );
    return 1;
}


/* ------------------------------------------------------------------------- *
 * TextInitContents()                                                        *
 *                                                                           *
 * Initializes the text data structure.  Populates it with the specified     *
 * text, if any; otherwise allocates an empty buffer.                        *
 * ------------------------------------------------------------------------- */
int TextInitContents( PTEXT pText, unsigned char *pchText, unsigned long cbText )
{
    if ( pText->pchContents ) TextDestroyContents( pText );

    if ( cbText && pchText ) {
        unsigned long ulInitial = INITIAL_BUF_SIZE;
        while ( ulInitial < ( cbText + BUF_INC_DEFAULT )) ulInitial *= 2;
        pText->pchContents = (unsigned char *) allocate_memory( ulInitial );
        if ( pText->pchContents == NULL ) return 0;
        memcpy( pText->pchContents, pchText, cbText );
        pText->ulSize   = ulInitial;
        pText->ulSp1Len = cbText;
        pText->ulSp2Len = 0;
        pText->ulGapLen = BUF_INC_DEFAULT;
    }
    else {
        pText->pchContents = (unsigned char *) allocate_memory( INITIAL_BUF_SIZE );
        if ( pText->pchContents == NULL ) return 0;
        pText->ulSize   = INITIAL_BUF_SIZE;
        pText->ulSp1Len = 0;
        pText->ulSp2Len = 0;
        pText->ulGapLen = INITIAL_BUF_SIZE / 2;
    }

    return 1;
}


/* ------------------------------------------------------------------------- *
 * TextInsert                                                                *
 *                                                                           *
 * Inserts one or more bytes into the sequence at the specified position.    *
 * ------------------------------------------------------------------------- */
int TextInsert( PTEXT pText, unsigned char *pch, unsigned long ulPosition, unsigned long ulLength )
{
    // Make sure the buffer exists
    if ( !pText || !pText->pchContents || !pText->ulSize )
        return 0;

    // Don't allow adding items more than one position past the end of the array
    if ( ulPosition > TEXTLEN( *pText )) return 0;

/*
#ifdef DEBUG_LOG
fprintf(dbg, "Inserting %u bytes at position %u\n", ulLength, ulPosition );
fprintf(dbg, " Buffer size:   %u\n", pText->ulSize );
fprintf(dbg, " Span 1 length: %u\n", pText->ulSp1Len );
fprintf(dbg, " Gap length:    %u\n", pText->ulGapLen );
fprintf(dbg, " Span 2 length: %u\n", pText->ulSp2Len );
fprintf(dbg, " Text size:     %u\n", TEXTLEN( *pText ));
fprintf(dbg, " First free:    %u\n", TEXTEND( *pText ));
#endif
*/

    if (( TEXTPOS2ABS( *pText, ulPosition ) == TEXTEND( *pText )) &&
          (( TEXTEND( *pText ) + ulLength ) < pText->ulSize ) && pText->ulSp2Len )
    {
        /* A cheap but useful optimization: if the insert position is just
         * after the last value, and there's enough space left at the end of
         * the buffer, then don't bother moving the gap - just append the new
         * data to the post-gap text.
         */
        unsigned long cbAddr = TEXTPOS2ABS( *pText, ulPosition );

        if ( ulLength == 1 )
            pText->pchContents[ cbAddr ] = *pch;
        else
            memcpy( pText->pchContents + cbAddr, pch, ulLength );
        pText->ulSp2Len += ulLength;
    }

    else {
        // TODO Optimize somehow by combining MoveGap and ResetGap when appropriate
        if ( ulPosition != pText->ulSp1Len )
            if ( ! MoveGap( pText, ulPosition )) return 0;
        if ( pText->ulGapLen <= ulLength )
            if ( ! ResetGap( pText, ulLength )) return 0;

        if ( ulLength == 1 )
            pText->pchContents[ ulPosition ] = *pch;
        else
            memcpy( pText->pchContents + ulPosition, pch, ulLength );
        pText->ulSp1Len += ulLength;
        pText->ulGapLen -= ulLength;
    }

    return 1;
}


/* ------------------------------------------------------------------------- *
 * TextLength()                                                              *
 *                                                                           *
 * Returns the length of the text within the buffer.                         *
 * ------------------------------------------------------------------------- */
unsigned long TextLength( PTEXT pText )
{
    if ( !pText ) return 0;
    return ( TEXTLEN(*pText) );
}


/* ------------------------------------------------------------------------- *
 * TextSequence                                                              *
 *                                                                           *
 * Writes a character sequence (string) of the requested length, starting at *
 * at the specified position.  Returns the number of bytes written (which    *
 * may be less than the requested length if the end of the text is reached.) *
 * ------------------------------------------------------------------------- */
unsigned long TextSequence( PTEXT pText, unsigned char *pchText, unsigned long ulPosition, unsigned long ulLength )
{
    unsigned long ulOffset;
    unsigned long i;

    if ( !pchText ) return 0;
    if ( ulPosition >= TEXTLEN( *pText )) return 0;

    for ( i = 0; i < ulLength; i++ ) {
        ulOffset = TEXTPOS2ABS( *pText, ulPosition + i );
        if ( ulOffset >= TEXTEND( *pText )) break;
        pchText[ i ] = pText->pchContents[ ulOffset ];
    }
    return i;
}


/* ------------------------------------------------------------------------- *
 * TextWCharAt                                                               *
 *                                                                           *
 * Returns the wide-character type (wchar_t) at the given position.  The     *
 * position in this case is also assumed to be a wchar_t offset, and is      *
 * internally converted into bytes accordingly.  If the position is invalid, *
 * 0 (a null character) is returned.                                         *
 * ------------------------------------------------------------------------- */
wchar_t TextWCharAt( PTEXT pText, unsigned long ulPosition )
{
    ulPosition *= sizeof( wchar_t );
    if ( ulPosition >= TEXTLEN( *pText )) return 0;
    return ( (wchar_t)(pText->pchContents[ TEXTPOS2ABS(*pText, ulPosition) ]) );
}



