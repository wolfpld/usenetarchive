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
#include <vector>

#ifndef _WIN32
#  include <unistd.h>
#endif

#include "../contrib/zstd/zstd.h"
#include "../contrib/zstd/dictBuilder/zdict.h"

#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"
#include "../common/HashSearch.hpp"
#include "../common/MessageView.hpp"
#include "../common/MetaView.hpp"
#include "../common/RawImportMeta.hpp"
#include "../common/String.hpp"
#include "../common/StringCompress.hpp"
#include "../common/System.hpp"
#include "../common/TaskDispatch.hpp"
#include "../common/ZMessageView.hpp"

int main( int argc, char** argv )
{
    int zlevel = 16;
    bool overwrite = false;

    if( argc < 4 )
    {
        fprintf( stderr, "USAGE: %s [params] source update destination\nParams:\n", argv[0] );
        fprintf( stderr, " -z level        - set compression level (default: %i)\n", zlevel );
        fprintf( stderr, " -o              - overwrite previously existing messages\n" );
        exit( 1 );
    }

    for(;;)
    {
        if( strcmp( argv[1], "-z" ) == 0 )
        {
            zlevel = atoi( argv[2] );
            argv += 2;
        }
        if( strcmp( argv[1], "-o" ) == 0 )
        {
            overwrite = true;
            argv++;
        }
        else
        {
            break;
        }
    }

    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Source directory doesn't exist.\n" );
        exit( 1 );
    }
    if( !Exists( argv[2] ) )
    {
        fprintf( stderr, "Update directory doesn't exist.\n" );
        exit( 1 );
    }
    if( Exists( argv[3] ) )
    {
        fprintf( stderr, "Destination directory already exists.\n" );
        exit( 1 );
    }

    std::string source = argv[1];
    source.append( "/" );
    std::string update = argv[2];
    update.append( "/" );
    std::string target = argv[3];
    target.append( "/" );

    MessageView uview( update + "meta", update + "data" );
    auto usize = uview.Size();

    std::string szmetafn = source + "zmeta";
    std::string szdatafn = source + "zdata";
    std::string szdictfn = source + "zdict";

    const ZMessageView zview( szmetafn, szdatafn, szdictfn );
    const HashSearch shash( source + "middata", source + "midhash", source + "midhashdata" );
    const HashSearch uhash( update + "middata", update + "midhash", update + "midhashdata" );
    const MetaView<uint32_t, uint8_t> smiddb( source + "midmeta", source + "middata" );
    const MetaView<uint32_t, uint8_t> umiddb( update + "midmeta", update + "middata" );
    const StringCompress scomp( source + "msgid.codebook" );
    const StringCompress ucomp( update + "msgid.codebook" );

    ZSTD_CDict* zdict;
    {
        FileMap<char> szdict( szdictfn );
        zdict = ZSTD_createCDict( szdict, szdict.Size(), zlevel );
    }

    const auto cpus = System::CPUCores();

    printf( "Repacking (%i threads)\n", cpus );

    struct Buffer
    {
        uint32_t compressedSize;
        uint32_t size;
        const char* data;
    };

    Buffer* data = new Buffer[usize];

    std::mutex mtx, cntmtx;
    TaskDispatch tasks( cpus );
    uint32_t start = 0;
    uint32_t inPass = ( usize + cpus - 1 ) / cpus;
    uint32_t left = usize;
    uint32_t cnt = 0;
    for( int i=0; i<cpus; i++ )
    {
        uint32_t todo = std::min( left, inPass );
        tasks.Queue( [data, zdict, start, todo, &uview, &mtx, &cnt, &cntmtx, usize] () {
            auto zctx = ZSTD_createCCtx();
            ExpandingBuffer eb1, eb2;
            for( uint32_t i=start; i<start+todo; i++ )
            {
                cntmtx.lock();
                auto c = cnt++;
                cntmtx.unlock();
                if( ( c & 0x3FF ) == 0 )
                {
                    printf( "%i/%i\r", c, usize );
                    fflush( stdout );
                }

                auto raw = uview.Raw( i );
                auto post = eb1.Request( raw.size );
                mtx.lock();
                auto _post = uview[i];
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

    std::string zmetafn = target + "zmeta";
    std::string zdatafn = target + "zdata";
    std::string zdictfn = target + "zdict";

    CreateDirStruct( target );
    CopyFile( szdictfn, zdictfn );

    CopyCommonFiles( source, target );

    FILE* zmeta = fopen( zmetafn.c_str(), "wb" );
    FILE* zdata = fopen( zdatafn.c_str(), "wb" );

    uint64_t offset = 0;
    if( overwrite )
    {
        auto ssize = zview.Size();
        for( int i=0; i<ssize; i++ )
        {
            uint8_t repack[2048];
            ucomp.Repack( smiddb[i], repack, scomp );
            const auto idx = uhash.Search( repack );
            if( idx == -1 )
            {
                const auto raw = zview.Raw( i );
                RawImportMeta packet = { offset, raw.size, raw.compressedSize };
                fwrite( &packet, 1, sizeof( RawImportMeta ), zmeta );

                fwrite( raw.ptr, 1, raw.compressedSize, zdata );
                offset += raw.compressedSize;
            }
        }
        for( int i=0; i<usize; i++ )
        {
            RawImportMeta packet = { offset, data[i].size, data[i].compressedSize };
            fwrite( &packet, 1, sizeof( RawImportMeta ), zmeta );

            fwrite( data[i].data, 1, data[i].compressedSize, zdata );
            offset += data[i].compressedSize;
        }
    }
    else
    {
        for( int i=0; i<usize; i++ )
        {
            uint8_t repack[2048];
            scomp.Repack( umiddb[i], repack, ucomp );
            const auto idx = shash.Search( repack );
            if( idx == -1 )
            {
                RawImportMeta packet = { offset, data[i].size, data[i].compressedSize };
                fwrite( &packet, 1, sizeof( RawImportMeta ), zmeta );

                fwrite( data[i].data, 1, data[i].compressedSize, zdata );
                offset += data[i].compressedSize;
            }
        }
        auto ssize = zview.Size();
        for( int i=0; i<ssize; i++ )
        {
            const auto raw = zview.Raw( i );
            RawImportMeta packet = { offset, raw.size, raw.compressedSize };
            fwrite( &packet, 1, sizeof( RawImportMeta ), zmeta );

            fwrite( raw.ptr, 1, raw.compressedSize, zdata );
            offset += raw.compressedSize;
        }
    }

    fclose( zmeta );
    fclose( zdata );

    delete[] data;

    return 0;
}
