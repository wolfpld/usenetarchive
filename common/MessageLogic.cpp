#include "MessageLogic.hpp"

int QuotationLevel( const char*& ptr, const char* end )
{
    int level = 0;

    while( ptr != end && ( *ptr == ' ' || *ptr == '>' || *ptr == ':' || *ptr == '|' || *ptr == '\t' ) )
    {
        if( *ptr == '>' || *ptr == ':' || *ptr == '|' )
        {
            level++;
        }
        ptr++;
    }

    return level;
}
