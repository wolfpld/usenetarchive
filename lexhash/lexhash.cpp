#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

#include "../contrib/xxhash/xxhash.h"
#include "../common/FileMap.hpp"
#include "../common/LexiconTypes.hpp"
#include "../common/MsgIdHash.hpp"

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s directory\n", argv[0] );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );
    FileMap<LexiconMetaPacket> meta( base + "lexmeta" );
    FileMap<char> str( base + "lexstr" );

    auto size = meta.DataSize();
    auto hashbits = MsgIdHashBits( size, 90 );
    auto hashsize = MsgIdHashSize( hashbits );
    auto hashmask = MsgIdHashMask( hashbits );

    auto offsets = new uint32_t[hashsize];
    auto hashdata = new uint32_t[hashsize];
    auto distance = new uint8_t[hashsize];
    memset( distance, 0xFF, hashsize );
    uint8_t distmax = 0;

    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0xFFF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto m = meta[i];
        auto s = str + m.str;

        uint32_t hash = XXH32( s, strlen( s ), 0 ) & hashmask;
        uint8_t dist = 0;
        uint32_t idx = i;
        uint32_t off = m.str;
        for(;;)
        {
            if( distance[hash] == 0xFF )
            {
                if( distmax < dist ) distmax = dist;
                distance[hash] = dist;
                hashdata[hash] = idx;
                offsets[hash] = off;
                break;
            }
            if( distance[hash] < dist )
            {
                if( distmax < dist ) distmax = dist;
                std::swap( distance[hash], dist );
                std::swap( hashdata[hash], idx );
                std::swap( offsets[hash], off );
            }
            dist++;
            assert( dist < std::numeric_limits<uint8_t>::max() );
            hash = (hash+1) & hashmask;
        }
    }

    printf( "\n" );

    FILE* fhashdata = fopen( ( base + "lexhashdata" ).c_str(), "wb" );
    fwrite( &distmax, 1, 1, fhashdata );
    fclose( fhashdata );

    uint32_t zero = 0;
    FILE* fhash = fopen( ( base + "lexhash" ).c_str(), "wb" );
    int cnt = 0;
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
            fwrite( offsets+i, 1, sizeof( uint32_t ), fhash );
            fwrite( hashdata+i, 1, sizeof( uint32_t ), fhash );
            cnt++;
        }
    }
    assert( cnt == size );
    fclose( fhash );

    printf( "%i/%i\n", hashsize, hashsize );

    return 0;
}
