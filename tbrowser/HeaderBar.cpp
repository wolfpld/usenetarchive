#include <stdlib.h>
#include <string.h>

#include "HeaderBar.hpp"
#include "UTF8.hpp"

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

        int w = getmaxx( m_win ) - 23 - strlen( m_archive );
        auto end = utfendcrlf( m_desc, w );
        wprintw( m_win, "%.*s", end - m_desc, m_desc );
    }

    wnoutrefresh( m_win );
}
