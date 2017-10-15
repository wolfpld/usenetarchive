#include <assert.h>
#include <unicode/locid.h>
#include <unicode/brkiter.h>
#include <unicode/unistr.h>

#include "Alloc.hpp"
#include "ICU.hpp"
#include "LexiconTypes.hpp"
#include "Slab.hpp"

UErrorCode wordItErr = U_ZERO_ERROR;
auto wordIt = icu::BreakIterator::createWordInstance( icu::Locale::getEnglish(), wordItErr );

static inline bool _isalpha( char c )
{
    return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' );
}

static inline bool _isdigit( char c )
{
    return c >= '0' && c <= '9';
}

static inline bool _isalnum( char c )
{
    return _isalpha( c ) || _isdigit( c );
}

static inline bool IsUtf( const char* begin, const char* end )
{
    assert( begin <= end );
    while( begin != end )
    {
        if( *begin++ & 0x80 )
        {
            return true;
        }
    }
    return false;
}

static void SplitICU( const char* ptr, const char* end, std::vector<std::string>& out, bool toLower )
{
    assert( ptr != end );

    auto us = icu::UnicodeString::fromUTF8( StringPiece( ptr, end-ptr ) );
    icu::UnicodeString lower;

    if( toLower )
    {
        lower = us.toLower( icu::Locale::getEnglish() );
        wordIt->setText( lower );
    }
    else
    {
        wordIt->setText( us );
    }

    icu::UnicodeString& data = toLower ? lower : us;

    int32_t p0 = 0;
    int32_t p1 = wordIt->first();
    while( p1 != icu::BreakIterator::DONE )
    {
        auto part = data.tempSubStringBetween( p0, p1 );
        auto len = part.countChar32();
        if( len >= LexiconMinLen )
        {
            std::string str;
            part.toUTF8String( str );

            auto start = str.c_str();
            auto end = start + str.size();

            auto origlen = len;
            while( *start == '_' )
            {
                start++;
                len--;
            }
            while( end > start && *(end-1) == '_' )
            {
                end--;
                len--;
            }

            if( len >= LexiconMinLen && len <= LexiconMaxLen )
            {
                if( origlen == len )
                {
                    out.emplace_back( std::move( str ) );
                }
                else
                {
                    out.emplace_back( std::string( start, end-start ) );
                }
            }
        }
        p0 = p1;
        p1 = wordIt->next();
    }
}

static void SplitASCII( const char* ptr, const char* end, std::vector<std::string>& out, bool toLower )
{
    static Slab<256*1024> tmpSlab;

    assert( ptr != end );

    const auto size = end - ptr;
    const char* bptr;
    if( toLower )
    {
        tmpSlab.Reset();
        auto buf = (char*)tmpSlab.Alloc( size );
        auto lc = buf;
        for( int i=0; i<size; i++ )
        {
            if( *ptr >= 'A' && *ptr <= 'Z' )
            {
                *lc++ = *ptr++ - 'A' + 'a';
            }
            else
            {
                *lc++ = *ptr++;
            }
        }
        bptr = buf;
    }
    else
    {
        bptr = ptr;
    }
    const char* bend = bptr + size;
    for(;;)
    {
        while( bptr < bend && !_isalnum( *bptr ) ) bptr++;
        auto e = bptr+1;
        while( e < bend && ( _isalnum( *e ) || *e == '_' || ( e < bend-1 && (
            ( _isalpha( e[-1] ) && _isalpha( e[1] ) && ( *e == ':' || *e == '.' || *e == '\'' ) ) ||
            ( _isdigit( e[-1] ) && _isdigit( e[1] ) && ( *e == ',' || *e == '.' || *e == '\'' || *e == ';' ) )
            ) ) ) ) e++;
        while( e > bptr+2 && e[-1] == '_' ) e--;
        auto len = e - bptr;
        if( len >= LexiconMinLen && len <= LexiconMaxLen )
        {
            out.emplace_back( std::string( bptr, e ) );
        }
        if( e >= bend ) break;
        bptr = e+1;
    }
}

void SplitLine( const char* ptr, const char* end, std::vector<std::string>& out, bool toLower )
{
    assert( ptr != end );
    out.clear();

    if( IsUtf( ptr, end ) )
    {
        while( ptr <= end )
        {
            auto putf = ptr;
            while( putf < end && ( *putf & 0x80 ) == 0 ) putf++;
            if( putf == end )
            {
                if( ptr != end )
                {
                    SplitASCII( ptr, end, out, toLower );
                }
                break;
            }
            auto split = putf;
            while( split > ptr && *split != ' ' && *split != '\t' ) split--;
            if( split > ptr )
            {
                SplitASCII( ptr, split, out, toLower );
                ptr = split+1;
            }
            while( putf < end && *putf != ' ' && *putf != '\t' ) putf++;
            SplitICU( ptr, putf, out, toLower );
            ptr = putf + 1;
        }
    }
    else
    {
        SplitASCII( ptr, end, out, toLower );
    }
}

std::string ToLower( const char* ptr, const char* end )
{
    const auto size = end - ptr;
    if( IsUtf( ptr, end ) )
    {
        auto us = icu::UnicodeString::fromUTF8( StringPiece( ptr, size ) );
        icu::UnicodeString lower = us.toLower( icu::Locale::getEnglish() );
        std::string ret;
        lower.toUTF8String( ret );
        return ret;
    }
    else
    {
        auto buf = (char*)alloca( size );
        auto lc = buf;
        for( int i=0; i<size; i++ )
        {
            if( *ptr >= 'A' && *ptr <= 'Z' )
            {
                *lc++ = *ptr++ - 'A' + 'a';
            }
            else
            {
                *lc++ = *ptr++;
            }
        }
        return std::string( buf, buf+size );
    }
}
