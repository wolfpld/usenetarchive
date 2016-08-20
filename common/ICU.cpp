#include <unicode/locid.h>
#include <unicode/brkiter.h>
#include <unicode/unistr.h>

#include "ICU.hpp"

UErrorCode wordItErr = U_ZERO_ERROR;
auto wordIt = icu::BreakIterator::createWordInstance( icu::Locale::getEnglish(), wordItErr );

void SplitLine( const char* ptr, const char* end, std::vector<std::string>& out )
{
    out.clear();
    auto us = icu::UnicodeString::fromUTF8( StringPiece( ptr, end-ptr ) );
    auto lower = us.toLower( icu::Locale::getEnglish() );

    wordIt->setText( lower );
    int32_t p0 = 0;
    int32_t p1 = wordIt->first();
    while( p1 != icu::BreakIterator::DONE )
    {
        auto part = lower.tempSubStringBetween( p0, p1 );
        auto len = part.length();
        if( len > 2 && len < 14 )
        {
            std::string str;
            part.toUTF8String( str );

            auto start = str.c_str();
            auto end = start + str.size();

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
                out.emplace_back( std::string( start, end-start ) );
            }
        }
        p0 = p1;
        p1 = wordIt->next();
    }
}
