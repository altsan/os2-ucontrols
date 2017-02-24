/*****************************************************************************
 * textseq.h                                                                 *
 *                                                                           *
 * Interface to the text sequence buffer.  The implementation details are    *
 * deliberately hidden from the caller with the idea that this may make it   *
 * easier to replace the implementation some day if desired.  (The current   *
 * implementation, in textseq.c, is a gap buffer model - but it should be    *
 * possible to change this to something else like piece-tables, without      *
 * having to change the interface.)                                          *
 *                                                                           *
 * The buffer is treated as a sequence of bytes, no more and no less.  NO    *
 * assumptions are made about how these bytes are organized or interpreted.  *
 * All sequence lengths are delimited by explicit byte counts; neither NULL  *
 * bytes nor any other values are taken into account.  (If the application   *
 * cares about such things, it is up to the application to worry about it    *
 * after retrieving the values from the buffer.)                             *
 *                                                                           *
 * The only context in which buffer items are not treated as type char is    *
 * the TextWCharAt() function, which takes and returns values of type        *
 * wchar_t (however that is defined by the current compiler).  This is       *
 * provided as a convenience for applications which need to retrieve single  *
 * UCS-2 values from the buffer.                                             *
 *                                                                           *
 *****************************************************************************/


// ---------------------------------------------------------------------------
// DATA TYPES
//

typedef void * EDITORTEXT;


// ---------------------------------------------------------------------------
// FUNCTION DECLARATIONS
//

/* ------------------------------------------------------------------------- *
 * TextByteAt                                                                *
 *                                                                           *
 * Returns the byte at the given text position.  If the position is invalid, *
 * 0 (a null-byte) is returned.                                              *
 * ------------------------------------------------------------------------- */
unsigned char TextByteAt( EDITORTEXT text, unsigned long ulPosition );


/* ------------------------------------------------------------------------- *
 * TextWCharAt                                                               *
 *                                                                           *
 * Returns the wide-character type (wchar_t) at the given position.  The     *
 * position in this case is also assumed to be a wchar_t offset, and is      *
 * internally converted into bytes accordingly.  If the position is invalid, *
 * 0 (a null character) is returned.                                         *
 * ------------------------------------------------------------------------- */
wchar_t TextWCharAt( EDITORTEXT text, unsigned long ulPosition );


/* ------------------------------------------------------------------------- *
 * TextClearContents()                                                       *
 *                                                                           *
 * Erases the current text contents (but keeps the buffer allocated).        *
 * ------------------------------------------------------------------------- */
int TextClearContents( EDITORTEXT text );


/* ------------------------------------------------------------------------- *
 * TextCreate()                                                              *
 *                                                                           *
 * Creates a new text data structure.  The actual buffer is not allocated;   *
 * use TextInitContents() to allocate the buffer.  The data structure should *
 * be deallocated with TextFree() when no longer required.                   *
 * ------------------------------------------------------------------------- */
int TextCreate( EDITORTEXT *pText );


/* ------------------------------------------------------------------------- *
 * TextDelete                                                                *
 *                                                                           *
 * Deletes one or more bytes from the sequence, starting at the specified    *
 * position.                                                                 *
 * ------------------------------------------------------------------------- */
int TextDelete( EDITORTEXT text, unsigned long ulPosition, unsigned long ulLength );


/* ------------------------------------------------------------------------- *
 * TextDestroyContents()                                                     *
 *                                                                           *
 * Frees the text buffer, but merely zeroes the rest of the data structure.  *
 * ------------------------------------------------------------------------- */
int TextDestroyContents( EDITORTEXT text );


/* ------------------------------------------------------------------------- *
 * TextFree()                                                                *
 *                                                                           *
 * Frees the text data structure, including freeing the buffer if necessary. *
 * ------------------------------------------------------------------------- */
int TextFree( EDITORTEXT *pText );


/* ------------------------------------------------------------------------- *
 * TextInitContents()                                                        *
 *                                                                           *
 * Initializes the text data structure.  Populates it with the specified     *
 * text, if any; otherwise allocates an empty buffer.                        *
 * ------------------------------------------------------------------------- */
int TextInitContents( EDITORTEXT text, unsigned char *pchText, unsigned long cbText );


/* ------------------------------------------------------------------------- *
 * TextInsert                                                                *
 *                                                                           *
 * Inserts one or more bytes into the sequence at the specified position.    *
 * ------------------------------------------------------------------------- */
int TextInsert( EDITORTEXT text, unsigned char *pch, unsigned long ulPosition, unsigned long ulLength );


/* ------------------------------------------------------------------------- *
 * TextLength()                                                              *
 *                                                                           *
 * Returns the length of the text within the buffer.                         *
 * ------------------------------------------------------------------------- */
unsigned long TextLength( EDITORTEXT text );


/* ------------------------------------------------------------------------- *
 * TextSequence                                                              *
 *                                                                           *
 * Copies a character sequence (string) of the requested length from the     *
 * text buffer into an output buffer, starting at the specified position.    *
 * (In other words, retrieves a subset of text and passes it to the caller.) *
 * Returns the number of bytes copied (which may be less than the requested  *
 * length if the end of the text is reached.)                                *
 * ------------------------------------------------------------------------- */
unsigned long TextSequence( EDITORTEXT text, unsigned char *pchText, unsigned long ulPosition, unsigned long ulLength );


