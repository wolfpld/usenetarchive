#ifndef __MESSAGELOGIC_HPP__
#define __MESSAGELOGIC_HPP__

class KillRe;

int QuotationLevel( const char*& ptr, const char* end );
const char* NextQuotationLevel( const char* ptr );

// returns pointer to header, or '\n' if not found
const char* FindOptionalHeader( const char* msg, const char* header, int hlen );
const char* FindHeader( const char* msg, const char* header, int hlen );

bool IsMsgId( const char* begin, const char* end );
bool IsSubjectMatch( const char* s1, const char* s2, const KillRe& kill );

int DetectWrote( const char* ptr );
const char* DetectWroteEnd( const char* ptr, int baseLevel );

#endif
