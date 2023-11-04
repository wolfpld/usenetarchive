#include <algorithm>
#include <assert.h>
#include <ctype.h>

#include "../common/UTF8.hpp"
#include "../libuat/Archive.hpp"
#include "../libuat/PersistentStorage.hpp"

#include "LevelColors.hpp"
#include "MessageView.hpp"
#include "Utf8Print.hpp"

MessageView::MessageView( Archive& archive, PersistentStorage& storage )
    : View( 0, 0, 1, 1 )
    , m_archive( &archive )
    , m_storage( storage )
    , m_idx( -1 )
    , m_linesWidth( -1 )
    , m_active( false )
    , m_allHeaders( false )
    , m_rot13( false )
    , m_viewSplit( ViewSplit::Auto )
{
}

void MessageView::Reset( Archive& archive )
{
    Close();
    m_archive = &archive;
    m_idx = -1;
    m_lines.Reset();
}

void MessageView::Resize()
{
    if( !m_active ) return;
    ResizeView( 0, 0, 1, 1 );   // fucking stupid screen size is wrong without doing this shit first
    int sw = getmaxx( stdscr );
    switch( m_viewSplit )
    {
    case ViewSplit::Auto:
        m_vertical = sw > 160;
        break;
    case ViewSplit::Vertical:
        m_vertical = true;
        break;
    case ViewSplit::Horizontal:
        m_vertical = false;
        break;
    default:
        assert( false );
        break;
    }
    int rsw;
    if( m_vertical )
    {
        rsw = sw - (sw / 2);
        ResizeView( sw / 2, 1, rsw, -2 );
    }
    else
    {
        int sh = getmaxy( stdscr ) - 2;
        ResizeView( 0, 1 + sh * 20 / 100, 0, sh - ( sh * 20 / 100 ) );
        rsw = sw;
    }
    if( rsw != m_linesWidth )
    {
        PrepareLines();
    }
    Draw();
}

bool MessageView::Display( uint32_t idx, int move )
{
    if( idx != m_idx )
    {
        m_idx = idx;
        m_text = m_archive->GetMessage( idx, m_eb );
        PrepareLines();
        m_top = 0;
        // If view is not active, drawing will be performed during resize.
        if( m_active )
        {
            Draw();
        }
        char unpack[2048];
        m_archive->UnpackMsgId( m_archive->GetMessageId( idx ), unpack );
        m_storage.MarkVisited( unpack );
    }
    else if( m_active )
    {
        if( move > 0 )
        {
            if( m_top + move >= m_lines.Lines().size() ) return true;
            m_top += move;
        }
        else if( move < 0 )
        {
            m_top = std::max( 0, m_top + move );
        }
        Draw();
    }
    m_active = true;
    return false;
}

void MessageView::Close()
{
    m_active = false;
}

void MessageView::SwitchHeaders()
{
    m_allHeaders = !m_allHeaders;
    m_top = 0;
    if( m_idx != -1 )
    {
        PrepareLines();
    }
    if( m_active )
    {
        Draw();
    }
}

void MessageView::SwitchROT13()
{
    m_rot13 = !m_rot13;
    if( m_active )
    {
        Draw();
    }
}

