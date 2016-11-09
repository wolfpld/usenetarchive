#include <assert.h>

#include "UTF8.hpp"

static int codepointlen( char c )
{
    if( c & 0x80 == 0 ) return 1;
    if( c & 0x20 == 0 ) return 2;
    if( c & 0x10 == 0 ) return 3;
    assert( c & 0x08 == 0 );
    return 4;
}

size_t utflen( const char* str )
{
    size_t ret = 0;
    while( *str != '\0' )
    {
        str += codepointlen( *str );
        ret++;
    }
    return ret;
}

const char* utfend( const char* str, size_t& len )
{
    size_t l = 0;
    while( l < len && *str != '\0' )
    {
        str += codepointlen( *str );
        l++;
    }
    len = l;
    return str;
}
