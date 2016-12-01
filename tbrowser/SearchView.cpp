#include "BottomBar.hpp"
#include "Browser.hpp"
#include "SearchView.hpp"

SearchView::SearchView( Browser* parent, BottomBar& bar )
    : View( 0, 1, 0, -2 )
    , m_parent( parent )
    , m_bar( bar )
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
        case 's':
        case '/':
        {
            auto query = m_bar.Query( "Search: ", m_query.c_str() );
            if( !query.empty() )
            {
                m_query = query;
            }
            else
            {
                m_bar.Update();
            }
            Draw();
            doupdate();
            break;
        }
        default:
            break;
        }
        m_bar.Update();
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
    if( m_query.empty() )
    {
        mvwprintw( m_win, 2, 4, "Nothing to show." );
    }
    else
    {
        mvwprintw( m_win, 2, 4, "No results for: %s", m_query.c_str() );
    }
    mvwprintw( m_win, 4, 4, "Press '/' to enter search query." );
    wnoutrefresh( m_win );
}
