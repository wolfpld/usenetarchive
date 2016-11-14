#include <algorithm>

#include "../common/String.hpp"
#include "../common/MessageLogic.hpp"
#include "../libuat/Archive.hpp"
#include "../libuat/PersistentStorage.hpp"

#include "MessageView.hpp"
#include "UTF8.hpp"

MessageView::MessageView( Archive& archive, PersistentStorage& storage )
    : View( 0, 0, 1, 1 )
    , m_archive( archive )
    , m_storage( storage )
    , m_idx( -1 )
    , m_active( false )
    , m_allHeaders( false )
{
}

MessageView::~MessageView()
{
}

void MessageView::Resize()
{
    if( !m_active ) return;
    ResizeView( 0, 0, 1, 1 );   // fucking stupid screen size is wrong without doing this shit first
    int sw = getmaxx( stdscr );
    m_vertical = sw > 160;
    if( m_vertical )
    {
        ResizeView( sw / 2, 1, sw - (sw / 2), -2 );
    }
    else
    {
        int sh = getmaxy( stdscr ) - 2;
        ResizeView( 0, 1 + sh * 20 / 100, 0, sh - ( sh * 20 / 100 ) );
    }
    Draw();
}

bool MessageView::Display( uint32_t idx, int move )
{
    if( idx != m_idx )
    {
        m_idx = idx;
        m_text = m_archive.GetMessage( idx );
        PrepareLines();
        m_top = 0;
        // If view is not active, drawing will be performed during resize.
        if( m_active )
        {
            Draw();
        }
        m_storage.MarkVisited( m_archive.GetMessageId( idx ) );
    }
    else if( m_active )
    {
        if( move > 0 )
        {
            if( m_top + move >= m_lines.size() ) return true;
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
        if( line >= m_lines.size() ) break;

        wmove( m_win, i, 0 );
        if( m_vertical )
        {
            wattron( m_win, COLOR_PAIR( 7 ) );
            waddch( m_win, ACS_VLINE );
            wattroff( m_win, COLOR_PAIR( 7 ) );
        }
        auto start = m_text + m_lines[line].offset;
        auto end = utfendcrlf( start, tw );
        if( m_lines[line].flags == L_Header )
        {
            auto hend = start;
            while( *hend != ' ' ) hend++;
            wattron( m_win, COLOR_PAIR( 2 ) | A_BOLD );
            wprintw( m_win, "%.*s", hend - start, start );
            wattroff( m_win, COLOR_PAIR( 2 ) );
            wattron( m_win, COLOR_PAIR( 7 ) );
            wprintw( m_win, "%.*s", end - hend, hend );
            wattroff( m_win, COLOR_PAIR( 7 ) | A_BOLD );
        }
        else
        {
            switch( m_lines[line].flags )
            {
            case L_Signature:
                wattron( m_win, COLOR_PAIR( 8 ) | A_BOLD );
                break;
            case L_Quote1:
                wattron( m_win, COLOR_PAIR( 5 ) );
                break;
            case L_Quote2:
                wattron( m_win, COLOR_PAIR( 3 ) );
                break;
            case L_Quote3:
                wattron( m_win, COLOR_PAIR( 6 ) | A_BOLD );
                break;
            case L_Quote4:
                wattron( m_win, COLOR_PAIR( 2 ) );
                break;
            default:
                break;
            }
            wprintw( m_win, "%.*s", end - start, start );
            switch( m_lines[line].flags )
            {
            case L_Signature:
                wattroff( m_win, COLOR_PAIR( 8 ) | A_BOLD );
                break;
            case L_Quote1:
                wattroff( m_win, COLOR_PAIR( 5 ) );
                break;
            case L_Quote2:
                wattroff( m_win, COLOR_PAIR( 3 ) );
                break;
            case L_Quote3:
                wattroff( m_win, COLOR_PAIR( 6 ) | A_BOLD );
                break;
            case L_Quote4:
                wattroff( m_win, COLOR_PAIR( 2 ) );
                break;
            default:
                break;
            }
        }
    }
    for( ; i<h-1; i++ )
    {
        wmove( m_win, i, 0 );
        if( m_vertical )
        {
            wattron( m_win, COLOR_PAIR( 7 ) );
            waddch( m_win, ACS_VLINE );
            wattroff( m_win, COLOR_PAIR( 7 ) );
        }
        wattron( m_win, COLOR_PAIR( 6 ) );
        wprintw( m_win, "~\n" );
    }
    wattroff( m_win, COLOR_PAIR( 6 ) );

    wmove( m_win, h-1, 0 );
    wattron( m_win, COLOR_PAIR( 1 ) );
    for( int i=0; i<w; i++ )
    {
        waddch( m_win, ' ' );
    }
    wmove( m_win, h-1, 0 );
    if( m_vertical )
    {
        wattron( m_win, COLOR_PAIR( 9 ) );
        waddch( m_win, ACS_VLINE );
        wattron( m_win, COLOR_PAIR( 1 ) );
    }
    waddch( m_win, ' ' );
    tw--;
    int len = tw;
    auto text = m_archive.GetSubject( m_idx );
    auto end = utfendl( text, len );
    utfprint( m_win, text, end );
    if( tw - len > 5 )
    {
        wattron( m_win, A_BOLD );
        wprintw( m_win, " :: " );
        wattroff( m_win, A_BOLD );
        text = m_archive.GetRealName( m_idx );
        end = utfend( text, w - len - 4 );
        utfprint( m_win, text, end );
    }
    wattroff( m_win, COLOR_PAIR( 1 ) );

    wnoutrefresh( m_win );
}

void MessageView::PrepareLines()
{
    m_lines.clear();
    auto txt = m_text;
    bool headers = true;
    bool sig = false;
    for(;;)
    {
        auto end = txt;
        while( *end != '\n' && *end != '\0' ) end++;
        if( headers )
        {
            if( end-txt == 0 )
            {
                m_lines.emplace_back( Line { uint32_t( txt - m_text ), L_Quote0 } );
                headers = false;
                while( *end == '\n' ) end++;
                end--;
            }
            else
            {
                if( m_allHeaders ||
                    strnicmpl( txt, "from: ", 6 ) == 0 ||
                    strnicmpl( txt, "newsgroups: ", 12 ) == 0 ||
                    strnicmpl( txt, "subject: ", 9 ) == 0 ||
                    strnicmpl( txt, "date: ", 6 ) == 0 )
                {
                    m_lines.emplace_back( Line { uint32_t( txt - m_text ), L_Header } );
                }
            }
        }
        else
        {
            if( strncmp( "-- \n", txt, 4 ) == 0 )
            {
                sig = true;
            }
            if( sig )
            {
                m_lines.emplace_back( Line { uint32_t( txt - m_text ), L_Signature } );
            }
            else
            {
                auto test = txt;
                int level = QuotationLevel( test, end );
                switch( level )
                {
                case 0:
                    m_lines.emplace_back( Line { uint32_t( txt - m_text ), L_Quote0 } );
                    break;
                case 1:
                    m_lines.emplace_back( Line { uint32_t( txt - m_text ), L_Quote1 } );
                    break;
                case 2:
                    m_lines.emplace_back( Line { uint32_t( txt - m_text ), L_Quote2 } );
                    break;
                case 3:
                    m_lines.emplace_back( Line { uint32_t( txt - m_text ), L_Quote3 } );
                    break;
                default:
                    m_lines.emplace_back( Line { uint32_t( txt - m_text ), L_Quote4 } );
                    break;
                }
            }
        }
        if( *end == '\0' ) break;
        txt = end + 1;
    }
}
