#include <assert.h>
#include <limits>
#include <string.h>

#include "MessageLines.hpp"
#include "MessageLogic.hpp"
#include "String.hpp"
#include "UTF8.hpp"

MessageLines::MessageLines()
    : m_width( std::numeric_limits<uint32_t>::max() )
{
    m_lineParts.reserve( 2048 );
    m_lines.reserve( 1024 );
}

void MessageLines::PrepareLines( const char* text, bool skipHeaders )
{
    m_lineParts.clear();
    m_lines.clear();

    std::vector<LinePart> partsTmpBuf;
    partsTmpBuf.reserve( 16 );
    auto txt = text;
    bool headers = true;
    bool sig = false;
    for(;;)
    {
        auto end = txt;
        while( *end != '\n' && *end != '\0' ) end++;
        const auto len = std::min<uint32_t>( end - txt, ( 1 << LenBits ) - 1 );
        const auto offset = uint32_t( txt - text );
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
                const bool essentialHeader =
                    strnicmpl( txt, "from: ", 6 ) == 0 ||
                    strnicmpl( txt, "newsgroups: ", 12 ) == 0 ||
                    strnicmpl( txt, "subject: ", 9 ) == 0 ||
                    strnicmpl( txt, "date: ", 6 ) == 0 ||
                    strnicmpl( txt, "to: ", 3 ) == 0;
                if( !skipHeaders || essentialHeader )
                {
                    BreakLine( offset, len, LineType::Header, partsTmpBuf, text, essentialHeader );
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
                    BreakLine( offset, len, LineType::Signature, partsTmpBuf, text, false );
                }
                else
                {
                    BreakLine( offset, len, LineType::Body, partsTmpBuf, text, false );
                }
            }
        }
        if( *end == '\0' ) break;
        txt = end + 1;
    }
    while( !m_lines.empty() && m_lines.back().parts == 0 ) m_lines.pop_back();

}

void MessageLines::Reset()
{
    m_lineParts.clear();
    m_lines.clear();
}

void MessageLines::AddEmptyLine()
{
    m_lines.emplace_back( Line { 0, 0 } );
}

void MessageLines::BreakLine( uint32_t offset, uint32_t len, LineType type, std::vector<LinePart>& parts, const char* text, bool essential )
{
    assert( len != 0 );

    parts.clear();

    switch( type )
    {
    case LineType::Header:
        SplitHeader( offset, len, parts, text );
        break;
    case LineType::Body:
        SplitBody( offset, len, parts, text );
        break;
    case LineType::Signature:
        parts.emplace_back( LinePart { offset, len, L_Signature, D_None, false } );
        break;
    }

    auto ul = utflen( text + offset, text + offset + len );
    if( ul <= m_width )
    {
        m_lines.emplace_back( Line { (uint32_t)m_lineParts.size(), (uint32_t)parts.size(), essential } );
        for( auto& part : parts )
        {
            m_lineParts.emplace_back( part );
        }
    }
    else
    {
        bool br = false;
        auto ptr = text + offset;
        auto end = ptr + len;
        auto w = m_width;
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
                auto ps = text + v.offset;
                auto pe = ps + v.len;

                if( ptr >= pe || e <= ps ) continue;

                const auto ss = std::max( ptr, ps );
                const auto se = std::min( e, pe );

                m_lineParts.emplace_back( LinePart { uint32_t( ss - text ), uint32_t( se - ss ), v.flags, v.deco, partBr } );
                partBr = false;
                partsNum++;
            }
            m_lines.emplace_back( Line { firstPart, partsNum, essential } );
            ptr = e;
            if( !br )
            {
                br = true;
                w--;
            }
        }
    }
}

