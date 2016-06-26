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
    ExpandingBuffer eb, eb2;
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

        while( ( buf - post + 14 ) < postsize && strncmp( buf, "Message-ID: <", 13 ) != 0 && strncmp( buf, "Message-Id: <", 13 ) != 0 ) buf++;
        if( strncmp( buf, "Message-ID: <", 13 ) != 0 && strncmp( buf, "Message-Id: <", 13 ) != 0 )
        {
            std::string error( post, post+postsize );
            fprintf( stderr, "Malformed message! ID=%i\n\n%s\n", i, error.c_str() );
            exit( 1 );
        }
        buf += 13;
        auto end = buf;
        while( *end != '>' ) end++;

        uint32_t midsize = end - buf + 1;
        char* tmp = eb2.Request( midsize );
        memcpy( tmp, buf, end-buf );
        tmp[end-buf] = '\0';
        fwrite( tmp, 1, midsize, middata );

        fwrite( &offset, 1, sizeof( offset ), midmeta );
        offset += midsize;
    }

    fclose( midmeta );
    fclose( middata );

    printf( "Processed %i MsgIDs.\n", size );

    return 0;
}
