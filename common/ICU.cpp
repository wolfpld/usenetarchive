#include <assert.h>
#include <ctype.h>
#include <unicode/locid.h>
#include <unicode/brkiter.h>
#include <unicode/unistr.h>

#include "ICU.hpp"
#include "LexiconTypes.hpp"
#include "Slab.hpp"

UErrorCode wordItErr = U_ZERO_ERROR;
auto wordIt = icu::BreakIterator::createWordInstance( icu::Locale::getEnglish(), wordItErr );

Slab<256*1024> tmpSlab;

void SplitLine( const char* ptr, const char* end, std::vector<std::string>& out, bool toLower )
{
    assert( ptr != end );
    out.clear();

    bool isUtf = false;
    for( auto p = ptr; p < end; p++ )
    {
        if( *p & 0x80 )
        {
            isUtf = true;
            break;
        }
    }

    if( isUtf )
    {
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
            auto len = part.length();
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
    else
    {
        tmpSlab.Reset();
        const auto size = end - ptr;
        auto buf = (char*)tmpSlab.Alloc( size );
        auto bptr = buf;
        const auto bend = buf + size;
        for( int i=0; i<size; i++ )
        {
            if( *ptr >= 'A' && *ptr <= 'Z' )
            {
                *bptr++ = *ptr++ - 'A' + 'a';
            }
            else
            {
                *bptr++ = *ptr++;
            }
        }
        bptr = buf;
        for(;;)
        {
            while( bptr < bend && !isalnum( *bptr ) ) bptr++;
            auto e = bptr+1;
            while( e < bend && ( isalnum( *e ) || *e == '_' || ( e < bend-1 && (
                    ( isalpha( e[-1] ) && isalpha( e[1] ) && ( *e == ':' || *e == '.' || *e == '\'' ) ) ||
                    ( isdigit( e[-1] ) && isdigit( e[1] ) && ( *e == ',' || *e == '.' || *e == '\'' || *e == ';' ) )
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
}

std::string ToLower( const char* ptr, const char* end )
{
    std::string ret;
    auto us = icu::UnicodeString::fromUTF8( StringPiece( ptr, end-ptr ) );
    icu::UnicodeString lower = us.toLower( icu::Locale::getEnglish() );
    lower.toUTF8String( ret );
    return ret;
}