void MessageLines::SplitHeader( uint32_t offset, uint32_t len, std::vector<LinePart>& parts, const char* text )
{
    auto origin = text + offset;
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

static int FindUrl( const char*& start, const char* end )
{
    assert( start <= end );
    for(;;)
    {
        auto ptr = start;
        while( ptr < end && *ptr != ':' ) ptr++;
        if( ptr >= end ) return -1;

        auto tmp = ptr;
        while( ptr > start && isalpha( ((unsigned char*)ptr)[-1] ) ) ptr--;

        // slrn: "all registered and reserved scheme names are >= 3 chars long"
        if( tmp - ptr < 3 )
        {
            start = tmp + 1;
            continue;
        }
        if( tmp - ptr == 4 && end - tmp >= 4 && memcmp( ptr, "news:", 5 ) == 0 )     // end - tmp >= 4  ->  ':' + a@b
        {
            tmp++;
            bool brackets = false;
            if( *tmp == '<' )
            {
                brackets = true;
                tmp++;
            }
            while( tmp < end && *tmp != ' ' && *tmp != '\t' && *tmp != '<' && *tmp != '>' ) tmp++;
            if( tmp < end && brackets && *tmp == '>' ) tmp++;
        }
        else if( end - tmp < 3 || tmp[1] != '/' || tmp[2] != '/' )
        {
            start = tmp + 1;
            continue;
        }
        else
        {
            assert( tmp[1] == '/' && tmp[2] == '/' );
            tmp += 3;
            while( tmp < end && *tmp != ' ' && *tmp != '\t' && *tmp != '"' && *tmp != '{' && *tmp != '}' && *tmp != '<' && *tmp != '>' ) tmp++;
        }

        while( tmp > ptr && ( *(tmp-1) == '.' || *(tmp-1) == ',' || *(tmp-1) == ';' || *(tmp-1) == ':' || *(tmp-1) == '(' || *(tmp-1) == ')' ) ) tmp--;

        start = tmp;
        if( tmp - ptr < 6 ) continue;
        if( tmp[-3] == ':' && tmp[-2] == '/' && tmp[-1] == '/' ) continue;

        start = ptr;
        return tmp - ptr;
    }
}

void MessageLines::SplitBody( uint32_t offset, uint32_t len, std::vector<LinePart>& parts, const char* text )
{
    auto str = text + offset;
    const auto end = str + len;
    auto test = str;
    int level = std::min( QuotationLevel( test, end ), 5 );

    for( int i=0; i<level; i++ )
    {
        const auto end = NextQuotationLevel( str ) + 1;
        parts.emplace_back( LinePart { uint64_t( str - text ), uint64_t( end - str ), uint64_t( L_Quote1 + i ) } );
        str = end;
    }

    test = str;
    int urlsize;
    while( ( urlsize = FindUrl( test, end ) ) != -1 )
    {
        if( test != str )
        {
            Decorate( str, test, L_Quote0 + level, parts, text );
        }
        parts.emplace_back( LinePart { uint64_t( test - text ), uint64_t( urlsize ), uint64_t( L_Quote0 + level ), uint64_t( D_Underline ) } );

        str = test + urlsize;
        test = str;
    }

    Decorate( str, end, L_Quote0 + level, parts, text );
}

void MessageLines::Decorate( const char* begin, const char* end, uint64_t flags, std::vector<LinePart>& parts, const char* text )
{
    assert( begin <= end );
    auto str = begin;
    for(;;)
    {
        while( str < end && *str != '_' && *str != '*' && *str != '/' ) str++;
        if( str >= end )
        {
            parts.emplace_back( LinePart { uint64_t( begin - text ), uint64_t( end - begin ), flags } );
            return;
        }
        auto ch = *str;
        if( ( str > begin && ( str[-1] == ch || isalnum( ((unsigned char*)str)[-1] ) ) ) ||
            ( end - str > 1 && !isalnum( ((unsigned char*)str)[1] ) ) )
        {
            str++;
            continue;
        }

        auto tmp = str + 1;
        while( tmp < end && *tmp != '_' && *tmp != '*' && *tmp != '/' ) tmp++;
        if( tmp >= end )
        {
            parts.emplace_back( LinePart { uint64_t( begin - text ), uint64_t( end - begin ), flags } );
            return;
        }
        if( *tmp != ch ||
            ( tmp > begin && !isalnum( ((unsigned char*)tmp)[-1] ) && !ispunct( ((unsigned char*)tmp)[-1] ) ) ||
            ( end - tmp > 1 && ( tmp[1] == ch || isalnum( ((unsigned char*)tmp)[1] ) ) ) )
        {
            str++;
            continue;
        }

        DecoType deco;
        switch( ch )
        {
        case '_':
            deco = D_Underline;
            break;
        case '*':
            deco = D_Bold;
            break;
        case '/':
            deco = D_Italics;
            break;
        default:
            assert( false );
            deco = D_None;
            break;
        }

        tmp++;
        if( str > begin )
        {
            parts.emplace_back( LinePart { uint64_t( begin - text ), uint64_t( str - begin ), flags } );
        }
        parts.emplace_back( LinePart { uint64_t( str - text ), uint64_t( tmp - str ), flags, uint64_t( deco ) } );

        begin = tmp;
        if( begin >= end ) return;
        str = begin;
    }
}
