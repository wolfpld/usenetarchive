#include "View.hpp"

static void AdaptWidthHeight( int& w, int& h )
{
    if( w <= 0 )
    {
        w += COLS;
    }
    if( h <= 0 )
    {
        h += LINES;
    }
}

View::View( int x, int y, int w, int h )
{
    AdaptWidthHeight( w, h );
    m_win = newwin( h, w, y, x );
    keypad( m_win, TRUE );
}

View::~View()
{
    delwin( m_win );
}

int View::GetKey() const
{
    return wgetch( m_win );
}

void View::ResizeView( int x, int y, int w, int h ) const
{
    AdaptWidthHeight( w, h );
    mvwin( m_win, y, x );
    wresize( m_win, h, w );
}
