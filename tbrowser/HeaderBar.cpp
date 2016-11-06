#include "HeaderBar.hpp"

HeaderBar::HeaderBar( const char* archive, const char* desc )
    : View( 0, 0, -1, 1 )
{
    wbkgd( m_win, COLOR_PAIR(1) );
    wprintw( m_win, " Usenet Archive" );
    wattron( m_win, A_BOLD );
    wprintw( m_win, " :: " );
    wattroff( m_win, A_BOLD );

    if( desc )
    {
        while( *desc )
        {
            if( *desc != '\n' && *desc != '\r' )
            {
                waddch( m_win, *desc );
            }
            desc++;
        }

        wattron( m_win, A_BOLD );
        wprintw( m_win, " :: " );
        wattroff( m_win, A_BOLD );
    }
    wprintw( m_win, "%s", archive );

    wrefresh( m_win );
}
