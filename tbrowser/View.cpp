#include "View.hpp"

View::View( int x, int y, int w, int h )
{
    if( w <= 0 )
    {
        w += COLS;
    }
    if( h <= 0 )
    {
        h += LINES;
    }
    m_win = newwin( h, w, y, x );
}

View::~View()
{
    delwin( m_win );
}

int View::GetKey()
{
    return wgetch( m_win );
}
