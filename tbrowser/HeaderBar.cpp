#include <stdlib.h>

#include "HeaderBar.hpp"

HeaderBar::HeaderBar( const char* archive, const char* desc )
    : View( 0, 0, 0, 1 )
    , m_archive( archive )
    , m_desc( desc )
{
    wbkgd( m_win, COLOR_PAIR(1) );
    Redraw();
}

void HeaderBar::Resize()
{
    ResizeView( 0, 0, 0, 1 );
    wclear( m_win );
    Redraw();
}

void HeaderBar::Redraw()
{
    wprintw( m_win, " Usenet Archive" );
    wattron( m_win, A_BOLD );
    wprintw( m_win, " :: " );
    wattroff( m_win, A_BOLD );

    wprintw( m_win, "%s", m_archive );

    if( m_desc )
    {
        wattron( m_win, A_BOLD );
        wprintw( m_win, " :: " );
        wattroff( m_win, A_BOLD );

        wchar_t buf[1024];
        mbstowcs( buf, m_desc, 1024 );
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

    wnoutrefresh( m_win );
}
