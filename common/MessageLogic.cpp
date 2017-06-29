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

const char* FindReferences( const char* msg )
{
    auto buf = FindOptionalHeader( msg, "references: ", 12 );
    if( *buf != '\n' ) return buf + 12;
    buf = FindOptionalHeader( msg, "in-reply-to: ", 13 );
    if( *buf != '\n' ) return buf + 13;
    return buf;
}

int ValidateReferences( const char*& buf )
{
    int valid = 0;
    while( *buf != '\n' )
    {
        if( *buf == '<' ) valid++;
        else if( *buf == '>' ) valid--;
        buf++;
    }
    return valid;
}

bool ValidateMsgId( const char* begin, const char* end, char* dst )
{
    bool broken = false;
    while( begin != end )
    {
        if( *begin != ' ' && *begin != '\t' )
        {
            *dst++ = *begin;
        }
        else
        {
            broken = true;
        }
        begin++;
    }
    *dst++ = '\0';
    return broken;
}

// Returns number of lines in "wrote" context
//   { content, quote } -> one line of context
//   { content, content with "wrote:", quote } -> two lines of context
//   anything else -> zero lines of context
int DetectWrote( const char* ptr )
{
    auto end = ptr;
    while( *end != '\n' && *end != '\0' ) end++;
    if( *end == '\0' ) return 0;
    if( QuotationLevel( ptr, end ) != 0 ) return 0;
    while( *end == '\n' ) end++;
    ptr = end;
    while( *end != '\n' && *end != '\0' ) end++;
    if( *end == '\0' ) return 0;
    if( QuotationLevel( ptr, end ) != 0 ) return 1;
    bool found = false;
    while( ptr < end - 6 )
    {
        if( strncmp( ptr++, "wrote:", 6 ) == 0 )
        {
            found = true;
            break;
        }
    }
    if( !found ) return 0;
    while( *end == '\n' ) end++;
    ptr = end;
    while( *end != '\n' && *end != '\0' ) end++;
    if( *end == '\0' ) return 0;
    if( QuotationLevel( ptr, end ) != 0 ) return 2;
    return 0;
}
