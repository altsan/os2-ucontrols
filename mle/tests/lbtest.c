#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include "linebuf.h"


int main( int argc, char *argv[] )
{
    LBOBUFFER buffer;
    ULONG     count,
              val,
              pos;

    if ( argc < 2 ) val = 100;
    else sscanf( argv[1], "%u", &val );

    if ( !LineBuffer_Init( &buffer, 10 ))
        return 1;

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

    LineBuffer_Free( &buffer );
    return 0;
}


