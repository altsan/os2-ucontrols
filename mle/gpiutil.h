// ***************************************************************************
// CONSTANTS

// Default width of a scrollbar
#define SB_SIZE                 14


// ***************************************************************************
// FUNCTIONS

void  DrawNice3DBorder( HPS hps, RECTL rcl );
LONG  GetCurrentDPI( HWND hwnd );
BOOL  GetImageFont( HPS hps, PSZ pszFont, LONG lPts, LONG lDPI, PFATTRS pfAttrs );
BOOL  SetFontFromPP( HPS hps, HWND hwnd, LONG lDPI, PFATTRS pfa );
BOOL  SetFontFromAttrs( HPS hps, LONG lDPI, PFATTRS pfa, FONTMETRICS fm );



