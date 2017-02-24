/*****************************************************************************
 * byteparse.c                                                               *
 *                                                                           *
 * Routines for character and string parsing, validation, etc.               *
 *                                                                           *
 *****************************************************************************/


#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unidef.h>
#include <uconv.h>
#include "byteparse.h"


/* ------------------------------------------------------------------------- *
 * DumpUnicodeString                                                         *
 *                                                                           *
 * Used for debugging.  Performs a quick-and-dirty dump of a UCS-2 string    *
 * to the specified file; non-ASCII characters are replaced by 0x7F.         *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   FILE *output:   The output file to write to (must be open).             *
 *   UniChar *psu:   The UCS-2 string to dump.                               *
 *   ULONG ulLength: The number of characters in the string.                 *
 *                                                                           *
 * RETURNS: N/A                                                              *
 * ------------------------------------------------------------------------- */
void DumpUnicodeString( FILE *output, UniChar *psu, ULONG ulLength )
{
    ULONG i;

    fprintf( output, "\"");
    for( i = 0; i < ulLength; i++ )
        fprintf( output, "%lc", (*(psu+i) < 0x80) ? *(psu+i) : 0x7F );
    fprintf( output, "\"\n");
}


/* ------------------------------------------------------------------------- *
 * DumpUnicodeIncrements                                                     *
 *                                                                           *
 * Used for debugging.  Performs a quick-and-dirty dump of a UCS-2 string    *
 * and its increments vector to the specified file; non-ASCII characters are *
 * replaced by 0x7F.                                                         *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   FILE *output:   The output file to write to (must be open).             *
 *   UniChar *psu:   The UCS-2 string to dump.                               *
 *                                                                           *
 *   ULONG ulLength: The number of characters in the string.                 *
 *                                                                           *
 * RETURNS: N/A                                                              *
 * ------------------------------------------------------------------------- */
void DumpUnicodeIncrements( FILE *output, UniChar *psu, LONG *alInc, ULONG ulLength )
{
    ULONG i;

    fprintf( output, "[");
    for( i = 0; i < ulLength; i++ )
        fprintf( output, "%lc=%u/%u ", (*(psu+i) < 0x80) ? *(psu+i) : 0x7F, alInc[UPOS_TO_BYTEOFF(i)], alInc[UPOS_TO_BYTEOFF(i+1)] );
    fprintf( output, "]\n");
//  for( i = 0; i < UPOS_TO_BYTEOFF( ulLength ); i++ ) fprintf( output, " %u", alInc[i] );
//  fprintf( output, "\n");
}


/* ------------------------------------------------------------------------- *
 * FindBreakPosition                                                         *
 *                                                                           *
 * Find the last legal line-breaking position in the given string.  The      *
 * basic logic is as follows:                                                *
 *  - Start at the specified index (the last character in the string)        *
 *  - If it is legal to break after the character at this position, return   *
 *    the current position                                                   *
 *  - Otherwise, if it is legal to break before the character at this        *
 *    position, return one character position less than the current position *
 *                                                                           *
 * If no valid break position is found, simply return the last position in   *
 * the string.  This should effectively enable wrapping at the window width  *
 * as a fall back for too-long words.                                        *
 *                                                                           *
 * What constitutes "legal to break" is determined by the line-breaking      *
 * macros in byteparse.h, and has to be handled slightly differently         *
 * depending on whether the string is codepage-encoded or in UCS-2.          *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   PCH    pchText: The string as a sequence of bytes.                      *
 *   ULONG  ulMax  : The last character in the string (our starting index).  *
 *   USHORT usCP   : The codepage in which pchText is encoded.               *
 *                                                                           *
 * RETURNS: ULONG                                                            *
 *   The index of the last character before the line break.                  *
 * ------------------------------------------------------------------------- */
ULONG FindBreakPosition( PCH pchText, ULONG ulMax, USHORT usCP )
{
    ULONG   ulPos;
    UniChar *psuText;

    switch ( usCP ) {

        case 1200:
            /* Under codepage 1200, pchText is an array of UniChars, and
             * ulMax/ulPos refer to characters rather than bytes.
             */
            psuText = (UniChar *) pchText;
            ulPos   = ulMax;
            while ( ulPos ) {
                if ( UCS_WRAPPABLE_AFTER( psuText[ ulPos ] ) &&
                     !UCS_WRAP_FORBIDDEN( psuText[ ulPos + 1 ] ))
                {
/*
#ifdef DEBUG_LOG
fprintf(dbg,"  - %lc (U+%04X at position %u) is post-wrappable\n", psuText[ ulPos ], psuText[ ulPos ], ulPos );
#endif
 */
                    return ( ulPos );
                }
                else if ( UCS_WRAPPABLE_BEFORE(( psuText[ ulPos ] )))
                {
/*
#ifdef DEBUG_LOG
fprintf(dbg,"  - %lc (U+%04X at position %u) is pre-wrappable\n", psuText[ ulPos ], psuText[ ulPos ], ulPos );
#endif
*/
                    return ( ulPos - 1 );
                }
/*
#ifdef DEBUG_LOG
                else {
fprintf(dbg,"  - %lc (U+%04X at position %u) is not wrappable\n", psuText[ ulPos ], psuText[ ulPos ], ulPos );
                }
#endif
*/
                ulPos--;
            }
            break;

        case 932:
        case 935:
        case 942:
        case 943:
        case 944:
        case 949:
        case 950:
        case 1381:
        case 1386:
            /* At the moment, assume wrapping is legal anywhere under DBCS
             * codepages.  (This matches the behaviour of the standard MLE.)
             */
            break;

        default:
            ulPos = ulMax;
            while ( ulPos ) {
                if ( ASCII_WRAPPABLE_AFTER( pchText[ ulPos ] ))
                    return ( ulPos );

                // A few PC codepages contain the soft hyphen, so check those
                else if (((( usCP >= 850 ) && ( usCP <= 859 )) ||
                         ( usCP == 868 ) || ( usCP == 869 ))   &&
                         ( pchText[ ulPos ] == 0xF0 ))
                    return ( ulPos );
                else if (((( usCP >= 1250 ) && ( usCP <= 1257 )) ||
                         (( usCP >= 912 ) && ( usCP <= 916 ))    ||
                         (( usCP >= 920 ) && ( usCP <= 923 ))    ||
                         ( usCP == 813 ) || ( usCP == 819 )      ||
                         ( usCP == 1004 ))                       &&
                         ( pchText[ ulPos ] == 0xAD ))
                    return ( ulPos );

                else if ( ASCII_WRAPPABLE_BEFORE( pchText[ ulPos ] ))
                    return ( ulPos - 1 );

                ulPos--;
            }
            break;

    }

    return ulMax;
}


