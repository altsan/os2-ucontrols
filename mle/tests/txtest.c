#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "textseq.h"


#define TEST_TEXT   "The fox jumps over the lazy dog.  Woven silk pyjamas exchanged for blue quartz?"

int main( int argc, char *argv[] )
{
    EDITORTEXT  text;
    ULONG     count,
              val,
              pos;
    CHAR      szText[ 256 ];

    if ( !TextCreate( &text ))
        return 1;

    TextInitContents( text, TEST_TEXT, strlen( TEST_TEXT ));

    printf("Text length: %u bytes\n", TextLength( text ));
    count = TextSequence( text, szText, 0, 255 );
    szText[ count ] = 0;
    printf("Text contents: %s\n\n", szText );

    TextInsert( text, "quick brown ", 4, 12 );
    printf("Text length: %u bytes\n", TextLength( text ));
    count = TextSequence( text, szText, 0, 255 );
    szText[ count ] = 0;
    printf("Text contents: %s\n\n", szText );

    TextInsert( text, "\r\n", 46, 2 );
    printf("Text length: %u bytes\n", TextLength( text ));
    count = TextSequence( text, szText, 0, 255 );
    szText[ count ] = 0;
    printf("Text contents: %s\n\n", szText );

    TextInsert( text, "  Yes!", count, 6 );
    printf("Text length: %u bytes\n", TextLength( text ));
    count = TextSequence( text, szText, 0, 255 );
    szText[ count ] = 0;
    printf("Text contents: %s\n", szText );

/*
    printf("+ %2u @%2u\t", 5, 0 );
    LineBuffer_Insert( &buffer, 5, 0 );
    LineBuffer_Dump( stdout, buffer, FALSE );
    //LineBuffer_Dump( stdout, buffer, TRUE );
    printf("+ %2u @%2u\t", 10, 1 );
    LineBuffer_Insert( &buffer, 10, 1 );
    LineBuffer_Dump( stdout, buffer, FALSE );
    //LineBuffer_Dump( stdout, buffer, TRUE );
    printf("+ %2u @%2u\t", 15, 2 );
    LineBuffer_Insert( &buffer, 15, 2 );
    LineBuffer_Dump( stdout, buffer, FALSE );
    //LineBuffer_Dump( stdout, buffer, TRUE );
    printf("+ %2u @%2u\t", 25, 3 );
    LineBuffer_Insert( &buffer, 25, 3 );
    LineBuffer_Dump( stdout, buffer, FALSE );
    //LineBuffer_Dump( stdout, buffer, TRUE );
    printf("+ %2u @%2u\t", 20, 3 );
    LineBuffer_Insert( &buffer, 20, 3 );
    LineBuffer_Dump( stdout, buffer, FALSE );
    //LineBuffer_Dump( stdout, buffer, TRUE );
    printf("+ %2u @%2u\t", 30, 5 );
    LineBuffer_Insert( &buffer, 30, 5 );
    LineBuffer_Dump( stdout, buffer, FALSE );
    //LineBuffer_Dump( stdout, buffer, TRUE );
    printf("+ %2u @%2u\t", 45, 6 );
    LineBuffer_Insert( &buffer, 45, 6 );
    LineBuffer_Dump( stdout, buffer, FALSE );
    //LineBuffer_Dump( stdout, buffer, TRUE );

    count = LineBuffer_Count( &buffer );
    pos = LineBuffer_FindPosition( &buffer, 0, count-1, 50 );
    printf("+ %2u @%2u\t", 50, pos );
    LineBuffer_Insert( &buffer, 50, pos );
    LineBuffer_Dump( stdout, buffer, FALSE );

    count = LineBuffer_Count( &buffer );
    pos = LineBuffer_FindPosition( &buffer, 0, count-1, 40 );
    printf("+ %2u @%2u\t", 40, pos );
    LineBuffer_Insert( &buffer, 40, pos );
    LineBuffer_Dump( stdout, buffer, FALSE );

    count = LineBuffer_Count( &buffer );
    pos = LineBuffer_FindPosition( &buffer, 0, count-1, 35 );
    printf("+ %2u @%2u\t", 35, pos );
    LineBuffer_Insert( &buffer, 35, pos );
    LineBuffer_Dump( stdout, buffer, FALSE );

    count = LineBuffer_Count( &buffer );
    pos = LineBuffer_FindPosition( &buffer, 0, count-1, val );
    printf("Position for value %u is: %u\n\n", val, pos );

    LineBuffer_Dump( stdout, buffer, TRUE );
*/
    TextFree( &text );
    return 0;
}


