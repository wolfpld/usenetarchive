#ifndef __STRING_HPP__
#define __STRING_HPP__

#include <algorithm>
#include <ctype.h>
#include <stdio.h>
#include <string>

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

static inline bool _isspace( char c )
{
    return c == ' ';
}

template <class T>
static inline void split( const char* s, T o )
{
    typedef const char* iter;

    iter i = s, j;
    iter e = s;
    while( *e ) e++;

    while( i != e )
    {
        i = std::find_if( i, e, []( char c ){ return !_isspace( c ); } );
        j = std::find_if( i, e, []( char c ){ return _isspace( c ); } );

        if( i != e )
        {
            *o++ = std::string( i, j );
        }

        i = j;
    }
}

static inline bool ReadLine( FILE* f, std::string& out )
{
    out.clear();

    for(;;)
    {
        char c;
        const size_t read = fread( &c, 1, 1, f );
        if( read == 0 || c == '\n' )
        {
            return read > 0 || !out.empty();
        }
        if( c != '\r' )
        {
            out += c;
        }
    }
}

#endif
