#include "BottomBar.hpp"
#include "Browser.hpp"
#include "SearchView.hpp"

SearchView::SearchView( Browser* parent, BottomBar& bar, Archive& archive )
    : View( 0, 1, 0, -2 )
    , m_parent( parent )
    , m_bar( bar )
    , m_archive( archive )
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
                std::swap( m_query, query );
                m_result = m_archive.Search( m_query.c_str() );
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
    if( m_result.empty() )
    {
        if( m_query.empty() )
        {
            mvwprintw( m_win, 2, 4, "Nothing to show." );
        }
        else
        {
            mvwprintw( m_win, 2, 4, "No results for: %s", m_query.c_str() );
        }
        mvwprintw( m_win, 4, 4, "Press '/' to enter search query." );
    }
    else
    {
        wattron( m_win, COLOR_PAIR( 2 ) | A_BOLD );
        wprintw( m_win, "%i", m_result.size() );
        wattroff( m_win, COLOR_PAIR( 2 ) | A_BOLD );
        wprintw( m_win, " results for query: " );
        wattron( m_win, A_BOLD );
        wprintw( m_win, "%s", m_query.c_str() );
        wattroff( m_win, A_BOLD );
    }
    wnoutrefresh( m_win );
}