/* ------------------------------------------------------------------------- *
 * IsDBCSLeadByte                                                            *
 *                                                                           *
 * Determine if the given byte is the leading-byte of a multi-byte DBCS      *
 * character.  Based on code by Alessandro Cantatore.                        *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   CHAR  ch   : The byte value to query.                                   *
 *   PBYTE pDBCS: The leading byte-ranges for the specified codepage.        *
 *                                                                           *
 * RETURNS: BOOL                                                             *
 * ------------------------------------------------------------------------- */
BOOL IsDBCSLeadByte( CHAR ch, PBYTE pDBCS )
{
   while ( *pDBCS )
      if (( ch >= *pDBCS++) && ( ch <= *pDBCS++ )) return TRUE;
   return FALSE;
}


/* ------------------------------------------------------------------------- *
 * NextCharSize                                                              *
 *                                                                           *
 * Determine the number of bytes comprising the next character in the given  *
 * string, based on the indicated codepage.  The indicated character         *
 * position is assumed to be at a character boundary; if this is not the     *
 * case, the result of this function will be undefined.                      *
 *                                                                           *
 * ARGUMENTS:                                                                *
 *   PCHAR pchText: The string to examine.                                   *
 *   ULONG cbStart: The byte offset of the next character in the string.     *
 *   ULONG cbTotal: The total length of the string in bytes.                 *
 *   ULONG ulCP   : The codepage in which the string is encoded.             *
 *   PBYTE pDBCS  : The leading byte-ranges for the specified codepage.      *
 *                                                                           *
 * RETURNS: USHORT                                                           *
 * ------------------------------------------------------------------------- */
USHORT NextCharSize( PCHAR pchText, ULONG cbStart, ULONG cbTotal, ULONG ulCP, PBYTE pDBCS )
{
    USHORT cbSize;

    switch ( ulCP ) {
        case 1200:
            cbSize = sizeof( UniChar );
            break;

        case 1207:
            if ( UPF8_IS_2BYTELEAD( pchText[ cbStart ] ))
                cbSize = 2;
            else if ( UPF8_IS_3BYTELEAD( pchText[ cbStart ] ))
                cbSize = 3;
            else
                cbSize = 1;
            break;

        case 1208:
            if ( UTF8_IS_2BYTELEAD( pchText[ cbStart ] ))
                cbSize = 2;
            else if ( UTF8_IS_3BYTELEAD( pchText[ cbStart ] ))
                cbSize = 3;
            else if ( UTF8_IS_4BYTELEAD( pchText[ cbStart ] ))
                cbSize = 4;
            else
                cbSize = 1;
            break;

        case 437:
        case 813:
        case 819:
        case 850:
        case 852:
        case 855:
        case 856:
        case 857:
        case 859:
        case 860:
        case 861:
        case 862:
        case 863:
        case 864:
        case 865:
        case 866:
        case 869:
        case 874:
        case 878:
        case 912:
        case 915:
        case 921:
        case 923:
        case 1004:
        case 1125:
        case 1131:
        case 1250:
        case 1251:
        case 1252:
        case 1253:
        case 1254:
        case 1255:
        case 1256:
        case 1257:
        case 1275:
            return 1;
            break;

        default:
            if ( pDBCS && IsDBCSLeadByte( pchText[ cbStart ], pDBCS ))
                cbSize = 2;
            else
                cbSize = 1;
            break;
    }

    if (( cbStart + cbSize ) >= cbTotal )
        return 1;
    else
        return cbSize;
}


ULONG NextLineBreak( PCHAR pchText, ULONG cbText, USHORT usCP )
{
    ULONG cbChars;

    // TODO rewrite to limit string search to cbText bytes

    if ( !pchText || !cbText ) return 0;
    // Truncate the checked characters at the first line-break
    if ( usCP == 1200 ) {
        UniChar auLineBreaks[] = { 0xA, 0xB, 0xC, 0xD, 0x2028, 0x2029 };
        cbChars = UPOS_TO_BYTEOFF(
                      UniStrcspn( (UniChar *) pchText, auLineBreaks )
                  );
    }
    else {
        CHAR acLineBreaks[] = { 0xA, 0xB, 0xC, 0xD };
        cbChars = strcspn( pchText, acLineBreaks );
    }

    return cbChars;
}

