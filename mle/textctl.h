// ----------------------------------------------------------------------------
// CONSTANTS

// Window class name
#define WC_UMLE                 "UCtlMultiLineEditor"

#define UMTA_BOLD               0x1         // Bold text
#define UMTA_ITALIC             0x2         // Italic text
#define UMTA_UNDERLINE          0x4         // Underlined text
#define UMTA_STRIKEOUT          0x8         // Struck-out text
#define UMTA_RIGHT2LEFT         0x100       // BiDi right-to-left text

#define MLFIE_UCS               4           // Import/export format is UCS-2

// ----------------------------------------------------------------------------
// TYPEDEFS

// Control data for the Unicode Multi-Line Editor
typedef struct _UMLE_Data {
    ULONG   cbCtlData;      // size of this structure in bytes
    USHORT  afIEFormat;     // import/export format
    ULONG   cchText;        // text limit
    IPT     iptAnchor;      // selection anchor point
    IPT     iptCursor;      // selection cursor point
    LONG    cxFormat;       // formatting-rectangle width in pels
    LONG    cyFormat;       // formatting-rectangle height in pels
    ULONG   afFormatFlags;  // format flags
} UMLECTLDATA, *PUMLECTLDATA;


// ----------------------------------------------------------------------------
// FUNCTIONS

BOOL UWinRegisterMLE( HAB hab );

