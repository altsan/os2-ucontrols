/*****************************************************************************
 * linebuf.h                                                                 *
 *                                                                           *
 * Interface to the "line offset buffer" data model, used to store the list  *
 * of byte positions within the editor text where line wrapping is required. *
 *                                                                           *
 * The model used is a gap buffer where each item is an ULONG.  It is        *
 * intended that the items be kept sorted in ascending order; managing this  *
 * is the application's responsibility (since all items are inserted singly, *
 * it is easy enough to maintain sort order by inserting each item in the    *
 * correct position).  Still, the only function which actually depends on    *
 * the items being sorted is LineBuffer_FindPosition(), which is used to     *
 * find the sorted insert position for a given new value.                    *
 *                                                                           *
 * The gap buffer logic is largely copied from textseq.c, with the obvious   *
 * difference that the contents are ULONGs rather than bytes; but the        *
 * supported operations are very slightly different (e.g. there is no way to *
 * query a whole sequence of items at once, only individual ones).           *
 *                                                                           *
 *****************************************************************************/


// ---------------------------------------------------------------------------
// CONSTANTS
//

#define LB_INVALID_POSITION     0xFFFFFFFF


// ---------------------------------------------------------------------------
// TYPEDEFS
//

// The implementation (gap buffer) of the data structure.
//
typedef struct _line_buffer {
    PULONG pulItems;            // The actual buffer containing values
    ULONG  ulSize,              // Total allocated buffer size
           ulSp1Len,            // Number of items before the gap
           ulSp2Len,            // Number of items after the gap
           ulGapLen;            // Size of the gap
    /* ------------------------------------------------------------- *
     * NOTES: Total number of items         == ulSp1Len + ulSp2Len   *
     *        Start of the pre-gap section  == 0                     *
     *        Start of the gap              == ulSp1Len              *
     *        Start of the post-gap section == ulSp1Len + ulGapLen   *
     * ------------------------------------------------------------- */
} LBOBUFFER, *PLBOBUFFER;


// ---------------------------------------------------------------------------
// FUNCTION DECLARATIONS
//

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
BOOL  LineBuffer_Clear( PLBOBUFFER pBuf, ULONG ulPosition );


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
ULONG LineBuffer_Count( PLBOBUFFER pBuf );


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
ULONG LineBuffer_Expand( PLBOBUFFER pBuffer, ULONG ulRequested );


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
void  LineBuffer_Free( PLBOBUFFER pBuf );


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
ULONG LineBuffer_FindPosition( PLBOBUFFER pBuf, ULONG ulStart, ULONG ulEnd, ULONG ulValue );


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
ULONG LineBuffer_Init( PLBOBUFFER pBuf, ULONG ulInitial );


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
BOOL  LineBuffer_Insert( PLBOBUFFER pBuf, ULONG cbValue, ULONG ulPosition );


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
ULONG LineBuffer_ItemAt( PLBOBUFFER pBuf, ULONG ulPosition );


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
BOOL  LineBuffer_MoveGap( PLBOBUFFER pBuf, ULONG ulPosition );


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
BOOL  LineBuffer_OpenGap( PLBOBUFFER pBuf, ULONG ulRequired );





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
ULONG LineBuffer_Dump( FILE *f, LBOBUFFER buffer, BOOL all );


