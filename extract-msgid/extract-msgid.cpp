#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>

#include "../contrib/lz4/lz4.h"
#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"
#include "../common/RawImportMeta.hpp"

static int strnicmpl( const char* l, const char* r, int n )
{
    while( n-- )
    {
        if( tolower( *l ) != *r ) return 1;
        else if( *l == '\0' ) return 0;
        l++; r++;
    }
    return 0;
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

    uint32_t offset = 0;
    ExpandingBuffer eb;
    char zero = 0;
    for( int i=0; i<size; i++ )
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
        offset += end-buf+1;
    }

    fclose( midmeta );
    fclose( middata );

    printf( "Processed %i MsgIDs.\n", size );

    return 0;
}
