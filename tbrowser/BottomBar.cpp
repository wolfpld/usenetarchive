#include <stdlib.h>

#include "BottomBar.hpp"

BottomBar::BottomBar()
    : View( 0, LINES-1, 0, 1 )
{
    PrintHelp();
    wnoutrefresh( m_win );
}

void BottomBar::Resize() const
{
    ResizeView( 0, LINES-1, 0, 1 );
    werase( m_win );
    PrintHelp();
    wnoutrefresh( m_win );
}

void BottomBar::PrintHelp() const
{
    waddch( m_win, ACS_DARROW );
    waddch( m_win, ACS_UARROW );
    wprintw( m_win, ":Move " );
    waddch( m_win, ACS_RARROW );
    wprintw( m_win, ":Exp " );
    waddch( m_win, ACS_LARROW );
    wprintw( m_win, ":Coll " );
    wprintw( m_win, "x:Co/Ex e:CoAll q:Quit RET:+Ln BCK:-Ln SPC:+Pg d:MrkRd ,:Bck .:Fwd t:Hdrs r:R13" );
}
