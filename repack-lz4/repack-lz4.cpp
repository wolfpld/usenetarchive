#include <algorithm>
#include <assert.h>
#include <atomic>
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
#include "../common/Slab.hpp"
#include "../common/System.hpp"
#include "../common/TaskDispatch.hpp"
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

    size_t size;
    {
        const ZMessageView zview( base + "zmeta", base + "zdata", base + "zdict" );
        size = zview.Size();
    }

    struct Buffer
    {
        uint32_t compressedSize;
        uint32_t size;
        const char* data;
    };

    const auto cpus = System::CPUCores();
    printf( "Repacking (%i threads)\n", cpus );

    Buffer* data = new Buffer[size];

    TaskDispatch tasks( cpus-1 );
    std::atomic<uint32_t> cnt( 0 );
    auto slab = new Slab<32*1024*1024>[cpus];

    for( int t=0; t<cpus; t++ )
    {
        tasks.Queue( [&cnt, size, &base, &data, t, &slab] {
            ExpandingBuffer eb, eb_dec;
            ZMessageView zview( base + "zmeta", base + "zdata", base + "zdict" );
            for(;;)
            {
                auto j = cnt.fetch_add( 1, std::memory_order_relaxed );
                if( j >= size ) break;
                if( ( j & 0x3FF ) == 0 )
                {
                    printf( "%i/%i\r", j, size );
                    fflush( stdout );
                }

                auto raw = zview.Raw( j );
                auto post = zview.GetMessage( j, eb_dec );

                int maxSize = LZ4_compressBound( raw.size );
                char* compressed = eb.Request( maxSize );
                int csize = LZ4_compress_HC( post, compressed, raw.size, maxSize, 16 );

                char* buf = (char*)slab[t].Alloc( csize );
                memcpy( buf, compressed, csize );

                data[j].compressedSize = csize;
                data[j].size = raw.size;
                data[j].data = buf;
            }
        } );
    }
    tasks.Sync();
    printf( "%i/%i\n", size, size );

    FILE* fmeta = fopen( metafn.c_str(), "wb" );
    FILE* fdata = fopen( datafn.c_str(), "wb" );

    uint64_t offset = 0;
    for( size_t idx=0; idx<size; idx++ )
    {
        if( ( idx & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", idx, size );
            fflush( stdout );
        }

        fwrite( data[idx].data, 1, data[idx].compressedSize, fdata );

        RawImportMeta metaPacket = { offset, data[idx].size, data[idx].compressedSize };
        fwrite( &metaPacket, 1, sizeof( RawImportMeta ), fmeta );
        offset += data[idx].compressedSize;
    }
    printf( "%i messages processed.\n", size );

    fclose( fmeta );
    fclose( fdata );

    return 0;
}
