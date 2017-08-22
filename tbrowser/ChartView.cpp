#include <limits>

#include "../libuat/Archive.hpp"

#include "Browser.hpp"
#include "ChartView.hpp"

enum { MarginW = 12 };
enum { MarginH = 10 };

// Proper 1/8 steps, broken on too many fonts
/*
const char* Block[8] = {
    "\xE2\x96\x81",
    "\xE2\x96\x82",
    "\xE2\x96\x83",
    "\xE2\x96\x84",
    "\xE2\x96\x85",
    "\xE2\x96\x86",
    "\xE2\x96\x87",
    "\xE2\x96\x88"
};
*/

// Double vertical resolution
const char* Block[8] = {
    "",
    "",
    "",
    "\xE2\x96\x84",
    "\xE2\x96\x84",
    "\xE2\x96\x84",
    "\xE2\x96\x84",
    "\xE2\x96\x88"
};

ChartView::ChartView( Browser* parent )
    : View( 0, 1, 0, -2 )
    , m_parent( parent )
    , m_active( false )
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
            break;
        case 'q':
            m_active = false;
            return;
        default:
            break;
        }
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

    const auto x0 = MarginW - 3;
    const auto y0 = MarginH / 2 - 1;
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

    assert( m_data.size() == xs );
    wattron( m_win, COLOR_PAIR( 7 ) );
    for( int x=0; x<xs; x++ )
    {
        const auto v = m_data[x];
        if( v == 0 ) continue;
        const auto f = v / 8;
        const auto r = v % 8;
        for( int y=0; y<f; y++ )
        {
            wmove( m_win, y0+1+ys-y-1, x0+1+x );
            wprintw( m_win, Block[7] );
        }
        if( r > 0 )
        {
            wmove( m_win, y0+1+ys-f-1, x0+1+x );
            wprintw( m_win, Block[r-1] );
        }
    }
    wattroff( m_win, COLOR_PAIR( 7 ) );

    wnoutrefresh( m_win );
}

void ChartView::Prepare()
{
    int w, h;
    getmaxyx( m_win, h, w );

    werase( m_win );
    mvwprintw( m_win, h/2, (w-14)/2, "Please wait..." );
    wnoutrefresh( m_win );
    doupdate();

    auto& archive = m_parent->GetArchive();
    const auto msgsz = archive.NumberOfMessages();
    uint32_t tbegin = std::numeric_limits<uint32_t>::max();
    uint32_t tend = std::numeric_limits<uint32_t>::min();
    for( uint32_t i=0; i<msgsz; i++ )
    {
        const auto t = archive.GetDate( i );
        if( t != 0 )
        {
            if( t < tbegin ) tbegin = t;
            if( t > tend ) tend = t;
        }
    }
    const auto trange = tend - tbegin + 1;
    const auto segments = w - MarginW;
    const auto tinv = double( segments-1 ) / trange;

    std::vector<uint32_t> seg( segments );

    for( uint32_t i=0; i<msgsz; i++ )
    {
        const auto t = archive.GetDate( i );
        if( t != 0 )
        {
            const auto s = uint32_t( ( t - tbegin ) * tinv );
            assert( s >= 0 && s < segments );
            seg[s]++;
        }
    }

    m_max = 0;
    for( const auto& v : seg ) if( v > m_max ) m_max = v;

    const auto th = h - MarginH + 2;
    m_data = std::vector<uint16_t>( segments );
    const auto minv = 1. / ( m_max+1 );
    for( int i=0; i<segments; i++ )
    {
        m_data[i] = seg[i] * minv * th * 8;
    }
}
