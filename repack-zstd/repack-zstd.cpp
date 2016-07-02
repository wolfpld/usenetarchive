#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>

#include "../contrib/zstd/common/zstd.h"
#include "../contrib/zstd/dictBuilder/zdict.h"

#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"
#include "../common/MessageView.hpp"
#include "../common/RawImportMeta.hpp"
#include "../common/String.hpp"

int main( int argc, char** argv )
{
    if( argc != 3 )
    {
        fprintf( stderr, "USAGE: %s source destination\n", argv[0] );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Source directory doesn't exist.\n" );
        exit( 1 );
    }
    if( Exists( argv[2] ) )
    {
        fprintf( stderr, "Destination directory already exists.\n" );
        exit( 1 );
    }

    CreateDirStruct( argv[2] );

    std::string base = argv[1];
    base.append( "/" );

    MessageView mview( base + "meta", base + "data" );
    const auto size = mview.Size();

    std::string zbase = argv[2];
    zbase.append( "/" );
    std::string zmetafn = zbase + "zmeta";
    std::string zdatafn = zbase + "zdata";
    std::string zdictfn = zbase + "zdict";

    printf( "Building dictionary\n" );
    uint64_t total = 0;
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0xFFF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto raw = mview.Raw( i );
        total += raw.size;
    }

    printf( "\nData buffer size: %i MB\n", total / 1024 / 1024 );

    auto samplesBuf = new char[total];
    auto samplesSizes = new size_t[size];
    auto ptr = samplesBuf;
    auto ss = samplesSizes;
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto post = mview[i];
        auto raw = mview.Raw( i );

        memcpy( ptr, post, raw.size );
        ptr += raw.size;
        *ss++ = raw.size;
    }

    enum { DictSize = 1024*1024 };
    auto dict = new char[DictSize];

    printf( "\nWorking...\n" );
    fflush( stdout );

    ZDICT_params_t params;
    memset( &params, 0, sizeof( ZDICT_params_t ) );
    params.notificationLevel = 3;
    params.compressionLevel = 16;
    auto realDictSize = ZDICT_trainFromBuffer_advanced( dict, DictSize, samplesBuf, samplesSizes, size, params );

    printf( "Dict size: %i\n", realDictSize );

    delete[] samplesBuf;
    delete[] samplesSizes;

    auto zdict = ZSTD_createCDict( dict, realDictSize, 16 );
    auto zctx = ZSTD_createCCtx();

    FILE* zdictfile = fopen( zdictfn.c_str(), "wb" );
    fwrite( dict, 1, realDictSize, zdictfile );
    fclose( zdictfile );
    delete[] dict;

    FILE* zmeta = fopen( zmetafn.c_str(), "wb" );
    FILE* zdata = fopen( zdatafn.c_str(), "wb" );

    printf( "Repacking\n" );
    ExpandingBuffer eb;
    uint64_t offset = 0;
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto post = mview[i];
        auto raw = mview.Raw( i );

        auto predSize = ZSTD_compressBound( raw.size );
        auto dst = eb.Request( predSize );
        auto dstSize = ZSTD_compress_usingCDict( zctx, dst, predSize, post, raw.size, zdict );

        RawImportMeta packet = { offset, raw.size, dstSize };
        fwrite( &packet, 1, sizeof( RawImportMeta ), zmeta );

        fwrite( dst, 1, dstSize, zdata );
        offset += dstSize;
    }

    ZSTD_freeCDict( zdict );
    ZSTD_freeCCtx( zctx );

    fclose( zmeta );
    fclose( zdata );

    return 0;
}
