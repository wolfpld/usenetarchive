#include <string.h>
#include <utility>

#include "MessageLogic.hpp"
#include "String.hpp"

int QuotationLevel( const char*& ptr, const char* end )
{
    int level = 0;

    while( ptr != end )
    {
        switch( *ptr )
        {
        case ':':
            if( ( ptr+1 != end && *(ptr+1) == ')' ) || ( ptr+2 != end && *(ptr+1) == '-' && *(ptr+2) == ')' ) )
            {
                return level;
            }
            // fallthrough
        case '>':
        case '|':
            level++;
            // fallthrough
        case ' ':
        case '\t':
            ptr++;
            break;
        default:
            if( ( *ptr >= 'A' && *ptr <= 'Z' ) || ( *ptr >= 'a' && *ptr <= 'z' ) )
            {
                auto p = ptr + 1;
                while( p != end && ( ( *p >= 'A' && *p <= 'Z' ) || ( *p >= 'a' && *p <= 'z' ) ) ) p++;
                if( p == end || *p != '>' ) return level;
                ptr = p;
            }
            else
            {
                return level;
            }
        }
    }
    return level;
}

const char* NextQuotationLevel( const char* ptr )
{
    for(;;)
    {
        switch( *ptr )
        {
        case ':':
        case '>':
        case '|':
            return ptr;
        default:
            ptr++;
            break;
        }
    }
}

const char* FindOptionalHeader( const char* msg, const char* header, int hlen )
{
    while( strnicmpl( msg, header, hlen ) != 0 && *msg != '\n' )
    {
        msg++;
        while( *msg++ != '\n' ) {}
    }
    return msg;
}

const char* FindHeader( const char* msg, const char* header, int hlen )
{
    while( strnicmpl( msg, header, hlen ) != 0 )
    {
        msg++;
        while( *msg++ != '\n' ) {}
    }
    return msg;
}

// Note that this function doesn't perform strict validation. Many message-ids are broken,
// for example containing forbidden space character. UAT can handle that.
bool IsMsgId( const char* begin, const char* end )
{
    uint32_t u = 0, h = 0, a = 0;   // unique, host, at
    while( begin != end )
    {
        if( *begin < 32 || *begin > 126 || *begin == '<' || *begin == '>' )
        {
            return false;
        }
        if( *begin == '@' )
        {
            a++;
            std::swap( u, h );
        }
        else
        {
            h++;
        }
        begin++;
    }
    return a == 1 && u > 0 && h > 0;
}

// Returns number of lines in "wrote" context
int DetectWrote( const char* ptr )
{
    // First line must be T_Content
    while( *ptr == '\n' || *ptr == ' ' || *ptr == '\t' ) ptr++;
    auto end = ptr;
    while( *end != '\n' && *end != '\0' ) end++;
    if( *end == '\0' ) return 0;
    if( QuotationLevel( ptr, end ) != 0 ) return 0;

    // If second line is T_Quote -> wrote context
    while( *end == '\n' || *end == ' ' || *end == '\t' ) end++;
    ptr = end;
    while( *end != '\n' && *end != '\0' ) end++;
    if( *end == '\0' ) return 0;
    if( QuotationLevel( ptr, end ) != 0 ) return 1;

    // Check for typical "wrote:" markers
    auto e = end - 1;
    while( e > ptr && ( *e == ' ' || *e == '\t' ) ) e--;
    e++;
    if( e - ptr >= 1 )
    {
        if( *(e-1) == ':' ) goto found;     // "something something wrote:"
    }
    if( e - ptr >= 2 )
    {
        if( *ptr == '[' && *(e-1) == ']' ) goto found;      // "[cut content]"
        if( *ptr == '<' && *(e-1) == '>' ) goto found;      // "<cut content>"
    }
    if( e - ptr >= 3 )
    {
        if( strncmp( e - 3, "...", 3 ) == 0 ) goto found;   // "something something wrote..."
    }
    return 0;

found:
    // But only if next line is T_Quote
    while( *end == '\n' || *end == ' ' || *end == '\t' ) end++;
    ptr = end;
    while( *end != '\n' && *end != '\0' ) end++;
    if( *end == '\0' ) return 0;
    if( QuotationLevel( ptr, end ) != 0 ) return 2;
    return 0;
}

const char* DetectWroteEnd( const char* ptr, int baseLevel )
{
    const char* orig = ptr;

    // First line must be baseLevel
    while( *ptr == '\n' || *ptr == ' ' || *ptr == '\t' ) ptr++;
    auto end = ptr;
    while( *end != '\n' && *end != '\0' ) end++;
    if( *end == '\0' ) return orig;
    if( QuotationLevel( ptr, end ) != baseLevel ) return orig;

    // If second line is baseLevel+1 -> wrote context
    while( *end == '\n' || *end == ' ' || *end == '\t' ) end++;
    ptr = end;
    while( *end != '\n' && *end != '\0' ) end++;
    if( *end == '\0' ) return orig;
    if( QuotationLevel( ptr, end ) == baseLevel+1 ) return ptr;

    // Check for typical "wrote:" markers
    auto e = end - 1;
    while( e > ptr && ( *e == ' ' || *e == '\t' ) ) e--;
    e++;
    if( e - ptr >= 1 )
    {
        if( *(e-1) == ':' ) goto found;     // "something something wrote:"
    }
    if( e - ptr >= 2 )
    {
        if( *ptr == '[' && *(e-1) == ']' ) goto found;      // "[cut content]"
        if( *ptr == '<' && *(e-1) == '>' ) goto found;      // "<cut content>"
    }
    if( e - ptr >= 3 )
    {
        if( strncmp( e - 3, "...", 3 ) == 0 ) goto found;   // "something something wrote..."
    }
    return orig;

found:
    // But only if next line is baseLevel+1
    while( *end == '\n' || *end == ' ' || *end == '\t' ) end++;
    ptr = end;
    while( *end != '\n' && *end != '\0' ) end++;
    if( *end == '\0' ) return orig;
    if( QuotationLevel( ptr, end ) == baseLevel+1 ) return ptr;
    return orig;
}
