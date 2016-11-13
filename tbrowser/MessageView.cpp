#include "../libuat/Archive.hpp"

#include "MessageView.hpp"

MessageView::MessageView( const Archive& archive )
    : View( 0, 0, 1, 1 )
    , m_archive( archive )
    , m_active( false )
{
}

MessageView::~MessageView()
{
}

void MessageView::Resize()
{
    if( !m_active ) return;
    int sw = getmaxx( stdscr );
    m_vertical = sw > 160;
    if( m_vertical )
    {
        ResizeView( sw / 2, 1, (sw+1) / 2, -2 );
    }
    else
    {
        int sh = getmaxy( stdscr ) - 2;
        ResizeView( 0, 1 + sh * 20 / 100, 0, sh - ( sh * 20 / 100 ) );
    }
    werase( m_win );
    wnoutrefresh( m_win );
}

void MessageView::SetActive( bool active )
{
    m_active = active;
}
