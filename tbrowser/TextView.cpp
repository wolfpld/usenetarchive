#include <algorithm>
#include <assert.h>
#include <chrono>
#include <ctype.h>
#include <sstream>
#include <vector>

#include "Browser.hpp"
#include "TextView.hpp"
#include "UTF8.hpp"

TextView::TextView( Browser* parent )
    : View( 0, 1, 0, -2 )
    , m_parent( parent )
    , m_active( false )
    , m_linesWidth( -1 )
{
}

void TextView::Entry( const char* text, int size )
{
    if( size == -1 ) size = strlen( text );
    m_size = size;
    m_text = text;
    PrepareLines();
    m_top = 0;

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
        case KEY_ENTER:
        case '\n':
        case 459:   // numpad enter
        case 'j':
        case KEY_DOWN:
            Move( 1 );
            doupdate();
            break;
        case KEY_BACKSPACE:
        case '\b':
        case 127:
        case 'k':
        case KEY_UP:
            Move( -1 );
            doupdate();
            break;
        case KEY_PPAGE:
        case KEY_DC:
        case 'b':
            Move( -( GetHeight() - 1 ) );
            doupdate();
            break;
        case KEY_NPAGE:
        case ' ':
            Move( GetHeight() - 1 );
            doupdate();
            break;
        default:
            break;
        }
    }
}

void TextView::Resize()
{
    ResizeView( 0, 1, 0, -2 );
    if( !m_active ) return;
    int sw = getmaxx( stdscr );
    if( sw != m_linesWidth )
    {
        PrepareLines();
    }
    Draw();
}

void TextView::Draw()
{
    int w, h;
    getmaxyx( m_win, h, w );
    int tw = w;
    werase( m_win );
    int i;
    for( i=0; i<h; i++ )
    {
        int line = m_top + i;
        if( line >= m_lines.size() ) break;

        wmove( m_win, i, 0 );
        auto start = m_text + m_lines[line].offset;
        auto end = start + m_lines[line].len;
        if( m_lines[line].linebreak )
        {
            wattron( m_win, COLOR_PAIR( 10 ) );
            waddch( m_win, '+' );
            wattroff( m_win, COLOR_PAIR( 10 ) );
        }
        wprintw( m_win, "%.*s", end - start, start );
    }
    wattron( m_win, COLOR_PAIR( 6 ) );
    for( ; i<h; i++ )
    {
        wmove( m_win, i, 0 );
        wprintw( m_win, "~\n" );
    }
    wattroff( m_win, COLOR_PAIR( 6 ) );
    wnoutrefresh( m_win );
}

void TextView::Move( int move )
{
    if( move > 0 )
    {
        if( m_top + move >= m_lines.size() ) return;
        m_top += move;
    }
    else if( move < 0 )
    {
        m_top = std::max( 0, m_top + move );
    }
    Draw();
}

void TextView::PrepareLines()
{
    // window width may be invalid here
    m_linesWidth = getmaxx( stdscr );
    m_lines.clear();
    if( m_linesWidth < 2 ) return;
    auto txt = m_text;
    for(;;)
    {
        auto end = txt;
        while( *end != '\n' && end - m_text < m_size ) end++;
        const auto len = std::min<uint32_t>( end - txt, ( 1 << LenBits ) - 1 );
        const auto offset = uint32_t( txt - m_text );
        if( offset >= ( 1 << OffsetBits ) ) return;
        BreakLine( offset, len );
        if( *end == '\0' ) break;
        txt = end + 1;
    }
    if( m_lines.back().len == 0 ) m_lines.pop_back();
}

void TextView::BreakLine( uint32_t offset, uint32_t len )
{
    auto ul = utflen( m_text + offset, m_text + offset + len );
    if( ul <= m_linesWidth )
    {
        m_lines.emplace_back( Line { offset, len, false } );
    }
    else
    {
        bool br = false;
        auto ptr = m_text + offset;
        auto end = ptr + len;
        auto w = m_linesWidth;
        while( ptr != end )
        {
            auto e = utfendcrlf( ptr, w );
            m_lines.emplace_back( Line { uint32_t( ptr - m_text ), uint32_t( e - ptr ), br } );
            ptr = e;
            if( !br )
            {
                br = true;
                w--;
            }
        }
    }
}
