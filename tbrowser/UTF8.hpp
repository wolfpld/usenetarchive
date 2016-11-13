#ifndef __UTF8_HPP__
#define __UTF8_HPP__

#include <stdlib.h>
#include <curses.h>

size_t utflen( const char* str );
const char* utfend( const char* str, int len );
const char* utfendl( const char* str, int& len );
const char* utfendcrlf( const char* str, int len );

void utfprint( WINDOW* win, const char* str );
void utfprint( WINDOW* win, const char* str, const char* end );

#endif
