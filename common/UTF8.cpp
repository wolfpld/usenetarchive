#include "UTF8.hpp"

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

size_t utflen( const char* str, const char* end )
{
    size_t ret = 0;
    while( *str != '\0' && str < end )
    {
        str += codepointlen( *str );
        ret++;
    }
    assert( *str == '\0' || str == end );
    return ret;
}

size_t utflen_relaxed( const char* str, const char* end )
{
    size_t ret = 0;
    while( *str != '\0' && str < end )
    {
        auto len = codepointlen( *str );
        str++;
        len--;
        while( len-- > 0 )
        {
            if( !iscontinuationbyte( *str++ ) || str > end )
            {
                return ret;
            }
        }
        ret++;
    }
    return ret;
}

const char* utfend( const char* str, int len )
{
    while( len-- > 0 && *str != '\0' )
    {
        str += codepointlen( *str );
    }
    return str;
}

const char* utfendl( const char* str, int& len )
{
    int l = 0;
    while( l < len && *str != '\0' )
    {
        str += codepointlen( *str );
        l++;
    }
    len = l;
    return str;
}

const char* utfendcrlf( const char* str, int len )
{
    while( len-- > 0 && *str != '\0' && *str != '\n' && *str != '\r' )
    {
        str += codepointlen( *str );
    }
    return str;
}

const char* utfendcrlfl( const char* str, int& len )
{
    int l = 0;
    while( l < len && *str != '\0' && *str != '\n' && *str != '\r' )
    {
        str += codepointlen( *str );
        l++;
    }
    len = l;
    return str;
}
