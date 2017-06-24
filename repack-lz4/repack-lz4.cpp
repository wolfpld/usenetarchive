#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

#ifdef _WIN32
#  include <direct.h>
#  include <windows.h>
#  undef GetMessage
#else
#  include <dirent.h>
#endif

#include "../contrib/lz4/lz4.h"
#include "../contrib/lz4/lz4hc.h"
#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/RawImportMeta.hpp"
#include "../common/ZMessageView.hpp"

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s archive\n", argv[0] );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Directory doesn't exist.\n" );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );
    std::string metafn = base + "meta";
    std::string datafn = base + "data";

    ZMessageView zview( base + "zmeta", base + "zdata", base + "zdict" );
    auto size = zview.Size();

    FILE* meta = fopen( metafn.c_str(), "wb" );
    FILE* data = fopen( datafn.c_str(), "wb" );

    ExpandingBuffer eb, eb_dec;
    uint64_t offset = 0;
    for( size_t idx=0; idx<size; idx++ )
    {
        if( ( idx & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", idx, size );
            fflush( stdout );
        }

        auto raw = zview.Raw( idx );
        auto buf = zview.GetMessage( idx, eb_dec );

        int maxSize = LZ4_compressBound( raw.size );
        char* compressed = eb.Request( maxSize );
        int csize = LZ4_compress_HC( buf, compressed, raw.size, maxSize, 16 );

        fwrite( compressed, 1, csize, data );

        RawImportMeta metaPacket = { offset, raw.size, csize };
        fwrite( &metaPacket, 1, sizeof( RawImportMeta ), meta );
        offset += csize;
    }
    printf( "%i messages processed.\n", size );

    fclose( meta );
    fclose( data );

    return 0;
}
