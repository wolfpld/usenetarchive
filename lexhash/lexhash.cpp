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

struct HashData
{
    uint32_t offset;
    uint32_t idx;
};

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
    auto hashbits = MsgIdHashBits( size, 75 );
    auto hashsize = MsgIdHashSize( hashbits );
    auto hashmask = MsgIdHashMask( hashbits );
    auto bucket = new std::vector<HashData>[hashsize];
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
        bucket[hash].emplace_back( HashData { m.str, i } );
    }

    printf( "\n" );

    std::string hashfn = base + "lexhash";
    std::string hashdatafn = base + "lexhashdata";

    FILE* fhash = fopen( hashfn.c_str(), "wb" );
    FILE* fhashdata = fopen( hashdatafn.c_str(), "wb" );

    const uint32_t zero = 0;
    fwrite( &zero, 1, sizeof( uint32_t ), fhashdata );
    uint32_t offset = sizeof( uint32_t );
    for( uint32_t i=0; i<hashsize; i++ )
    {
        if( ( i & 0xFFF ) == 0 )
        {
            printf( "%i/%i\r", i, hashsize );
            fflush( stdout );
        }

        std::sort( bucket[i].begin(), bucket[i].end(), [&str]( const HashData& l, const HashData& r ) { return strcmp( str + l.offset, str + r.offset ) > 0; } );

        uint32_t num = bucket[i].size();
        if( num == 0 )
        {
            fwrite( &zero, 1, sizeof( uint32_t ), fhash );
        }
        else
        {
            fwrite( &offset, 1, sizeof( offset ), fhash );
            fwrite( &num, 1, sizeof( num ), fhashdata );
            fwrite( bucket[i].data(), 1, num * sizeof( HashData ), fhashdata );
            offset += sizeof( num ) + num * sizeof( HashData );
        }
    }

    fclose( fhash );
    fclose( fhashdata );

    delete[] bucket;

    printf( "Processed %i buckets.\n", hashsize );

    return 0;
}
