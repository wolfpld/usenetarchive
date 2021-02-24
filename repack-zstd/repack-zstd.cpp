#include <algorithm>
#include <assert.h>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <thread>
#include <vector>

#ifndef _WIN32
#  include <unistd.h>
#endif

#include "../contrib/zstd/zstd.h"
#include "../contrib/zstd/dictBuilder/zdict.h"

#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"
#include "../common/MessageView.hpp"
#include "../common/RawImportMeta.hpp"
#include "../common/String.hpp"
#include "../common/System.hpp"
#include "../common/TaskDispatch.hpp"

int main( int argc, char** argv )
{
    int zlevel = 16;
    int dpower = 31;

    if( argc < 2 )
    {
        fprintf( stderr, "USAGE: %s [params] directory\nParams:\n", argv[0] );
        fprintf( stderr, " -z level        - set compression level (default: %i)\n", zlevel );
        fprintf( stderr, " -s power        - set max sample size to 2^power (default: %i)\n", dpower );
        exit( 1 );
    }

    for(;;)
    {
        if( strcmp( argv[1], "-z" ) == 0 )
        {
            zlevel = atoi( argv[2] );
            argv += 2;
        }
        else if( strcmp( argv[1], "-s" ) == 0 )
        {
            dpower = std::min( 31, std::max( 10, atoi( argv[2] ) ) );
            argv += 2;
        }
        else
        {
            break;
        }
    }

    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Directory doesn't exist.\n" );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );

    MessageView mview( base + "meta", base + "data" );
    auto size = mview.Size();

    std::string zmetafn = base + "zmeta";
    std::string zdatafn = base + "zdata";
    std::string zdictfn = base + "zdict";

    printf( "Building dictionary\n" );

    std::string buf1fn = base + ".sb.tmp";
    std::string buf2fn = base + ".ss.tmp";

    FILE* buf1 = fopen( buf1fn.c_str(), "wb" );
    FILE* buf2 = fopen( buf2fn.c_str(), "wb" );
    uint64_t total = 0;
    auto samples = size;
    bool limitHit = false;
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto raw = mview.Raw( i );
        if( !limitHit && total + raw.size >= ( 1U << dpower ) )
        {
            printf( "Limiting sample size to %i MB - %i samples in, %i samples out.\n", total >> 20, i, size - i );
            samples = i;
            limitHit = true;
        }
        total += raw.size;

        auto post = mview[i];

        fwrite( post, 1, raw.size, buf1 );
        fwrite( &raw.size, 1, sizeof( size_t ), buf2 );
    }
    fclose( buf1 );
    fclose( buf2 );

    enum { DictSize = 4*1024*1024 };
    auto dict = new char[DictSize];
    size_t realDictSize;

    {
        auto samplesBuf = FileMap<char>( buf1fn );
        auto samplesSizes = FileMap<size_t>( buf2fn );

        printf( "\nWorking...\n" );
        fflush( stdout );

        ZDICT_fastCover_params_t params = {};
        params.d = 6;
        params.k = 50;
        params.f = 30;
        params.nbThreads = std::thread::hardware_concurrency();
        params.zParams.compressionLevel = zlevel;

        realDictSize = ZDICT_optimizeTrainFromBuffer_fastCover( dict, DictSize, samplesBuf, samplesSizes, samples, &params );
    }

    unlink( buf1fn.c_str() );
    unlink( buf2fn.c_str() );

    printf( "Dict size: %i\n", realDictSize );

    auto zdict = ZSTD_createCDict( dict, realDictSize, zlevel );

    FILE* zdictfile = fopen( zdictfn.c_str(), "wb" );
    fwrite( dict, 1, realDictSize, zdictfile );
    fclose( zdictfile );
    delete[] dict;

    const auto cpus = System::CPUCores();

    printf( "Repacking (%i threads)\n", cpus );

    struct Buffer
    {
        uint32_t compressedSize;
        uint32_t size;
        const char* data;
    };

    Buffer* data = new Buffer[size];

    std::mutex mtx, cntmtx;
    TaskDispatch tasks( cpus );
    uint32_t start = 0;
    uint32_t inPass = ( size + cpus - 1 ) / cpus;
    uint32_t left = size;
    uint32_t cnt = 0;
    for( int i=0; i<cpus; i++ )
    {
        uint32_t todo = std::min( left, inPass );
        tasks.Queue( [data, zdict, start, todo, &mview, &mtx, &cnt, &cntmtx, size] () {
            auto zctx = ZSTD_createCCtx();
            ExpandingBuffer eb1, eb2;
            for( uint32_t i=start; i<start+todo; i++ )
            {
                cntmtx.lock();
                auto c = cnt++;
                cntmtx.unlock();
                if( ( c & 0x3FF ) == 0 )
                {
                    printf( "%i/%i\r", c, size );
                    fflush( stdout );
                }

                auto raw = mview.Raw( i );
                auto post = eb1.Request( raw.size );
                mtx.lock();
                auto _post = mview[i];
                memcpy( post, _post, raw.size );
                mtx.unlock();

                auto predSize = ZSTD_compressBound( raw.size );
                auto dst = eb2.Request( predSize );
                auto dstSize = ZSTD_compress_usingCDict( zctx, dst, predSize, post, raw.size, zdict );

                char* buf = new char[dstSize];
                memcpy( buf, dst, dstSize );

                data[i].compressedSize = dstSize;
                data[i].size = raw.size;
                data[i].data = buf;
            }
            ZSTD_freeCCtx( zctx );
        } );
        start += todo;
        left -= todo;
    }
    tasks.Sync();

    ZSTD_freeCDict( zdict );

    printf( "\nWriting to disk...\n" );
    fflush( stdout );

    FILE* zmeta = fopen( zmetafn.c_str(), "wb" );
    FILE* zdata = fopen( zdatafn.c_str(), "wb" );

    uint64_t offset = 0;
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        RawImportMeta packet = { offset, data[i].size, data[i].compressedSize };
        fwrite( &packet, 1, sizeof( RawImportMeta ), zmeta );

        fwrite( data[i].data, 1, data[i].compressedSize, zdata );
        offset += data[i].compressedSize;
    }

    printf( "\n" );

    fclose( zmeta );
    fclose( zdata );

    delete[] data;

    return 0;
}
