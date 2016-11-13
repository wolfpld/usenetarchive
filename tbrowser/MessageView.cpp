#include "../common/String.hpp"
#include "../common/MessageLogic.hpp"
#include "../libuat/Archive.hpp"

#include "MessageView.hpp"

MessageView::MessageView( Archive& archive )
    : View( 0, 0, 1, 1 )
    , m_archive( archive )
    , m_idx( -1 )
    , m_active( false )
{
}

MessageView::~MessageView()
{
}

void MessageView::Resize()
{
    if( !m_active ) return;
    int sw = getmaxx( stdscr );
    m_vertical = sw > 160;
    if( m_vertical )
    {
        ResizeView( sw / 2, 1, (sw+1) / 2, -2 );
    }
    else
    {
        int sh = getmaxy( stdscr ) - 2;
        ResizeView( 0, 1 + sh * 20 / 100, 0, sh - ( sh * 20 / 100 ) );
    }
    Draw();
}

void MessageView::Display( uint32_t idx )
{
    if( idx != m_idx )
    {
        m_idx = idx;
        m_text = m_archive.GetMessage( idx );
        PrepareLines();
    }
    // If view is not active, drawing will be performed during resize.
    if( m_active )
    {
        Draw();
    }
    m_active = true;
}

void MessageView::Close()
{
    m_active = false;
}

void MessageView::Draw()
{
    werase( m_win );
    wprintw( m_win, "%i\n", m_idx );
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
                if( strnicmpl( txt, "from: ", 6 ) == 0 ||
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
