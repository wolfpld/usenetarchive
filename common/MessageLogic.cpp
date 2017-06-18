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
