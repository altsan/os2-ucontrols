// ***************************************************************************
// CONSTANTS

// The maximum number of characters that GPI can render in a single operation
#define UCS_MAX_RENDER          256         // value in UniChars
#define CB_MAX_RENDER           512         // value in bytes


// ***************************************************************************
// FUNCTIONS

LONG  DrawTabbedUnicodeText( HPS hps, PPOINTL pptl, RECTL rcl, FONTMETRICS fm, ULONG ulTabSize, UniChar *puszText, ULONG ulChars );
ULONG FitTextWidth( HPS hps, PCHAR pchText, ULONG cbText, LONG lWidth, PPOINTL pptl, ULONG ulTabSize, FONTMETRICS fm, ULONG ulCodepage, PBYTE pDBCS );
BOOL  FixedWidthIncrements( UniChar *psuText, ULONG ulLength, PLONG alInc, FONTMETRICS fm );
ULONG QueryTextWidth( HPS hps, PCHAR pchText, ULONG cbText, FONTMETRICS fm, ULONG ulTabSize, ULONG usCP );
ULONG QueryUnicodeTextWidth( HPS hps, UniChar *puszText, ULONG ulChars, FONTMETRICS fm, ULONG ulTabSize );



