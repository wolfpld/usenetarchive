#include <algorithm>

#include "../common/Alloc.hpp"
#include "../common/String.hpp"
#include "../common/MessageLogic.hpp"
#include "../libuat/Archive.hpp"
#include "../libuat/PersistentStorage.hpp"

#include "LevelColors.hpp"
#include "MessageView.hpp"
#include "UTF8.hpp"

MessageView::MessageView( Archive& archive, PersistentStorage& storage )
    : View( 0, 0, 1, 1 )
    , m_archive( &archive )
    , m_storage( storage )
    , m_idx( -1 )
    , m_linesWidth( -1 )
    , m_active( false )
    , m_allHeaders( false )
    , m_rot13( false )
{
    m_lineParts.reserve( 2048 );
    m_lines.reserve( 1024 );
}

void MessageView::Reset( Archive& archive )
{
    Close();
    m_archive = &archive;
    m_idx = -1;
    m_lineParts.clear();
    m_lines.clear();
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
    if( sw != m_linesWidth )
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

void MessageView::SwitchROT13()
{
    m_rot13 = !m_rot13;
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
        if( m_lines[line].empty ) continue;
        const LinePart* part = m_lineParts.data() + m_lines[line].idx;
        const int pnum = m_lines[line].parts;
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
            case L_HeaderName:
                rot13allowed = false;
                wattron( m_win, COLOR_PAIR( 2 ) | A_BOLD );
                break;
            case L_HeaderBody:
                wattron( m_win, COLOR_PAIR( 7 ) | A_BOLD );
                break;
            case L_Signature:
                wattron( m_win, COLOR_PAIR( 8 ) | A_BOLD );
                break;
            case L_Quote1:
            case L_Quote2:
            case L_Quote3:
            case L_Quote4:
            case L_Quote5:
                wattron( m_win, QuoteFlags[part->flags - L_Quote1] );
                break;
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
            case L_HeaderName:
                wattroff( m_win, COLOR_PAIR( 2 ) | A_BOLD );
                break;
            case L_HeaderBody:
                wattroff( m_win, COLOR_PAIR( 7 ) | A_BOLD );
                break;
            case L_Signature:
                wattroff( m_win, COLOR_PAIR( 8 ) | A_BOLD );
                break;
            case L_Quote1:
            case L_Quote2:
            case L_Quote3:
            case L_Quote4:
            case L_Quote5:
                wattroff( m_win, QuoteFlags[part->flags - L_Quote1] );
                break;
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
    wprintw( m_win, tmp );
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
        sprintf( tmp, " (%.1f%%%%) ", 100.f * ( m_top + h - 1 ) / m_lines.size() );
    }
    len = strlen( tmp );
    wmove( m_win, h-1, w-len );
    wprintw( m_win, tmp );
    wattroff( m_win, COLOR_PAIR( 11 ) | A_BOLD );

    wnoutrefresh( m_win );
}

void MessageView::PrepareLines()
{
    // window width may be invalid here
    m_linesWidth = getmaxx( stdscr );
    if( m_linesWidth > 160 )
    {
        m_linesWidth = m_linesWidth - (m_linesWidth/2) - 1;
    }
    m_lineParts.clear();
    m_lines.clear();
    if( m_linesWidth < 2 ) return;
    auto txt = m_text;
    bool headers = true;
    bool sig = false;
    for(;;)
    {
        auto end = txt;
        while( *end != '\n' && *end != '\0' ) end++;
        const auto len = std::min<uint32_t>( end - txt, ( 1 << LenBits ) - 1 );
        const auto offset = uint32_t( txt - m_text );
        if( offset >= ( 1 << OffsetBits ) ) return;
        if( headers )
        {
            if( len == 0 )
            {
                AddEmptyLine();
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
                    strnicmpl( txt, "date: ", 6 ) == 0 ||
                    strnicmpl( txt, "to: ", 3 ) == 0 )
                {
                    BreakLine( offset, len, LineType::Header );
                }
            }
        }
        else
        {
            if( len == 0 )
            {
                AddEmptyLine();
            }
            else
            {
                if( strncmp( "-- \n", txt, 4 ) == 0 )
                {
                    sig = true;
                }
                if( sig )
                {
                    BreakLine( offset, len, LineType::Signature );
                }
                else
                {
                    BreakLine( offset, len, LineType::Body );
                }
            }
        }
        if( *end == '\0' ) break;
        txt = end + 1;
    }
    while( !m_lines.empty() && m_lines.back().empty ) m_lines.pop_back();
}

