#include <limits>
#include <time.h>

#include "../libuat/Archive.hpp"
#include "../libuat/Galaxy.hpp"

#include "BottomBar.hpp"
#include "Browser.hpp"
#include "ChartView.hpp"
#include "Help.hpp"

enum { MarginW = 16 };
enum { MarginH = 16 };

const char* Block[2][8] = {
// Proper 1/8 steps, broken on too many fonts
{
    "\xE2\x96\x81",
    "\xE2\x96\x82",
    "\xE2\x96\x83",
    "\xE2\x96\x84",
    "\xE2\x96\x85",
    "\xE2\x96\x86",
    "\xE2\x96\x87",
    "\xE2\x96\x88"
},
// Double vertical resolution
{
    " ",
    " ",
    "\xE2\x96\x84",
    "\xE2\x96\x84",
    "\xE2\x96\x84",
    "\xE2\x96\x84",
    "\xE2\x96\x88",
    "\xE2\x96\x88"
} };

ChartView::ChartView( Browser* parent, Archive& archive, BottomBar& bar, Galaxy* galaxy )
    : View( 0, 1, 0, -2 )
    , m_parent( parent )
    , m_active( false )
    , m_archive( &archive )
    , m_search( std::make_unique<SearchEngine>( archive ) )
    , m_bar( bar )
    , m_galaxy( galaxy )
    , m_hires( false )
    , m_trend( false )
    , m_galaxyMode( false )
{
}

void ChartView::Entry()
{
    Prepare();

    m_active = true;
    Draw();
    doupdate();

    while( auto key = GetKey() )
    {
        switch( key )
        {
        case KEY_RESIZE:
            m_parent->Resize();
            Resize();
            doupdate();
            break;
        case 'q':
            m_active = false;
            return;
        case 'h':
            m_hires = !m_hires;
            Draw();
            doupdate();
            break;
        case 'o':
            m_trend = !m_trend;
            m_galaxyMode = false;
            Prepare();
            Draw();
            doupdate();
            break;
        case '?':
            m_parent->DisplayTextView( ChartHelpContents );
            Draw();
            doupdate();
            break;
        case 's':
        case '/':
        {
            auto query = m_bar.Query( "Search: ", m_query.c_str() );
            if( !query.empty() )
            {
                m_galaxyMode = false;
                m_posts.clear();
                std::swap( m_query, query );
                const auto res = m_search->Search( m_query.c_str(), SearchEngine::SF_AdjacentWords | SearchEngine::SF_FuzzySearch | SearchEngine::SF_SetLogic );
                const auto sz = res.results.size();
                if( sz > 1 )
                {
                    m_posts.reserve( sz );
                    for( auto& v : res.results )
                    {
                        m_posts.emplace_back( v.postid );
                    }
                }
                Prepare();
            }
            else
            {
                m_bar.Update();
            }
            Draw();
            doupdate();
            break;
        }
        case 'a':
            m_galaxyMode = false;
            m_posts.clear();
            Prepare();
            Draw();
            doupdate();
            break;
        case 'g':
            if( !m_galaxy )
            {
                m_bar.Status( "Galaxy not available." );
            }
            else if( !m_galaxyMode )
            {
                m_galaxyMode = true;
                m_posts.clear();
                Prepare();
                Draw();
                doupdate();
            }
            break;
        default:
            break;
        }
        m_bar.Update();
    }
}

void ChartView::Resize()
{
    ResizeView( 0, 1, 0, -2 );
    if( !m_active ) return;
    Prepare();
    Draw();
}