void MessageView::Draw()
{
    int w, h;
    getmaxyx( m_win, h, w );
    int tw = w;
    if( m_vertical ) tw--;
    werase( m_win );
    int i;
    for( i=0; i<h-1; i++ )
    {
        int line = m_top + i;
        if( line >= m_lines.Lines().size() ) break;

        wmove( m_win, i, 0 );
        if( m_vertical )
        {
            wattron( m_win, COLOR_PAIR( 7 ) );
            waddch( m_win, ACS_VLINE );
            wattroff( m_win, COLOR_PAIR( 7 ) );
        }
        if( m_lines.Lines()[line].parts == 0 ) continue;
        const MessageLines::LinePart* part = m_lines.Parts().data() + m_lines.Lines()[line].idx;
        const int pnum = m_lines.Lines()[line].parts;
        for( int p=0; p<pnum; p++, part++ )
        {
            auto start = m_text + part->offset;
            auto end = start + part->len;
            if( part->linebreak )
            {
                wattron( m_win, COLOR_PAIR( 10 ) );
                waddch( m_win, '+' );
                wattroff( m_win, COLOR_PAIR( 10 ) );
            }
            int len = end - start;
            bool rot13allowed = true;
            switch( part->flags )
            {
            case MessageLines::L_HeaderName:
                rot13allowed = false;
                wattron( m_win, COLOR_PAIR( 2 ) | A_BOLD );
                break;
            case MessageLines::L_HeaderBody:
                wattron( m_win, COLOR_PAIR( 7 ) | A_BOLD );
                break;
            case MessageLines::L_Signature:
                wattron( m_win, COLOR_PAIR( 8 ) | A_BOLD );
                break;
            case MessageLines::L_Quote1:
            case MessageLines::L_Quote2:
            case MessageLines::L_Quote3:
            case MessageLines::L_Quote4:
            case MessageLines::L_Quote5:
                wattron( m_win, QuoteFlags[part->flags - MessageLines::L_Quote1] );
                break;
            default:
                break;
            }
            switch( part->deco )
            {
#ifdef _MSC_VER
            case MessageLines::D_Underline:
            case MessageLines::D_Italics:
            case MessageLines::D_Url:
#else
            case MessageLines::D_Underline:
            case MessageLines::D_Url:
                wattron( m_win, A_UNDERLINE );
                break;
            case MessageLines::D_Italics:
#  ifdef A_ITALIC
                wattron( m_win, A_ITALIC );
#  endif
                break;
#endif
            case MessageLines::D_Bold:
                wattron( m_win, A_BOLD );
                break;
            case MessageLines::D_None:
            default:
                break;
            }
            if( m_rot13 && rot13allowed )
            {
                PrintRot13( start, end );
            }
            else
            {
                wprintw( m_win, "%.*s", len, start );
            }
            switch( part->flags )
            {
            case MessageLines::L_HeaderName:
                wattroff( m_win, COLOR_PAIR( 2 ) | A_BOLD );
                break;
            case MessageLines::L_HeaderBody:
                wattroff( m_win, COLOR_PAIR( 7 ) | A_BOLD );
                break;
            case MessageLines::L_Signature:
                wattroff( m_win, COLOR_PAIR( 8 ) | A_BOLD );
                break;
            case MessageLines::L_Quote1:
            case MessageLines::L_Quote2:
            case MessageLines::L_Quote3:
            case MessageLines::L_Quote4:
            case MessageLines::L_Quote5:
                wattroff( m_win, QuoteFlags[part->flags - MessageLines::L_Quote1] );
                break;
            default:
                break;
            }
            switch( part->deco )
            {
#ifdef _MSC_VER
            case MessageLines::D_Underline:
            case MessageLines::D_Italics:
            case MessageLines::D_Url:
#else
            case MessageLines::D_Underline:
            case MessageLines::D_Url:
                wattroff( m_win, A_UNDERLINE );
                break;
            case MessageLines::D_Italics:
#  ifdef A_ITALIC
                wattroff( m_win, A_ITALIC );
#  endif
                break;
#endif
            case MessageLines::D_Bold:
                wattroff( m_win, A_BOLD );
                break;
            case MessageLines::D_None:
            default:
                break;
            }
        }
    }
    wattron( m_win, COLOR_PAIR( 6 ) );
    const bool atEnd = i<h-1;
    for( ; i<h-1; i++ )
    {
        wmove( m_win, i, 0 );
        if( m_vertical )
        {
            wattron( m_win, COLOR_PAIR( 7 ) );
            waddch( m_win, ACS_VLINE );
            wattron( m_win, COLOR_PAIR( 6 ) );
        }
        wprintw( m_win, "~\n" );
    }
    wattroff( m_win, COLOR_PAIR( 6 ) );

    char* tmp = (char*)alloca( w+1 );
    memset( tmp, ' ', w );
    tmp[w] = '\0';
    wmove( m_win, h-1, 0 );
    wattron( m_win, COLOR_PAIR( 1 ) );
    wprintw( m_win, "%s", tmp );
    wmove( m_win, h-1, 0 );
    if( m_vertical )
    {
        wattron( m_win, COLOR_PAIR( 9 ) );
        waddch( m_win, ACS_VLINE );
    }
    wattron( m_win, COLOR_PAIR( 11 ) | A_BOLD );
    waddch( m_win, ' ' );
    tw--;
    int len = tw;
    auto text = m_archive->GetSubject( m_idx );
    auto end = utfendl( text, len );
    utfprint( m_win, text, end );
    if( tw - len > 5 )
    {
        wattron( m_win, COLOR_PAIR( 1 ) );
        wprintw( m_win, " :: " );
        wattron( m_win, COLOR_PAIR( 11 ) );
        text = m_archive->GetRealName( m_idx );
        end = utfend( text, w - len - 4 );
        utfprint( m_win, text, end );
    }
    if( atEnd )
    {
        sprintf( tmp, " (End) " );
    }
    else if( m_top == 0 )
    {
        sprintf( tmp, " (Top) " );
    }
    else
    {
        sprintf( tmp, " (%.1f%%%%) ", 100.f * ( m_top + h - 1 ) / m_lines.Lines().size() );
    }
    len = strlen( tmp );
    wmove( m_win, h-1, w-len );
    wprintw( m_win, "%s", tmp );
    wattroff( m_win, COLOR_PAIR( 11 ) | A_BOLD );

    wnoutrefresh( m_win );
}

void MessageView::PrepareLines()
{
    // window width may be invalid here
    m_linesWidth = getmaxx( stdscr );
    if( m_vertical )
    {
        m_linesWidth = m_linesWidth - (m_linesWidth/2) - 1;
    }

    m_lines.Reset();
    if( m_linesWidth < 2 ) return;
    m_lines.SetWidth( m_linesWidth );
    m_lines.PrepareLines( m_text, !m_allHeaders );
}

void MessageView::PrintRot13( const char* start, const char* end )
{
    assert( start <= end );
    char* tmp = (char*)alloca( end - start );
    auto src = start;
    auto dst = tmp;
    while( src != end )
    {
        const auto c = *src;
        const auto cpl = codepointlen( c );
        if( cpl == 1 )
        {
            if( c >= 'a' && c <= 'z' )
            {
                *dst = ( ( c - 'a' + 13 ) % 26 ) + 'a';
            }
            else if( c >= 'A' && c <= 'Z' )
            {
                *dst = ( ( c - 'A' + 13 ) % 26 ) + 'A';
            }
            else
            {
                *dst = c;
            }
            src++;
            dst++;
        }
        else
        {
            memcpy( dst, src, cpl );
            src += cpl;
            dst += cpl;
        }
    }
    wprintw( m_win, "%.*s", int( end - start ), tmp );
}

ViewSplit MessageView::NextViewSplit()
{
    auto i = (int)m_viewSplit;
    i++;
    if( i == (int)ViewSplit::NUM_VIEW_SPLIT ) i = 0;
    m_viewSplit = (ViewSplit)i;
    return m_viewSplit;
}
