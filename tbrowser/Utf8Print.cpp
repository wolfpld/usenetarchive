#include "../common/UTF8.hpp"
#include "Utf8Print.hpp"

void utfprint( WINDOW* win, const char* str )
{
    auto end = str;
    while( *end )
    {
        if( ( *end & 0x80 ) == 0 )
        {
            if( *end <= 0x1F || *end == 0x7F )
            {
                wprintw( win, "%.*s", int( end - str ), str );
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
        wprintw( win, "%.*s", int( end - str ), str );
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
                wprintw( win, "%.*s", int( end - str ), str );
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
        wprintw( win, "%.*s", int( end - str ), str );
    }
}
