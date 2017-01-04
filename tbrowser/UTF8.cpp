#include <assert.h>

#include "UTF8.hpp"

bool iscontinuationbyte( char c )
{
    return ( c & 0xC0 ) == 0x80;
}

int codepointlen( char c )
{
    if( ( c & 0x80 ) == 0 ) return 1;
    if( ( c & 0x20 ) == 0 ) return 2;
    if( ( c & 0x10 ) == 0 ) return 3;
    assert( ( c & 0x08 ) == 0 );
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

void utfprint( WINDOW* win, const char* str )
{
    auto end = str;
    while( *end )
    {
        if( ( *end & 0x80 ) == 0 )
        {
            if( *end <= 0x1F || *end == 0x7F )
            {
                wprintw( win, "%.*s", end - str, str );
                waddch( win, ' ' );
                end++;
                str = end;
            }
            else
            {
                end++;
            }
        }
        else
        {
            end += codepointlen( *end );
        }
    }
    if( end != str )
    {
        wprintw( win, "%.*s", end - str, str );
    }
}

void utfprint( WINDOW* win, const char* str, const char* _end )
{
    auto end = str;
    while( *end && end != _end )
    {
        if( ( *end & 0x80 ) == 0 )
        {
            if( *end <= 0x1F || *end == 0x7F )
            {
                wprintw( win, "%.*s", end - str, str );
                waddch( win, ' ' );
                end++;
                str = end;
            }
            else
            {
                end++;
            }
        }
        else
        {
            end += codepointlen( *end );
        }
    }
    if( end != str )
    {
        wprintw( win, "%.*s", end - str, str );
    }
}
