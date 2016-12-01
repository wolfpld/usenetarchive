#include "Browser.hpp"
#include "SearchView.hpp"

SearchView::SearchView( Browser* parent )
    : View( 0, 1, 0, -2 )
    , m_parent( parent )
    , m_active( false )
{
}

void SearchView::Entry()
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
