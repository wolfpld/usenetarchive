#include "SearchView.hpp"

SearchView::SearchView()
    : View( 0, 1, 0, -2 )
    , m_active( false )
{
}

void SearchView::Resize()
{
    if( !m_active ) return;
    ResizeView( 0, 1, 0, -2 );
    Draw();
}

void SearchView::Draw()
{
    wclear( m_win );
    wnoutrefresh( m_win );
}
