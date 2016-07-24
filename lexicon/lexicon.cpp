#include <assert.h>
#include <algorithm>
#include <ctype.h>
#include <limits>
#include <unordered_map>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

#ifdef _WIN32
#  include <malloc.h>
#else
#  include <alloca.h>
#endif

#include <unicode/locid.h>
#include <unicode/brkiter.h>
#include <unicode/unistr.h>

#include "../common/LexiconTypes.hpp"
#include "../common/MetaView.hpp"
#include "../common/MessageView.hpp"
#include "../common/String.hpp"

const char* AllowedHeaders[] = {
    "from",
    "subject",
    nullptr
};

bool IsHeaderAllowed( const char* hdr, const char* end )
{
    int size = end - hdr;
    char* tmp = (char*)alloca( size+1 );
    for( int i=0; i<size; i++ )
    {
        tmp[i] = tolower( hdr[i] );
    }
    tmp[size] = '\0';

    auto test = AllowedHeaders;
    while( *test )
    {
        if( strncmp( tmp, *test, size+1 ) == 0 )
        {
            return true;
        }
        test++;
    }
    return false;
}

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
        std::string str;
        part.toUTF8String( str );
        if( str.size() > 2 && str.size() < 14 )
        {
            out.emplace_back( std::move( str ) );
        }
        p0 = p1;
        p1 = wordIt->next();
    }
}

using HitData = std::unordered_map<std::string, std::unordered_map<uint32_t, std::vector<uint8_t>>>;

enum { MaxChildren = 0xF8 };

void Add( HitData& data, const std::vector<std::string>& words, uint32_t idx, int type, int basePos, int childCount )
{
    assert( ( idx & LexiconPostMask ) == idx );
    assert( childCount <= LexiconChildMax );
    idx = ( idx & LexiconPostMask ) | ( childCount << LexiconChildShift );

    uint8_t enc = LexiconHitTypeEncoding[type];
    uint8_t max = LexiconHitPosMask[type];
    for( auto& w : words )
    {
        auto& hits = data[w];
        auto& vec = hits[idx];
        if( vec.size() < std::numeric_limits<uint8_t>::max() )
        {
            uint8_t hit = enc | std::min<uint8_t>( max, basePos++ );
            vec.emplace_back( hit );
        }
    }
}

void CountChildren( MetaView<uint32_t, uint32_t>& conn, uint32_t idx, int& cnt )
{
    if( ++cnt == MaxChildren ) return;
    auto data = conn[idx];
    data += 2;
    auto num = *data++;
    for( int i=0; i<num; i++ )
    {
        CountChildren( conn, *data++, cnt );
        if( cnt == MaxChildren ) return;
    }
}

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s raw\n", argv[0] );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );

    MessageView mview( base + "meta", base + "data" );
    MetaView<uint32_t, uint32_t> conn( base + "connmeta", base + "conndata" );
    const auto size = mview.Size();
    std::vector<std::string> wordbuf;

    // Purposefully disable destruction to not waste time at application exit
    HitData* dataPtr = new HitData();
    HitData& data = *dataPtr;

    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        bool headers = true;
        bool signature = false;
        int basePos[5] = {};

        int children = -1;
        CountChildren( conn, i, children );
        children /= 8;

        auto post = mview[i];
        for(;;)
        {
            auto end = post;
            if( headers )
            {
                if( *end == '\n' )
                {
                    headers = false;
                    continue;
                }
                while( *end != ':' ) end++;
                end += 2;
                if( IsHeaderAllowed( post, end-2 ) )
                {
                    const char* line = end;
                    while( *end != '\n' ) end++;
                    SplitLine( line, end, wordbuf );
                    Add( data, wordbuf, i, T_Header, 0, children );
                }
                else
                {
                    while( *end != '\n' ) end++;
                }
                post = end + 1;
            }
            else
            {
                const char* line = end;
                int quotLevel = 0;
                while( *end != '\n' && *end != '\0' ) end++;
                if( end - line == 4 && strncmp( line, "-- ", 3 ) == 0 )
                {
                    signature = true;
                }
                else
                {
                    while( *line == ' ' || *line == '>' || *line == ':' || *line == '|' || *line == '\t' )
                    {
                        if( *line == '>' || *line == ':' || *line == '|' )
                        {
                            quotLevel++;
                        }
                        line++;
                    }
                }
                if( line != end )
                {
                    SplitLine( line, end, wordbuf );
                    LexiconType t;
                    if( signature )
                    {
                        t = T_Signature;
                    }
                    else
                    {
                        switch( quotLevel )
                        {
                        case 0:
                            t = T_Content;
                            break;
                        case 1:
                            t = T_Quote1;
                            break;
                        case 2:
                            t = T_Quote2;
                            break;
                        default:
                            t = T_Quote3;
                            break;
                        }
                    }
                    Add( data, wordbuf, i, t, basePos[t], children );
                    basePos[t] += wordbuf.size();
                }
                if( *end == '\0' ) break;
                post = end + 1;
            }
        }
    }

    printf( "\nSaving...\n" );
    fflush( stdout );

    FILE* fmeta = fopen( ( base + "lexmeta" ).c_str(), "wb" );
    FILE* fstr = fopen( ( base + "lexstr" ).c_str(), "wb" );
    FILE* fdata = fopen( ( base + "lexdata" ).c_str(), "wb" );
    FILE* fhit = fopen( ( base + "lexhit" ).c_str(), "wb" );

    uint32_t ostr = 0;
    uint32_t odata = 0;
    uint32_t ohit = 0;

    uint32_t idx = 0;
    const auto dataSize = data.size();
    for( auto& v : data )
    {
        if( ( idx & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", idx, dataSize );
            fflush( stdout );
        }
        idx++;

        uint32_t dsize = v.second.size();
        fwrite( &ostr, 1, sizeof( uint32_t ), fmeta );
        fwrite( &odata, 1, sizeof( uint32_t ), fmeta );
        fwrite( &dsize, 1, sizeof( uint32_t ), fmeta );

        auto strsize = v.first.size() + 1;
        fwrite( v.first.c_str(), 1, strsize, fstr );
        ostr += strsize;

        for( auto& d : v.second )
        {
            fwrite( &d.first, 1, sizeof( uint32_t ), fdata );
            fwrite( &ohit, 1, sizeof( uint32_t ), fdata );

            uint8_t num = std::min<uint8_t>( std::numeric_limits<uint8_t>::max(), d.second.size() );
            fwrite( &num, 1, sizeof( uint8_t ), fhit );
            fwrite( d.second.data(), 1, sizeof( uint8_t ) * num, fhit );
            ohit += sizeof( uint8_t ) * (num+1);
        }
        odata += sizeof( uint32_t ) * dsize * 2;
    }

    printf( "\n" );

    fclose( fmeta );
    fclose( fstr );
    fclose( fdata );
    fclose( fhit );

    return 0;
}
