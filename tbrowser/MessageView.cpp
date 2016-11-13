#include "../libuat/Archive.hpp"

#include "MessageView.hpp"

MessageView::MessageView( const Archive& archive )
    : View( 0, 0, 1, 1 )
    , m_archive( archive )
    , m_idx( -1 )
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
    Draw();
}

void MessageView::Display( uint32_t idx )
{
    m_idx = idx;
    // If view is not active, drawing will be performed during resize.
    if( m_active )
    {
        Draw();
    }
    m_active = true;
}

void MessageView::Close()
{
    m_active = false;
}

void MessageView::Draw()
{
    werase( m_win );
    wprintw( m_win, "%i\n", m_idx );
    wnoutrefresh( m_win );
}
