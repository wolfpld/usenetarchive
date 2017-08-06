#include <algorithm>
#include <assert.h>
#include <chrono>
#include <ctype.h>
#include <map>
#include <sstream>
#include <vector>

#include "../libuat/Archive.hpp"
#include "../libuat/PersistentStorage.hpp"
#include "../libuat/SearchEngine.hpp"
#include "../common/ICU.hpp"
#include "../common/MessageLogic.hpp"

#include "BottomBar.hpp"
#include "Browser.hpp"
#include "LevelColors.hpp"
#include "SearchView.hpp"
#include "UTF8.hpp"

SearchView::SearchView( Browser* parent, BottomBar& bar, Archive& archive, PersistentStorage& storage )
    : View( 0, 1, 0, -2 )
    , m_parent( parent )
    , m_bar( bar )
    , m_archive( &archive )
    , m_search( std::make_unique<SearchEngine>( archive ) )
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
        case KEY_EXIT:
        case 27:
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
                auto start = std::chrono::high_resolution_clock::now();
                m_result = m_search->Search( m_query.c_str(), SearchEngine::SF_AdjacentWords | SearchEngine::SF_FuzzySearch | SearchEngine::SF_SetLogic );
                m_queryTime = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - start ).count() / 1000.f;
                m_preview.clear();
                m_preview.reserve( m_result.results.size() );
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
        case 'j':
            MoveCursor( 1 );
            doupdate();
            break;
        case KEY_UP:
        case 'k':
            MoveCursor( -1 );
            doupdate();
            break;
        case KEY_NPAGE:
            MoveCursor( m_bottom - m_top );
            doupdate();
            break;
        case KEY_PPAGE:
            MoveCursor( m_top - m_bottom );
            doupdate();
            break;
        case KEY_ENTER:
        case '\n':
        case 459:   // numpad enter
            if( !m_result.results.empty() )
            {
                m_parent->OpenMessage( m_result.results[m_cursor].postid );
                m_active = false;
                return;
            }
            break;
        case 'a':
            std::sort( m_result.results.begin(), m_result.results.end(), [this]( const auto& l, const auto& r ) { return m_archive->GetDate( l.postid ) < m_archive->GetDate( r.postid ); } );
            m_top = m_bottom = m_cursor = 0;
            m_preview.clear();
            Draw();
            doupdate();
            break;
        case 'd':
            std::sort( m_result.results.begin(), m_result.results.end(), [this]( const auto& l, const auto& r ) { return m_archive->GetDate( l.postid ) > m_archive->GetDate( r.postid ); } );
            m_top = m_bottom = m_cursor = 0;
            m_preview.clear();
            Draw();
            doupdate();
            break;
        case 'r':
            std::sort( m_result.results.begin(), m_result.results.end(), []( const auto& l, const auto& r ) { return l.rank > r.rank; } );
            m_top = m_bottom = m_cursor = 0;
            m_preview.clear();
            Draw();
            doupdate();
            break;
        default:
            break;
        }
        m_bar.Update();
    }
}

void SearchView::Resize()
{
    ResizeView( 0, 1, 0, -2 );
    if( !m_active ) return;
    Draw();
}