void ChartView::Draw()
{
    int w, h;
    getmaxyx( m_win, h, w );
    werase( m_win );

    wmove( m_win, 1, (w-14)/2 );
    wattron( m_win, COLOR_PAIR( 4 ) | A_BOLD );
    wprintw( m_win, "Activity chart" );
    wattroff( m_win, COLOR_PAIR( 4 ) | A_BOLD );

    wmove( m_win, 3, 4 );
    if( m_galaxyMode )
    {
        wprintw( m_win, "Showing all posts in galaxy." );
    }
    else if( m_posts.empty() )
    {
        wprintw( m_win, "Showing all posts." );
    }
    else
    {
        wprintw( m_win, "Results for: %s", m_query.c_str() );
    }

    const auto x0 = 9;
    const auto y0 = 6;
    const auto xs = w - MarginW;
    const auto ys = h - MarginH;

    char tmp[32];
    sprintf( tmp, "%i", m_max );
    wattron( m_win, COLOR_PAIR( 2 ) | A_BOLD );
    wmove( m_win, y0 - 1, x0 - 1 - strlen( tmp ) );
    wprintw( m_win, "%s ", tmp );
    wattroff( m_win, COLOR_PAIR( 2 ) | A_BOLD );

    waddch( m_win, ACS_UARROW );
    for( int y=y0; y<y0+ys+1; y++ )
    {
        wmove( m_win, y, x0 );
        waddch( m_win, ACS_VLINE );
    }
    wmove( m_win, y0+ys+1, x0 - 2 );
    wattron( m_win, COLOR_PAIR( 2 ) | A_BOLD );
    wprintw( m_win, "0 " );
    wattroff( m_win, COLOR_PAIR( 2 ) | A_BOLD );
    waddch( m_win, ACS_LLCORNER );
    for( int x=0; x<xs; x++ )
    {
        waddch( m_win, ACS_HLINE );
    }
    waddch( m_win, ACS_RARROW );

    const auto color = m_posts.empty() ? 7 : 5;

    assert( m_data.size() == xs );
    for( int x=0; x<xs; x++ )
    {
        if( x % 4 == 0 )
        {
            wattroff( m_win, COLOR_PAIR( color ) );
            wattron( m_win, COLOR_PAIR( 4 ) );
            for( int j=0; j<7; j++ )
            {
                wmove( m_win, y0+2+ys+j, x0+1+x+j );
                waddch( m_win, m_label[x][j] );
            }
            wattroff( m_win, COLOR_PAIR( 4 ) );
            wattron( m_win, COLOR_PAIR( color ) );
        }
        const auto v = m_data[x];
        if( v == 0 ) continue;
        const auto f = v / 8;
        const auto r = v % 8;
        if( m_posts.empty() || !m_trend )
        {
            for( int y=0; y<f; y++ )
            {
                wmove( m_win, y0+1+ys-y-1, x0+1+x );
                wprintw( m_win, Block[m_hires ? 0 : 1][7] );
            }
            if( r > 0 )
            {
                wmove( m_win, y0+1+ys-f-1, x0+1+x );
                wprintw( m_win, Block[m_hires ? 0 : 1][r-1] );
            }
        }
        else
        {
            const auto t = m_trendData[x];
            auto tf = t / 8;
            const auto tr = t % 8;
            for( int y=0; y<tf; y++ )
            {
                wmove( m_win, y0+1+ys-y-1, x0+1+x );
                wprintw( m_win, Block[m_hires ? 0 : 1][7] );
            }
            wattroff( m_win, COLOR_PAIR( color ) );
            if( tr > 0 )
            {
                wattron( m_win, COLOR_PAIR( 17 ) );
                wmove( m_win, y0+1+ys-tf-1, x0+1+x );
                wprintw( m_win, Block[m_hires ? 0 : 1][tr-1] );
                wattroff( m_win, COLOR_PAIR( 17 ) );
                tf++;
            }
            wattron( m_win, COLOR_PAIR( 7 ) );
            for( int y=tf; y<f; y++ )
            {
                wmove( m_win, y0+1+ys-y-1, x0+1+x );
                wprintw( m_win, Block[m_hires ? 0 : 1][7] );
            }
            if( r > 0 )
            {
                wmove( m_win, y0+1+ys-f-1, x0+1+x );
                wprintw( m_win, Block[m_hires ? 0 : 1][r-1] );
            }
            wattroff( m_win, COLOR_PAIR( 7 ) );
            wattron( m_win, COLOR_PAIR( color ) );
        }
    }
    wattroff( m_win, COLOR_PAIR( color ) );

    wnoutrefresh( m_win );
}

