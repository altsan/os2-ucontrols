// ***************************************************************************
// MACROS
//

// ----------------------------------------------------------------------------
// Quick codepage checks
//

/* The following aren't literally ALL single-byte codepages, but they're all
 * the known, ASCII-based, PM-compatible ones which are likely to be used.
 */
#define SINGLE_BYTE_CODEPAGE( cp )  (( cp == 437 )  || ( cp == 813 )  || \
                                     ( cp == 819 )  || ( cp == 850 )  || \
                                     ( cp == 852 )  || ( cp == 855 )  || \
                                     ( cp == 856 )  || ( cp == 857 )  || \
                                     ( cp == 859 )  || ( cp == 860 )  || \
                                     ( cp == 861 )  || ( cp == 862 )  || \
                                     ( cp == 863 )  || ( cp == 864 )  || \
                                     ( cp == 864 )  || ( cp == 865 )  || \
                                     ( cp == 866 )  || ( cp == 869 )  || \
                                     ( cp == 874 )  || ( cp == 878 )  || \
                                     ( cp == 912 )  || ( cp == 915 )  || \
                                     ( cp == 921 )  || ( cp == 923 )  || \
                                     ( cp == 1004 ) || ( cp == 1125 ) || \
                                     ( cp == 1131 ) || ( cp == 1250 ) || \
                                     ( cp == 1251 ) || ( cp == 1252 ) || \
                                     ( cp == 1253 ) || ( cp == 1254 ) || \
                                     ( cp == 1255 ) || ( cp == 1256 ) || \
                                     ( cp == 1257 ) || ( cp == 1275 ))


// ----------------------------------------------------------------------------
// Word Boundaries / Text Wrapping
//

#define NEWLINE_CHAR( c )           ((( ( c >= 0xA ) && ( c <= 0xD ) ) || \
                                      ( (unsigned short) c == 0x2028 ) || \
                                      ( (unsigned short) c == 0x2029 )) ? 1 : 0 )

// It is legal to insert a line-break BEFORE any of these ASCII characters:
#define ASCII_WRAPPABLE_BEFORE( c ) ((( NEWLINE_CHAR( c )) || ( c==0x9 ) || ( c==0x20 )) ? 1 : 0 )

// It is legal to insert a line-break AFTER any of these ASCII characters:
#define ASCII_WRAPPABLE_AFTER( c )  ((( NEWLINE_CHAR( c )) || ( c==0x20 )) ? 1 : 0 )

// It is legal to insert a line-break BEFORE any of these UCS-2 characters
// (at least prior to taking UCS_WRAP_FORBIDDEN into account):
#define UCS_WRAPPABLE_BEFORE( c )   ((( ASCII_WRAPPABLE_BEFORE( c ))     || \
                                      ( c == 0x200B ) || ( c == 0x2013 ) || \
                                      ( c == 0x2014 ) || ( c == 0x2015 ) || \
                                      (( c > 0x2E79 ) && ( c < 0x3001 )) || \
                                      ( c == 0x3003 ) || ( c == 0x3004 ) || \
                                      (( c > 0x3005 ) && ( c < 0x3009 )) || \
                                      ( c == 0x300A ) || ( c == 0x300C ) || \
                                      ( c == 0x300E ) || ( c == 0x3010 ) || \
                                      ( c == 0x3014 ) || ( c == 0x3016 ) || \
                                      ( c == 0x3018 ) || ( c == 0x301A ) || \
                                      ( c == 0x301D ) || ( c == 0x301E ) || \
                                      (( c > 0x3020 ) && ( c < 0x3041 )) || \
                                      ( c == 0x3042 ) || ( c == 0x3044 ) || \
                                      ( c == 0x3046 ) || ( c == 0x3048 ) || \
                                      (( c > 0x3049 ) && ( c < 0x3063 )) || \
                                      (( c > 0x3063 ) && ( c < 0x3083 )) || \
                                      ( c == 0x3084 ) || ( c == 0x3086 ) || \
                                      (( c > 0x3087 ) && ( c < 0x3099 )) || \
                                      ( c == 0x309D ) || ( c == 0x309E ) || \
                                      ( c == 0x30A2 ) || ( c == 0x30A4 ) || \
                                      ( c == 0x30A6 ) || ( c == 0x30A8 ) || \
                                      (( c > 0x30A9 ) && ( c < 0x30C3 )) || \
                                      (( c > 0x30C3 ) && ( c < 0x30E3 )) || \
                                      ( c == 0x30E4 ) || ( c == 0x30E6 ) || \
                                      (( c > 0x30E7 ) && ( c < 0x30F5 )) || \
                                      ( c == 0x30F7 ) || ( c == 0x30FA ) || \
                                      ( c == 0x30FD ) || ( c == 0x30FE ) || \
                                      (( c > 0x30FF ) && ( c < 0xA4D0 )) || \
                                      ( c == 0xFF62 ) || ( c == 0xFF66 ) || \
                                      (( c > 0xFF71 ) && ( c < 0xFF9E )) || \
                                      (( c > 0xFF9F ) && ( c < 0xFFE0 ))) ? 1 : 0 )

