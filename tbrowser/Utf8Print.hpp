#ifndef __UTF8PRINT_HPP__
#define __UTF8PRINT_HPP__

#include <curses.h>

void utfprint( WINDOW* win, const char* str );
void utfprint( WINDOW* win, const char* str, const char* end );

#endif
