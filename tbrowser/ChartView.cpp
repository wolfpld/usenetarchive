#include "Browser.hpp"
#include "ChartView.hpp"

ChartView::ChartView( Browser* parent )
    : View( 0, 1, 0, -2 )
    , m_parent( parent )
    , m_active( false )
{
}

void ChartView::Entry()
{
    m_active = true;
    Draw();
    doupdate();

    while( auto key = GetKey() )
    {
        switch( key )
        {
        case KEY_RESIZE:
            m_parent->Resize();
            break;
        case 'q':
            m_active = false;
            return;
        default:
            break;
        }
    }
}

void ChartView::Resize()
{
    ResizeView( 0, 1, 0, -2 );
    if( !m_active ) return;
    Draw();
}

void ChartView::Draw()
{
    int w, h;
    getmaxyx( m_win, h, w );
    werase( m_win );
    wnoutrefresh( m_win );
}