// It is legal to insert a line-break AFTER any of these UCS-2 characters:
#define UCS_WRAPPABLE_AFTER( c )    ((( ASCII_WRAPPABLE_AFTER( c ))         || \
                                      ( c == 0x00AD )  ||  ( c == 0x200B ) || \
                                      ( c == 0x2010 )  ||  ( c == 0x2013 ) || \
                                      ( c == 0x2014 )  ||  ( c >= 0xFFF0 ) || \
                                      (( c >= 0x2E80 ) && ( c <= 0xA4CF )) || \
                                      (( c >= 0xAC00 ) && ( c <= 0xD7AF )) || \
                                      (( c >= 0xFE30 ) && ( c <= 0xFE6F ))) ? 1 : 0 )

// It is ILLEGAL to insert a line-break BEFORE any of these UCS-2 characters:
#define UCS_WRAP_FORBIDDEN( c )     ((( c == 0x3001 ) || ( c == 0x3002 ) || \
                                      ( c == 0x3005 ) || ( c == 0x3007 ) || \
                                      ( c == 0x3009 ) || ( c == 0x300B ) || \
                                      ( c == 0x300D ) || ( c == 0x300F ) || \
                                      ( c == 0x3011 ) || ( c == 0x3015 ) || \
                                      ( c == 0x3017 ) || ( c == 0x3019 ) || \
                                      ( c == 0x301B ) || ( c == 0x301C ) || \
                                      ( c == 0x3041 ) || ( c == 0x3043 ) || \
                                      ( c == 0x3045 ) || ( c == 0x3047 ) || \
                                      ( c == 0x3049 ) || ( c == 0x3063 ) || \
                                      ( c == 0x3083 ) || ( c == 0x3085 ) || \
                                      ( c == 0x3087 )                    || \
                                      (( c > 0x3098 ) && ( c < 0x309D )) || \
                                      ( c == 0x30A1 ) || ( c == 0x30A3 ) || \
                                      ( c == 0x30A5 ) || ( c == 0x30A7 ) || \
                                      ( c == 0x30A9 ) || ( c == 0x30C3 ) || \
                                      ( c == 0x30E3 ) || ( c == 0x30E5 ) || \
                                      ( c == 0x30E7 ) || ( c == 0x30FB ) || \
                                      ( c == 0x30FC ) || ( c == 0xFF61 ) || \
                                      (( c > 0xFF62 ) && ( c < 0xFF66 )) || \
                                      (( c > 0xFF66 ) && ( c < 0xFF71 )) || \
                                      ( c == 0xFF9E ) || ( c == 0xFF9F ) || \
                                      ( c > 0xFFDF )) ? 1 : 0 )


// Do not render these characters if they occur at the end of a wrapped line:
#define SKIP_WRAPPED_CHAR( c )      ((( NEWLINE_CHAR( c )) || ( c==0x20 )) ? 1 : 0 )


// ----------------------------------------------------------------------------
// Character Boundaries / String Parsing
//

#define UTF8_IS_FOLLOWING( b )   (( b & 0xB0 == 0x80 ) ? 1 : 0 )
#define UTF8_IS_LEADING( b )     (( b >= 0xC2 ) ? 1 : 0 )
#define UTF8_IS_2BYTELEAD( b )   ((( b >= 0xC2 ) && ( b <= 0xDF )) ? 1 : 0 )
#define UTF8_IS_3BYTELEAD( b )   ((( b >= 0xE0 ) && ( b <= 0xEF )) ? 1 : 0 )
#define UTF8_IS_4BYTELEAD( b )   (( b >= 0xF0 ) ? 1 : 0 )

#define UPF8_IS_SINGLE( b )      (( b < 0x80 ) ? 1 : 0 )
#define UPF8_IS_2BYTELEAD( b )   (((( b > 0x80 ) && ( b < 0xEC )) || ( b > 0xEF )) ? 1 : 0 )
#define UPF8_IS_3BYTELEAD( b )   ((( b > 0xEB ) && ( b < 0xF0 )) ? 1 : 0 )


// ----------------------------------------------------------------------------
// Conversions
//

// Convert an offset in a UCS-2 string from bytes to character positions
#define BYTEOFF_TO_UPOS( b )        (( b ) / 2 )

// Convert an offset in a UCS-2 string from character positions to bytes
#define UPOS_TO_BYTEOFF( u )        (( u ) * 2 )

// Convert a pair of bytes to a UniChar
#define BYTES2UNICHAR( b1, b2 )     ((( b1 ) << 8 ) | ( b2 ))



// ***************************************************************************
// FUNCTIONS
//

void   DumpUnicodeString( FILE *output, UniChar *psu, ULONG ulLength );
void   DumpUnicodeIncrements( FILE *output, UniChar *psu, LONG *alInc, ULONG ulLength );
ULONG  FindBreakPosition( PCH pchText, ULONG ulMax, USHORT usCP );
BOOL   IsDBCSLeadByte( CHAR ch, PBYTE pDBCS );
USHORT NextCharSize( PCHAR pchText, ULONG cbStart, ULONG cbTotal, ULONG ulCP, PBYTE pDBCS );
ULONG  NextLineBreak( PCHAR pchText, ULONG cbText, USHORT usCP );


