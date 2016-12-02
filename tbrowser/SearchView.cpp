#include "../libuat/Archive.hpp"
#include "../libuat/PersistentStorage.hpp"

#include "BottomBar.hpp"
#include "Browser.hpp"
#include "SearchView.hpp"
#include "UTF8.hpp"

SearchView::SearchView( Browser* parent, BottomBar& bar, Archive& archive, PersistentStorage& storage )
    : View( 0, 1, 0, -2 )
    , m_parent( parent )
    , m_bar( bar )
    , m_archive( archive )
    , m_storage( storage )
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
        wprintw( m_win, " %i", m_result.size() );
        wattroff( m_win, COLOR_PAIR( 2 ) | A_BOLD );
        wprintw( m_win, " results for query: " );
        wattron( m_win, A_BOLD );
        wprintw( m_win, "%s", m_query.c_str() );
        wattroff( m_win, A_BOLD );

        int cnt = 0;
        int h = getmaxy( m_win ) - 1;
        int w = getmaxx( m_win );
        while( h > 0 && cnt < m_result.size() )
        {
            const auto& res = m_result[cnt];
            wmove( m_win, cnt + 1, 0 );
            wprintw( m_win, "%5i ", cnt );
            wattron( m_win, COLOR_PAIR(8) | A_BOLD );
            wprintw( m_win, "(%5.1f%%) ", res.rank * 100.f );
            wattroff( m_win, COLOR_PAIR(8) | A_BOLD );

            if( m_storage.WasVisited( m_archive.GetMessageId( res.postid ) ) )
            {
                waddch( m_win, 'R' );
            }
            else
            {
                waddch( m_win, '-' );
            }

            wprintw( m_win, " [" );
            auto realname = m_archive.GetRealName( res.postid );
            wattron( m_win, COLOR_PAIR(3) | A_BOLD );
            int len = 16;
            auto end = utfendl( realname, len );
            utfprint( m_win, realname, end );
            if( len < 16 )
            {
                wmove( m_win, cnt + 1, 34 );
            }
            wattroff( m_win, COLOR_PAIR(3) | A_BOLD );
            wprintw( m_win, "] " );

            time_t date = m_archive.GetDate( res.postid );
            auto lt = localtime( &date );
            char buf[64];
            auto dlen = strftime( buf, 64, "%F %R", lt );

            auto subject = m_archive.GetSubject( res.postid );
            len = w - 38 - dlen;
            end = utfendl( subject, len );
            utfprint( m_win, subject, end );

            wmove( m_win, cnt+1, w-dlen-2 );
            waddch( m_win, '[' );
            wattron( m_win, COLOR_PAIR(2) );
            wprintw( m_win, "%s", buf );
            wattroff( m_win, COLOR_PAIR(2) );
            waddch( m_win, ']' );

            cnt++;
            h--;
        }
    }
    wnoutrefresh( m_win );
}