void MessageView::AddEmptyLine()
{
    m_lines.emplace_back( Line { 0, 0, true } );
}

void MessageView::BreakLine( uint32_t offset, uint32_t len, LineType type )
{
    assert( len != 0 );

    std::vector<LinePart> parts;

    switch( type )
    {
    case LineType::Header:
        SplitHeader( offset, len, parts );
        break;
    case LineType::Body:
        SplitBody( offset, len, parts );
        break;
    case LineType::Signature:
        parts.emplace_back( LinePart { offset, len, L_Signature, false } );
        break;
    }

    auto ul = utflen( m_text + offset, m_text + offset + len );
    if( ul <= m_linesWidth )
    {
        m_lines.emplace_back( Line { (uint32_t)m_lineParts.size(), (uint32_t)parts.size(), false } );
        for( auto& part : parts )
        {
            m_lineParts.emplace_back( part );
        }
    }
    else
    {
        bool br = false;
        auto ptr = m_text + offset;
        auto end = ptr + len;
        auto w = m_linesWidth;
        while( ptr != end )
        {
            if( br )
            {
                while( ptr < end && *ptr == ' ' ) ptr++;
                if( ptr == end ) return;
            }

            auto lw = w;
            auto e = utfendcrlfl( ptr, lw );

            if( lw == w && *e != ' ' && *(e-1) != ' ' )
            {
                const auto original = e;
                while( --e > ptr && *e != ' ' ) {}
                if( e == ptr ) e = original;
            }

            const uint32_t firstPart = m_lineParts.size();
            uint32_t partsNum = 0;
            auto partBr = br;
            for( const auto& v : parts )
            {
                auto ps = m_text + v.offset;
                auto pe = ps + v.len;

                if( ptr >= pe || e <= ps ) continue;

                const auto ss = std::max( ptr, ps );
                const auto se = std::min( e, pe );

                m_lineParts.emplace_back( LinePart { uint32_t( ss - m_text ), uint32_t( se - ss ), v.flags, partBr } );
                partBr = false;
                partsNum++;
            }
            m_lines.emplace_back( Line { firstPart, partsNum, false } );
            ptr = e;
            if( !br )
            {
                br = true;
                w--;
            }
        }
    }
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
    wprintw( m_win, "%.*s", end - start, tmp );
}

void MessageView::SplitHeader( uint32_t offset, uint32_t len, std::vector<LinePart>& parts )
{
    auto origin = m_text + offset;
    auto str = origin;
    int i;
    for( i=0; i<len; i++ )
    {
        if( *str++ == ':' ) break;
    }

    uint32_t nameLen = str - origin;
    uint32_t bodyLen = len - nameLen;

    if( bodyLen < 2 )
    {
        parts.emplace_back( LinePart { offset, len, L_HeaderBody } );
    }
    else
    {
        parts.emplace_back( LinePart { offset, nameLen, L_HeaderName } );
        parts.emplace_back( LinePart { offset + nameLen, bodyLen, L_HeaderBody } );
    }
}

void MessageView::SplitBody( uint32_t offset, uint32_t len, std::vector<LinePart>& parts )
{
    auto origin = m_text + offset;
    auto str = origin;
    auto test = str;
    int level = std::min( QuotationLevel( test, origin + len ), 5 );

    for( int i=0; i<level; i++ )
    {
        auto end = NextQuotationLevel( str ) + 1;
        parts.emplace_back( LinePart { uint64_t( str - m_text ), uint64_t( end - str ), uint64_t( L_Quote1 + i ) } );
        str = end;
    }

    parts.emplace_back( LinePart { uint64_t( str - m_text ), uint64_t( len - ( str - origin ) ), uint64_t( L_Quote0 + level ) } );
}
