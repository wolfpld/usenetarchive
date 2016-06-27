#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>

#include "../contrib/lz4/lz4.h"
#include "../contrib/xxhash/xxhash.h"
#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"
#include "../common/MsgIdHash.hpp"
#include "../common/RawImportMeta.hpp"
#include "../common/String.hpp"

struct HashData
{
    uint32_t offset;
    uint32_t idx;
};

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s raw\n", argv[0] );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );
    std::string metafn = base + "meta";
    std::string datafn = base + "data";

    if( !Exists( metafn ) || !Exists( datafn ) )
    {
        fprintf( stderr, "Raw data files do not exist.\n" );
        exit( 1 );
    }

    FileMap<RawImportMeta> meta( metafn );
    FileMap<char> data( datafn );

    auto size = meta.Size() / sizeof( RawImportMeta );

    std::string midmetafn = base + "midmeta";
    std::string middatafn = base + "middata";

    FILE* midmeta = fopen( midmetafn.c_str(), "wb" );
    FILE* middata = fopen( middatafn.c_str(), "wb" );

    auto bucket = new std::vector<HashData>[MsgIdHashSize];

    uint32_t offset = 0;
    ExpandingBuffer eb;
    char zero = 0;
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto postsize = meta[i].size;
        auto post = eb.Request( postsize );
        auto dec = LZ4_decompress_fast( data + meta[i].offset, post, postsize );
        auto buf = post;
        assert( dec == meta[i].compressedSize );

        while( strnicmpl( buf, "message-id: <", 13 ) != 0 )
        {
            buf++;
            while( *buf++ != '\n' ) {}
        }
        buf += 13;
        auto end = buf;
        while( *end != '>' ) end++;
        fwrite( buf, 1, end-buf, middata );
        fwrite( &zero, 1, 1, middata );

        fwrite( &offset, 1, sizeof( offset ), midmeta );

        uint32_t hash = XXH32( buf, end-buf, 0 ) & MsgIdHashMask;
        bucket[hash].emplace_back( HashData { offset, i } );

        offset += end-buf+1;
    }

    fclose( midmeta );
    fclose( middata );

    printf( "Processed %i MsgIDs.\n", size );

    std::string midhashfn = base + "midhash";
    std::string midhashdatafn = base + "midhashdata";

    FILE* midhash = fopen( midhashfn.c_str(), "wb" );
    FILE* midhashdata = fopen( midhashdatafn.c_str(), "wb" );

    FileMap<char> msgid( middatafn );
    offset = 0;
    for( uint32_t i=0; i<MsgIdHashSize; i++ )
    {
        if( ( i & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        std::sort( bucket[i].begin(), bucket[i].end(), [&msgid]( const HashData& l, const HashData& r ) { return strcmp( msgid + l.offset, msgid + r.offset ) > 0; } );

        fwrite( &offset, 1, sizeof( offset ), midhash );

        uint32_t num = bucket[i].size();
        fwrite( &num, 1, sizeof( num ), midhashdata );
        fwrite( bucket[i].data(), 1, num * sizeof( HashData ), midhashdata );
        offset += sizeof( num ) + num * sizeof( HashData );
    }

    fclose( midhash );
    fclose( midhashdata );

    delete[] bucket;

    printf( "Processed %i buckets.\n", MsgIdHashSize );

    return 0;
}
