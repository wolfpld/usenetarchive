#include <stdlib.h>

#include "BottomBar.hpp"

BottomBar::BottomBar( int total )
    : View( 0, LINES-1, 0, 1 )
    , m_total( total )
{
    wbkgd( m_win, COLOR_PAIR(1) );
    wrefresh( m_win );
}

void BottomBar::Resize( int index )
{
    ResizeView( 0, LINES-1, 0, 1 );
    werase( m_win );
    Update( index );
}

void BottomBar::Update( int index )
{
    werase( m_win );
    wprintw( m_win, " [%i/%i]", index, m_total );
    wnoutrefresh( m_win );
}
