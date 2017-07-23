#ifndef __MESSAGELOGIC_HPP__
#define __MESSAGELOGIC_HPP__

int QuotationLevel( const char*& ptr, const char* end );
const char* NextQuotationLevel( const char* ptr );

// returns pointer to header, or '\n' if not found
const char* FindOptionalHeader( const char* msg, const char* header, int hlen );
const char* FindHeader( const char* msg, const char* header, int hlen );

const char* FindReferences( const char* msg );
int ValidateReferences( const char*& buf );
bool ValidateMsgId( const char* begin, const char* end, char* dst );
bool IsMsgId( const char* begin, const char* end );

int DetectWrote( const char* ptr );
const char* DetectWroteEnd( const char* ptr, int baseLevel );

#endif
