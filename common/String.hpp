#ifndef __STRING_HPP__
#define __STRING_HPP__

#include <ctype.h>

static inline int strnicmpl( const char* l, const char* r, int n )
{
    while( n-- )
    {
        if( tolower( *l ) != *r ) return 1;
        else if( *l == '\0' ) return 0;
        l++; r++;
    }
    return 0;
}

#endif
