#include <unicode/locid.h>
#include <unicode/brkiter.h>
#include <unicode/unistr.h>

#include "ICU.hpp"
#include "LexiconTypes.hpp"

UErrorCode wordItErr = U_ZERO_ERROR;
auto wordIt = icu::BreakIterator::createWordInstance( icu::Locale::getEnglish(), wordItErr );

void SplitLine( const char* ptr, const char* end, std::vector<std::string>& out, bool toLower )
{
    out.clear();
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
        if( len >= LexiconMinLen && len <= LexiconMaxLen )
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

            if( len > 2 )
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

std::string ToLower( const char* ptr, const char* end )
{
    std::string ret;
    auto us = icu::UnicodeString::fromUTF8( StringPiece( ptr, end-ptr ) );
    icu::UnicodeString lower = us.toLower( icu::Locale::getEnglish() );
    lower.toUTF8String( ret );
    return ret;
}