void SearchView::Draw()
{
    werase( m_win );
    if( m_result.results.empty() )
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

        wattron( m_win, COLOR_PAIR( 8 ) | A_BOLD );
        mvwprintw( m_win, 6, 4, "Search hints:" );
        mvwprintw( m_win, 7, 6, "- Quote words to disable fuzzy search." );
        mvwprintw( m_win, 8, 6, "- Prepend word with from: to search for author." );
        mvwprintw( m_win, 9, 6, "- Prepend word with subject: to search in subject." );
        mvwprintw( m_win, 10, 6, "- Prepend word with + to require this word." );
        mvwprintw( m_win, 11, 6, "- Prepend word with - to exclude this word." );
        mvwprintw( m_win, 12, 6, "- Append word with * to search for any word with such beginning." );
        wattroff( m_win, COLOR_PAIR( 8 ) | A_BOLD );
    }
    else
    {
        wattron( m_win, COLOR_PAIR( 2 ) | A_BOLD );
        wprintw( m_win, " %i", m_result.results.size() );
        wattroff( m_win, COLOR_PAIR( 2 ) | A_BOLD );
        wprintw( m_win, " results for query: " );
        wattron( m_win, A_BOLD );
        wprintw( m_win, "%s", m_query.c_str() );
        wattron( m_win, COLOR_PAIR( 8 ) );
        wprintw( m_win, " (%.3f ms elapsed)", m_queryTime );
        wattroff( m_win, COLOR_PAIR( 8 ) | A_BOLD );

        const int h = getmaxy( m_win ) - 1;
        const int w = getmaxx( m_win );
        int cnt = m_top;
        int line = 0;
        while( line < h && cnt < m_result.results.size() )
        {
            const auto& res = m_result.results[cnt];
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
            char unpack[2048];
            m_archive->UnpackMsgId( m_archive->GetMessageId( res.postid ), unpack );
            if( m_storage.WasVisited( unpack ) )
            {
                waddch( m_win, 'R' );
            }
            else
            {
                waddch( m_win, '-' );
            }

            wprintw( m_win, " [" );
            auto realname = m_archive->GetRealName( res.postid );
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

            time_t date = m_archive->GetDate( res.postid );
            auto lt = localtime( &date );
            char buf[64];
            auto dlen = strftime( buf, 64, "%F %R", lt );

            auto subject = m_archive->GetSubject( res.postid );
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
            line++;

            int frameCol = COLOR_PAIR(6);
            assert( cnt <= m_preview.size() );
            if( m_preview.size() == cnt )
            {
                FillPreview( cnt );
            }

            const auto& preview = m_preview[cnt];
            auto it = preview.begin();
            auto itend = preview.end();
            while( it != itend && line != h )
            {
                wmove( m_win, line+1, 0 );
                wattron( m_win, frameCol );
                waddch( m_win, ACS_VLINE );
                wattroff( m_win, frameCol );
                int len = w - 3;

                bool newline;
                do
                {
                    int l = len;
                    auto end = utfendl( it->text.c_str(), l );
                    len -= l;

                    wattron( m_win, it->color );
                    utfprint( m_win, it->text.c_str(), end );
                    wattroff( m_win, it->color );

                    newline = it->newline;
                    ++it;
                }
                while( !newline );

                wmove( m_win, line+1, w-1 );
                wattron( m_win, frameCol );
                waddch( m_win, ACS_VLINE );
                wattroff( m_win, frameCol );
                line++;
            }
            if( line < h )
            {
                wattron( m_win, frameCol );
                waddch( m_win, ACS_LLCORNER );
                for( int i=0; i<w-2; i++ )
                {
                    waddch( m_win, '-' );
                }
                waddch( m_win, ACS_LRCORNER );
                wattroff( m_win, frameCol );
                line++;
            }

            cnt++;
        }
        m_bottom = cnt;
    }
    wnoutrefresh( m_win );
}

void SearchView::Reset( Archive& archive )
{
    m_archive = &archive;
    m_search = std::make_unique<SearchEngine>( archive );
    m_result.results.clear();
    m_query.clear();
    m_top = m_bottom = m_cursor = 0;
}

static inline bool CheckOverlap( const char* w1s, const char* w1e, const char* w2s, const char* w2e )
{
    return w1s < w2e && w1e > w2s;
}

