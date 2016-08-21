#include "MessageLogic.hpp"

int QuotationLevel( const char*& ptr, const char* end )
{
    int level = 0;

    while( ptr != end )
    {
        switch( *ptr )
        {
        case ':':
            if( ptr+1 != end && *(ptr+1) == ')' )
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
