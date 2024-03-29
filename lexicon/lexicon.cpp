#include <assert.h>
#include <algorithm>
#include <ctype.h>
#include <limits>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <vector>

#include "../contrib/xxhash/xxhash.h"
#include "../common/ICU.hpp"
#include "../common/LexiconTypes.hpp"
#include "../common/MetaView.hpp"
#include "../common/MessageLogic.hpp"
#include "../common/MessageView.hpp"
#include "../common/MsgIdHash.hpp"

#include "../contrib/martinus/robin_hood.h"

enum class HeaderType
{
    From,
    Subject,
    Invalid
};

const char* AllowedHeaders[] = {
    "from",
    "subject",
    nullptr
};

HeaderType IsHeaderAllowed( const char* hdr, const char* end )
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
            switch( test - AllowedHeaders )
            {
            case 0:
                return HeaderType::From;
            case 1:
                return HeaderType::Subject;
            default:
                assert( false );
                return HeaderType::Invalid;
            }
        }
        test++;
    }
    return HeaderType::Invalid;
}

using HitData = robin_hood::unordered_flat_map<std::string, robin_hood::unordered_flat_map<uint32_t, std::vector<uint8_t>>>;

static void Add( HitData& data, std::vector<std::string>& words, uint32_t idx, int type, int basePos, int childCount )
{
    assert( ( idx & LexiconPostMask ) == idx );
    assert( childCount <= LexiconChildMax );
    idx = ( idx & LexiconPostMask ) | ( childCount << LexiconChildShift );

    uint8_t enc = LexiconHitTypeEncoding[type];
    uint8_t max = LexiconHitPosMask[type];
    for( auto& w : words )
    {
        auto it = data.find( w );
        if( it == data.end() )
        {
            uint8_t hit = enc | std::min<uint8_t>( max, basePos++ );
            data.emplace( std::move( w ), robin_hood::unordered_flat_map<uint32_t, std::vector<uint8_t>>( { { idx, std::vector<uint8_t> { hit } } } ) );
        }
        else
        {
            auto& vec = it->second[idx];
            if( vec.size() < std::numeric_limits<uint8_t>::max() )
            {
                if( basePos < max )
                {
                    uint8_t hit = enc | std::min<uint8_t>( max, basePos++ );
                    vec.emplace_back( hit );
                }
                else
                {
                    uint8_t hit = enc | max;
                    if( std::find( vec.begin(), vec.end(), hit ) == vec.end() )
                    {
                        vec.emplace_back( hit );
                    }
                }
            }
        }
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
            printf( "%i/%zu\r", i, size );
            fflush( stdout );
        }

        bool headers = true;
        bool signature = false;
        int wrote;
        int basePos[NUM_LEXICON_TYPES] = {};

        int children = LexiconTransformChildNum( conn[i][2] - 1 );

        auto post = mview[i];
        for(;;)
        {
            auto end = post;
            if( headers )
            {
                if( *end == '\n' )
                {
                    headers = false;
                    while( *end == '\n' ) end++;
                    post = end;
                    wrote = DetectWrote( post );
                    continue;
                }
                while( *end != ':' ) end++;
                end += 2;
                auto headerType = IsHeaderAllowed( post, end-2 );
                if( headerType != HeaderType::Invalid )
                {
                    int type;
                    switch( headerType )
                    {
                    case HeaderType::From:
                        type = T_From;
                        break;
                    case HeaderType::Subject:
                        type = T_Subject;
                        break;
                    default:
                        assert( false );
                        type = 0;
                        break;
                    }
                    const char* line = end;
                    while( *end != '\n' ) end++;
                    SplitLine( line, end, wordbuf );
                    Add( data, wordbuf, i, type, 0, children );
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
                if( end - line == 3 && strncmp( line, "-- ", 3 ) == 0 )
                {
                    signature = true;
                }
                else
                {
                    quotLevel = QuotationLevel( line, end );
                    assert( wrote <= 0 || quotLevel == 0 );
                }
                if( line != end )
                {
                    SplitLine( line, end, wordbuf );
                    LexiconType t;
                    if( signature )
                    {
                        t = T_Signature;
                    }
                    else if( wrote > 0 )
                    {
                        t = T_Wrote;
                        wrote--;
                    }
                    else
                    {
                        t = LexiconTypeFromQuotLevel( quotLevel );
                    }
                    Add( data, wordbuf, i, t, basePos[t], children );
                    basePos[t] += wordbuf.size();
                }
                if( *end == '\0' ) break;
                post = end + 1;
            }
        }
    }

    auto it = data.begin();
    while( it != data.end() )
    {
        if( it->second.size() == 1 )
        {
            it = data.erase( it );
        }
        else
        {
            ++it;
        }
    }

    printf( "\nSaving...\n" );
    fflush( stdout );

    auto wordNum = data.size();
    auto hashbits = MsgIdHashBits( wordNum, 90 );
    auto hashsize = MsgIdHashSize( hashbits );
    auto hashmask = MsgIdHashMask( hashbits );

    auto hashdata = new uint32_t[hashsize];
    auto distance = new uint8_t[hashsize];
    memset( distance, 0xFF, hashsize );
    uint8_t distmax = 0;

    int cnt = 0;
    std::vector<const char*> strings;
    strings.reserve( wordNum );
    for( auto& v : data )
    {
        if( ( cnt & 0xFFF ) == 0 )
        {
            printf( "%i/%zu\r", cnt, wordNum );
            fflush( stdout );
        }

        const auto& s = v.first;
        strings.emplace_back( s.c_str() );

        uint32_t hash = XXH32( s.c_str(), s.size(), 0 ) & hashmask;
        uint8_t dist = 0;
        uint32_t idx = cnt;
        for(;;)
        {
            if( distance[hash] == 0xFF )
            {
                if( distmax < dist ) distmax = dist;
                distance[hash] = dist;
                hashdata[hash] = idx;
                break;
            }
            if( distance[hash] < dist )
            {
                if( distmax < dist ) distmax = dist;
                std::swap( distance[hash], dist );
                std::swap( hashdata[hash], idx );
            }
            dist++;
            assert( dist < std::numeric_limits<uint8_t>::max() );
            hash = (hash+1) & hashmask;
        }

        cnt++;
    }

    printf( "\n" );

    FILE* fhashdata = fopen( ( base + "lexhashdata" ).c_str(), "wb" );
    fwrite( &distmax, 1, 1, fhashdata );
    fclose( fhashdata );

    FILE* fhash = fopen( ( base + "lexhash" ).c_str(), "wb" );
    FILE* fstr = fopen( ( base + "lexstr" ).c_str(), "wb" );

    uint32_t zero = 0;
    uint32_t stroffset = fwrite( &zero, 1, 1, fstr );

    auto offsetData = new uint32_t[wordNum];

    cnt = 0;
    for( int i=0; i<hashsize; i++ )
    {
        if( ( i & 0x3FFF ) == 0 )
        {
            printf( "%i/%i\r", i, hashsize );
            fflush( stdout );
        }

        if( distance[i] == 0xFF )
        {
            fwrite( &zero, 1, sizeof( uint32_t ), fhash );
            fwrite( &zero, 1, sizeof( uint32_t ), fhash );
        }
        else
        {
            fwrite( &stroffset, 1, sizeof( uint32_t ), fhash );
            fwrite( hashdata+i, 1, sizeof( uint32_t ), fhash );

            offsetData[hashdata[i]] = stroffset;
            cnt++;
            auto str = strings[hashdata[i]];
            stroffset += fwrite( str, 1, strlen( str ) + 1, fstr );
        }
    }
    assert( cnt == data.size() );
    fclose( fhash );
    fclose( fstr );

    printf( "\n" );

    FILE* fmeta = fopen( ( base + "lexmeta" ).c_str(), "wb" );
    FILE* fdata = fopen( ( base + "lexdata" ).c_str(), "wb" );
    FILE* fhit = fopen( ( base + "lexhit" ).c_str(), "wb" );

    uint32_t odata = 0;
    uint32_t ohit = 0;

    uint32_t idx = 0;
    const auto dataSize = data.size();
    for( auto& v : data )
    {
        if( ( idx & 0x3FF ) == 0 )
        {
            printf( "%i/%zu\r", idx, dataSize );
            fflush( stdout );
        }

        uint32_t dsize = v.second.size();
        fwrite( &offsetData[idx], 1, sizeof( uint32_t ), fmeta );
        fwrite( &odata, 1, sizeof( uint32_t ), fmeta );
        fwrite( &dsize, 1, sizeof( uint32_t ), fmeta );

        for( auto& d : v.second )
        {
            uint8_t num = std::min<uint8_t>( std::numeric_limits<uint8_t>::max(), d.second.size() );

            fwrite( &d.first, 1, sizeof( uint32_t ), fdata );

            if( num < 4 )
            {
                uint32_t numshift = num << LexiconHitShift;
                uint32_t v = 0;
                for( int i=0; i<num; i++ )
                {
                    v <<= 8;
                    v |= d.second[i];
                }
                v |= numshift;
                fwrite( &v, 1, sizeof( uint32_t ), fdata );
            }
            else
            {
                fwrite( &ohit, 1, sizeof( uint32_t ), fdata );
                ohit += fwrite( &num, 1, sizeof( uint8_t ), fhit );
                ohit += fwrite( d.second.data(), 1, sizeof( uint8_t ) * num, fhit );
            }
        }
        odata += sizeof( uint32_t ) * dsize * 2;

        idx++;
    }

    printf( "\n" );

    fclose( fmeta );
    fclose( fdata );
    fclose( fhit );

    return 0;
}