void ChartView::Prepare()
{
    int w, h;
    getmaxyx( m_win, h, w );

    uint32_t tbegin = std::numeric_limits<uint32_t>::max();
    uint32_t tend = std::numeric_limits<uint32_t>::min();

    size_t msgsz;
    if( m_galaxyMode )
    {
        assert( m_galaxy );
        msgsz = 0;
        const auto archsz = m_galaxy->GetNumberOfArchives();
        for( size_t i=0; i<archsz; i++ )
        {
            if( !m_galaxy->IsArchiveAvailable( i ) ) continue;
            auto& arch = m_galaxy->GetArchive( i, false );
            const auto sz = arch->NumberOfMessages();
            for( uint32_t i=0; i<sz; i++ )
            {
                const auto t = arch->GetDate( i );
                if( t != 0 )
                {
                    if( t < tbegin ) tbegin = t;
                    if( t > tend ) tend = t;
                }
            }
            msgsz += sz;
        }
    }
    else if( m_posts.empty() || m_trend )
    {
        msgsz = m_archive->NumberOfMessages();
        for( uint32_t i=0; i<msgsz; i++ )
        {
            const auto t = m_archive->GetDate( i );
            if( t != 0 )
            {
                if( t < tbegin ) tbegin = t;
                if( t > tend ) tend = t;
            }
        }
    }
    else
    {
        msgsz = m_posts.size();
        for( uint32_t i=0; i<msgsz; i++ )
        {
            const auto t = m_archive->GetDate( m_posts[i] );
            if( t != 0 )
            {
                if( t < tbegin ) tbegin = t;
                if( t > tend ) tend = t;
            }
        }
    }
    assert( tbegin <= tend );

    const auto trange = tend - tbegin + 1;
    const auto segments = w - MarginW;
    const auto tinv = double( segments-1 ) / trange;

    std::vector<uint32_t> seg( segments );

    if( m_galaxyMode )
    {
        const auto archsz = m_galaxy->GetNumberOfArchives();
        for( size_t i=0; i<archsz; i++ )
        {
            if( !m_galaxy->IsArchiveAvailable( i ) ) continue;
            auto& arch = m_galaxy->GetArchive( i, false );
            const auto sz = arch->NumberOfMessages();
            for( uint32_t i=0; i<sz; i++ )
            {
                const auto t = arch->GetDate( i );
                if( t != 0 )
                {
                    const auto s = uint32_t( ( t - tbegin ) * tinv );
                    assert( s >= 0 && s < segments );
                    seg[s]++;
                }
            }
        }
    }
    else if( m_posts.empty() || m_trend )
    {
        for( uint32_t i=0; i<msgsz; i++ )
        {
            const auto t = m_archive->GetDate( i );
            if( t != 0 )
            {
                const auto s = uint32_t( ( t - tbegin ) * tinv );
                assert( s >= 0 && s < segments );
                seg[s]++;
            }
        }
    }
    else
    {
        for( uint32_t i=0; i<msgsz; i++ )
        {
            const auto t = m_archive->GetDate( m_posts[i] );
            if( t != 0 )
            {
                const auto s = uint32_t( ( t - tbegin ) * tinv );
                assert( s >= 0 && s < segments );
                seg[s]++;
            }
        }
    }

    m_max = 0;
    for( const auto& v : seg ) if( v > m_max ) m_max = v;

    const auto th = h - MarginH + 2;
    m_data = std::vector<uint16_t>( segments );
    m_label = std::vector<char[7]>( segments );
    const auto minv = 1. / ( m_max+1 );
    const auto sinv = 1. / tinv;
    for( int i=0; i<segments; i++ )
    {
        m_data[i] = seg[i] * minv * th * 8;
        const time_t ts = uint32_t( i * sinv ) + tbegin;
        auto lt = localtime( &ts );
        char buf[16];
        auto dlen = strftime( buf, 16, "%Y-%m", lt );
        memcpy( m_label[i], buf, 7 );
    }

    if( !m_posts.empty() && m_trend )
    {
        memset( seg.data(), 0, sizeof( uint32_t ) * segments );
        msgsz = m_posts.size();
        for( uint32_t i=0; i<msgsz; i++ )
        {
            const auto t = m_archive->GetDate( m_posts[i] );
            if( t != 0 )
            {
                const auto s = uint32_t( ( t - tbegin ) * tinv );
                assert( s >= 0 && s < segments );
                seg[s]++;
            }
        }

        m_trendData = std::vector<uint16_t>( segments );
        for( int i=0; i<segments; i++ )
        {
            m_trendData[i] = seg[i] * minv * th * 8;
        }
    }
}

void ChartView::Reset( Archive& archive )
{
    m_archive = &archive;
    m_search = std::make_unique<SearchEngine>( archive );
    m_query.clear();
    m_posts.clear();
}
