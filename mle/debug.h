#include <stdarg.h>

#ifdef DEBUG_LOG

FILE *dbg;

FILE *_dfopen( void );
void _dfclose( void );
void _dprintf( char *psz, ... );

#define DEBUG_START     _dfopen
#define DEBUG_END       _dfclose
#define DEBUG_PRINTF    _dprintf

#else

void _nop( void );
void _nop1( char *psz, ... );

#define DEBUG_START     _nop
#define DEBUG_END       _nop
#define DEBUG_PRINTF    _nop1

#endif


// Handy message box for errors and debug messages
#define ErrorPopup( text ) \
    WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, text, "Error", 0, MB_OK | MB_ERROR )


