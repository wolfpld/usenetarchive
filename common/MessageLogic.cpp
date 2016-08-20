#include "MessageLogic.hpp"

int QuotationLevel( const char*& ptr, const char* end )
{
    int level = 0;

    while( ptr != end )
    {
        switch( *ptr )
        {
        case '>':
        case ':':
        case '|':
            level++;
            //fallthrough:
        case ' ':
        case '\t':
            ptr++;
            break;
        default:
            return level;
        }
    }
    return level;
}
