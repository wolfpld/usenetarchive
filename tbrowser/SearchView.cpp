#include <algorithm>
#include <assert.h>
#include <ctype.h>
#include <sstream>
#include <vector>

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
    , m_top( 0 )
    , m_bottom( 0 )
    , m_cursor( 0 )
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
                m_preview.clear();
                m_preview.reserve( m_result.size() );
                m_top = m_bottom = m_cursor = 0;
            }
            else
            {
                m_bar.Update();
            }
            Draw();
            doupdate();
            break;
        }
        case KEY_DOWN:
            MoveCursor( 1 );
            doupdate();
            break;
        case KEY_UP:
            MoveCursor( -1 );
            doupdate();
            break;
        case KEY_NPAGE:
            MoveCursor( m_bottom - m_top );
            doupdate();
            break;
        case KEY_PPAGE:
            MoveCursor( m_top - m_bottom );
            break;
        case KEY_ENTER:
        case '\n':
        case 459:   // numpad enter
            if( !m_result.empty() )
            {
                m_parent->OpenMessage( m_result[m_cursor].postid );
                return;
            }
            break;
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

        const int h = getmaxy( m_win ) - 1;
        const int w = getmaxx( m_win );
        int cnt = m_top;
        int line = 0;
        while( line < h && cnt < m_result.size() )
        {
            const auto& res = m_result[cnt];
            wmove( m_win, line + 1, 0 );
            wattron( m_win, COLOR_PAIR(1) );
            wprintw( m_win, "%5i ", cnt+1 );
            wattron( m_win, COLOR_PAIR(15) );
            wprintw( m_win, "(%5.1f%%) ", res.rank * 100.f );

            if( m_cursor == cnt )
            {
                wmove( m_win, line + 1, 0 );
                wattron( m_win, COLOR_PAIR(14) | A_BOLD );
                wprintw( m_win, "->" );
                wattroff( m_win, COLOR_PAIR(14) | A_BOLD );
                wmove( m_win, line + 1, 15 );
            }

            wattron( m_win, COLOR_PAIR(1) );
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
            wattron( m_win, COLOR_PAIR(13) | A_BOLD );
            int len = 16;
            auto end = utfendl( realname, len );
            utfprint( m_win, realname, end );
            while( len++ < 16 )
            {
                waddch( m_win, ' ' );
            }
            wattroff( m_win, COLOR_PAIR(3) | A_BOLD );
            wattron( m_win, COLOR_PAIR(1) );
            wprintw( m_win, "] " );

            time_t date = m_archive.GetDate( res.postid );
            auto lt = localtime( &date );
            char buf[64];
            auto dlen = strftime( buf, 64, "%F %R", lt );

            auto subject = m_archive.GetSubject( res.postid );
            len = w - 38 - dlen;
            end = utfendl( subject, len );
            wattron( m_win, A_BOLD );
            utfprint( m_win, subject, end );
            wattroff( m_win, A_BOLD );

            while( len++ < w - 38 - dlen )
            {
                waddch( m_win, ' ' );
            }

            wmove( m_win, line+1, w-dlen-2 );
            waddch( m_win, '[' );
            wattron( m_win, COLOR_PAIR(14) | A_BOLD );
            wprintw( m_win, "%s", buf );
            wattroff( m_win, COLOR_PAIR(14) | A_BOLD );
            wattron( m_win, COLOR_PAIR(1) );
            waddch( m_win, m_cursor == cnt ? '<' : ']' );
            wattroff( m_win, COLOR_PAIR(1) );

            assert( cnt <= m_preview.size() );
            if( m_preview.size() == cnt )
            {
                FillPreview( cnt );
            }
            wmove( m_win, line+2, 0 );
            len = w * std::min( 3, h - line - 1 );
            const char* preview = m_preview[cnt].c_str();
            end = utfendl( preview, len );
            utfprint( m_win, preview, end );

            cnt++;
            line += 4;
        }
        m_bottom = cnt;
    }
    wnoutrefresh( m_win );
}

void SearchView::FillPreview( int idx )
{
    auto msg = std::string( m_archive.GetMessage( m_result[idx].postid ) );
    auto lower = msg;
    std::transform( lower.begin(), lower.end(), lower.begin(), ::tolower );

    std::vector<size_t> tpos;
    for( auto& match : m_result[idx].matched )
    {
        size_t pos = 0;
        while( ( pos = lower.find( match, pos+1 ) ) != std::string::npos ) tpos.emplace_back( pos );
    }
    std::sort( tpos.begin(), tpos.end() );

    std::vector<std::pair<size_t, size_t>> ranges;
    int stop = std::min<int>( 6, tpos.size() );
    for( int i=0; i<stop; i++ )
    {
        size_t start = tpos[i];
        for( int j=0; j<10; j++ )
        {
            if( start == 0 ) break;
            start--;
            while( start > 0 && msg[start] != ' ' && msg[start] != '\n' ) start--;
        }
        size_t end = tpos[i];
        for( int j=0; j<10; j++ )
        {
            if( end == msg.size() ) break;
            end++;
            while( end < msg.size() && msg[end] != ' ' && msg[end] != '\n' ) end++;
        }
        ranges.emplace_back( start, end );
    }

    for( int i=ranges.size()-1; i>0; i-- )
    {
        if( ranges[i].first <= ranges[i-1].second )
        {
            ranges[i-1].second = ranges[i].second;
            ranges.erase( ranges.begin() + i );
        }
    }

    std::ostringstream s;
    for( auto& v : ranges )
    {
        s << " ...";
        for( size_t i = v.first; i<v.second; i++ )
        {
            switch( msg[i] )
            {
            case '\n':
                s.put( ' ' );
                break;
            default:
                s.put( msg[i] );
                break;
            }
        }
    }
    s << " ...";

    m_preview.emplace_back( s.str() );
}

void SearchView::MoveCursor( int offset )
{
    while( offset < 0 )
    {
        if( m_cursor == 0 ) break;
        m_cursor--;
        if( m_cursor < m_top )
        {
            m_top--;
            m_bottom--;
        }
        offset++;
    }
    while( offset > 0 )
    {
        if( m_cursor == m_result.size() - 1 ) break;
        m_cursor++;
        if( m_cursor >= m_bottom )
        {
            m_top++;
            m_bottom++;
        }
        offset--;
    }
    Draw();
}
