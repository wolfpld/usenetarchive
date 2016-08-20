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

#include "../common/ICU.hpp"
#include "../common/LexiconTypes.hpp"
#include "../common/MetaView.hpp"
#include "../common/MessageView.hpp"
#include "../common/String.hpp"

const char* AllowedHeaders[] = {
    "from",
    "subject",
    nullptr
};

const char* IgnoredWords[] = {
    "nie",
    "sie",
    u8"si\u0119",
    "jest",
    "jak",
    "ale",
    "czy",
    "tak",
    "http",
    "tylko",
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

void EraseWords( std::vector<std::string>& wordbuf )
{
    auto it = wordbuf.begin();
    while( it != wordbuf.end() )
    {
        bool skip = false;
        auto ptr = IgnoredWords;
        while( *ptr )
        {
            if( strcmp( *ptr, it->c_str() ) == 0 )
            {
                skip = true;
                break;
            }
            ptr++;
        }
        if( skip )
        {
            it = wordbuf.erase( it );
        }
        else
        {
            ++it;
        }
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

        int children = conn[i][2] - 1;
        children = std::min<uint32_t>( MaxChildren, children );
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
                    EraseWords( wordbuf );
                    if( !wordbuf.empty() )
                    {
                        Add( data, wordbuf, i, T_Header, 0, children );
                    }
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
                    EraseWords( wordbuf );
                    if( !wordbuf.empty() )
                    {
                        Add( data, wordbuf, i, t, basePos[t], children );
                        basePos[t] += wordbuf.size();
                    }
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
            uint8_t num = std::min<uint8_t>( std::numeric_limits<uint8_t>::max(), d.second.size() );

            fwrite( &d.first, 1, sizeof( uint32_t ), fdata );

            if( num < 4 )
            {
                uint32_t v = ohit | ( num << LexiconHitShift );
                fwrite( &v, 1, sizeof( uint32_t ), fdata );
            }
            else
            {
                fwrite( &ohit, 1, sizeof( uint32_t ), fdata );
                ohit += fwrite( &num, 1, sizeof( uint8_t ), fhit );
            }

            ohit += fwrite( d.second.data(), 1, sizeof( uint8_t ) * num, fhit );
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
