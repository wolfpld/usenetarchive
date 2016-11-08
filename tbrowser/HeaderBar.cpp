#include <stdlib.h>

#include "HeaderBar.hpp"

HeaderBar::HeaderBar( const char* archive, const char* desc )
    : View( 0, 0, 0, 1 )
{
    wbkgd( m_win, COLOR_PAIR(1) );
    wprintw( m_win, " Usenet Archive" );
    wattron( m_win, A_BOLD );
    wprintw( m_win, " :: " );
    wattroff( m_win, A_BOLD );

    wprintw( m_win, "%s", archive );

    if( desc )
    {
        wattron( m_win, A_BOLD );
        wprintw( m_win, " :: " );
        wattroff( m_win, A_BOLD );

        wchar_t buf[1024];
        mbstowcs( buf, desc, 1024 );
        auto ptr = buf;

        while( *ptr )
        {
            if( *ptr != '\n' && *ptr != '\r' )
            {
                waddch( m_win, *ptr );
            }
            ptr++;
        }
    }

    wrefresh( m_win );
}
