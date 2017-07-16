#ifndef __UTF8_HPP__
#define __UTF8_HPP__

#include <assert.h>
#include <stdlib.h>
#include <curses.h>

inline bool iscontinuationbyte( char c )
{
    return ( c & 0xC0 ) == 0x80;
}

inline int codepointlen( char c )
{
    if( ( c & 0x80 ) == 0 ) return 1;
    if( ( c & 0x20 ) == 0 ) return 2;
    if( ( c & 0x10 ) == 0 ) return 3;
    assert( ( c & 0x08 ) == 0 );
    return 4;
}

size_t utflen( const char* str );
size_t utflen( const char* str, const char* end );
size_t utflen_relaxed( const char* str, const char* end );
const char* utfend( const char* str, int len );
const char* utfendl( const char* str, int& len );
const char* utfendcrlf( const char* str, int len );
const char* utfendcrlfl( const char* str, int& len );

void utfprint( WINDOW* win, const char* str );
void utfprint( WINDOW* win, const char* str, const char* end );

#endif