void SearchView::FillPreview( int idx )
{
    const auto& res = m_result.results[idx];

    auto msg = std::string( m_archive->GetMessage( res.postid, m_eb ) );
    auto content = msg.c_str();
    // skip headers
    for(;;)
    {
        if( *content == '\n' ) break;
        while( *content++ != '\n' ) {}
    }
    content++;

    int wrote = DetectWrote( content );

    std::vector<std::vector<std::pair<const char*, const char*>>> wlmap;    // word-line map
    std::vector<std::pair<const char*, const char*>> lines;  // start-end addresses of all lines
    std::vector<LexiconType> linetype;
    {
        auto line = content;
        for(;;)
        {
            auto end = line;
            while( *end != '\n' && *end != '\0' ) end++;
            if( end - line == 4 && strncmp( line, "-- ", 3 ) == 0 ) break;  // start of signature
            auto tmp = line;
            const auto quotLevel = QuotationLevel( tmp, end );
            assert( wrote <= 0 || quotLevel == 0 );
            if( tmp != end )
            {
                lines.emplace_back( line, end );
                wlmap.emplace_back();
                if( wrote > 0 )
                {
                    linetype.emplace_back( T_Wrote );
                    wrote--;
                }
                else
                {
                    linetype.emplace_back( LexiconTypeFromQuotLevel( quotLevel ) );
                }
            }
            if( *end == '\0' ) break;
            line = end + 1;
        }
    }

    if( lines.size() == 0 )
    {
        m_preview.emplace_back();
        return;
    }

    std::vector<std::string> wordbuf;
    for( int h=0; h<res.hitnum; h++ )
    {
        const auto htype = LexiconDecodeType( res.hits[h] );
        if( htype == T_Signature || htype == T_From || htype == T_Subject ) continue;
        const auto hpos = LexiconHitPos( res.hits[h] );
        uint8_t max = LexiconHitPosMask[htype];

        int basePos = 0;
        for( int i=0; i<lines.size(); i++ )
        {
            auto line = lines[i].first;
            auto end = lines[i].second;

            auto ptr = line;
            QuotationLevel( ptr, end ); // just to walk ptr
            if( linetype[i] == htype )
            {
                SplitLine( ptr, end, wordbuf, hpos == max );
                if( basePos + wordbuf.size() > hpos )
                {
                    auto wptr = ptr;
                    if( hpos < max )
                    {
                        const auto& word = wordbuf[hpos - basePos];
                        for(;;)
                        {
                            while( strncmp( wptr, word.c_str(), word.size() ) != 0 )
                            {
                                wptr++;
                                assert( wptr <= end - word.size() );
                            }
                            auto wend = wptr + word.size();
                            if( std::find_if( wlmap[i].begin(), wlmap[i].end(), [wptr, wend] ( const auto& v ) { return CheckOverlap( wptr, wend, v.first, v.second ); } ) == wlmap[i].end() ) break;
                            wptr++;
                        }
                        wlmap[i].emplace_back( wptr, wptr + word.size() );
                        break;
                    }
                    else
                    {
                        const auto& word = m_result.matched[res.words[h]];
                        const auto wlen = strlen( word );

                        auto lower = ToLower( ptr, end );
                        auto lptr = lower.c_str();
                        auto lend = lptr + lower.size();
                        auto lstart = lptr;

                        bool next = false;
                        for(;;)
                        {
                            while( strncmp( lptr, word, wlen ) != 0 )
                            {
                                lptr++;
                                if( lptr > lend - wlen )
                                {
                                    next = true;
                                    break;
                                }
                            }
                            if( next ) break;
                            wptr = ptr + ( lptr - lstart );
                            auto wend = wptr + wlen;
                            if( std::find_if( wlmap[i].begin(), wlmap[i].end(), [wptr, wend] ( const auto& v ) { return CheckOverlap( wptr, wend, v.first, v.second ); } ) == wlmap[i].end() ) break;
                            lptr++;
                        }
                        if( !next )
                        {
                            wptr = ptr + ( lptr - lstart );
                            wlmap[i].emplace_back( wptr, wptr + wlen );
                            break;
                        }
                    }
                }
                else
                {
                    basePos += wordbuf.size();
                }
            }
        }
    }

    for( auto& v : wlmap )
    {
        std::sort( v.begin(), v.end(), []( const auto& l, const auto& r ) { return l.first < r.first; } );
    }

    enum class Print : uint8_t
    {
        False,
        True,
        Separator
    };

    std::vector<Print> print( lines.size(), Print::False );
    bool contentPresent = false;
    int remove = -1;
    for( int i=0; i<lines.size(); i++ )
    {
        if( !wlmap[i].empty() )
        {
            contentPresent = true;
            int start = std::max( 0, i - 2 );
            int end = std::min<int>( lines.size(), i + 3 );
            for( int j=start; j<end; j++ )
            {
                print[j] = Print::True;
            }
            if( end < lines.size() )
            {
                print[end] = Print::Separator;
                remove = end;
            }
            else
            {
                remove = -1;
            }
        }
    }
    if( remove != -1 )
    {
        print[remove] = Print::False;
    }

    if( !contentPresent )
    {
        for( int i=0; i<lines.size(); i++ )
        {
            if( linetype[i] == T_Content )
            {
                int start = std::max( 0, i-1 );
                int end = std::min<int>( i+3, lines.size() );
                while( start != end )
                {
                    print[start++] = Print::True;
                }
                break;
            }
        }
    }

    m_preview.emplace_back();
    auto& preview = m_preview.back();

    for( int i=0; i<lines.size(); i++ )
    {
        if( print[i] == Print::False ) continue;
        if( print[i] == Print::Separator )
        {
            preview.emplace_back( PreviewData { std::string( " -*-" ), COLOR_PAIR( 8 ) | A_BOLD, true } );
            continue;
        }

        auto ptr = lines[i].first;
        auto end = lines[i].second;

        if( wlmap[i].empty() )
        {
            if( ptr == end )
            {
                preview.back().newline = true;
            }
            else
            {
                auto tmp = ptr;
                auto quotLevel = QuotationLevel( tmp, end );

                auto color = quotLevel == 0 ? 0 : QuoteFlags[std::min( quotLevel-1, NumQuoteFlags-1 )];
                preview.emplace_back( PreviewData { std::string( ptr, end ), color, true } );
            }
        }
        else
        {
            auto tmp = ptr;
            auto quotLevel = QuotationLevel( tmp, end );
            auto color = quotLevel == 0 ? 0 : QuoteFlags[std::min( quotLevel-1, NumQuoteFlags-1 )];
            for( auto& v : wlmap[i] )
            {
                assert( v.first >= lines[i].first );
                assert( v.first < lines[i].second );
                assert( v.second > lines[i].first );
                assert( v.second <= lines[i].second );
                assert( v.first >= ptr );

                if( v.first != ptr )
                {
                    preview.emplace_back( PreviewData { std::string( ptr, v.first ), color, false } );
                }
                preview.emplace_back( PreviewData { std::string( v.first, v.second ), COLOR_PAIR( 16 ) | A_BOLD, false } );
                ptr = v.second;
            }
            preview.emplace_back( PreviewData { std::string( ptr, end ), color, true } );
        }
    }
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
        if( m_cursor == m_result.results.size() - 1 ) break;
        m_cursor++;
        if( m_cursor >= m_bottom - 1 )
        {
            m_top++;
            m_bottom++;
        }
        offset--;
    }
    Draw();
}
