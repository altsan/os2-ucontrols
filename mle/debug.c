#include <stdio.h>
#include <stdlib.h>

#include "debug.h"


/* ------------------------------------------------------------------------- *
 * _dfopen                                                                   *
 * ------------------------------------------------------------------------- */
FILE *_dfopen( void )
{
#ifdef DEBUG_LOG
    dbg = fopen( DEBUG_LOG, "w");
#endif
}


/* ------------------------------------------------------------------------- *
 * _dfclose                                                                  *
 * ------------------------------------------------------------------------- */
void _dfclose( void )
{
#ifdef DEBUG_LOG
    fclose( dbg );
#endif
}


/* ------------------------------------------------------------------------- *
 * _dprintf                                                                  *
 * ------------------------------------------------------------------------- */
void _dprintf( char *psz, ... )
{
#ifdef DEBUG_LOG
    va_list vl;
    va_start( vl, psz );
    vfprintf( dbg, psz, vl );
    va_end( vl );
#endif
}

void _nop( void ) {;}
void _nop1( char *psz, ... ) {;}

