#include "View.hpp"

View::View( int x, int y, int w, int h )
{
    if( w == -1 )
    {
        w = COLS - x;
    }
    if( h == -1 )
    {
        h = LINES - y;
    }
    m_win = newwin( h, w, y, x );
}

View::~View()
{
    delwin( m_win );
}
