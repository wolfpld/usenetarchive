#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include "../contrib/lz4/lz4.h"
#include "../contrib/xxhash/xxhash.h"
#include "../common/FileMap.hpp"
#include "../common/Filesystem.hpp"
#include "../common/MsgIdHash.hpp"
#include "../common/RawImportMeta.hpp"

int main( int argc, char** argv )
{
    if( argc != 4 )
    {
        fprintf( stderr, "USAGE: %s source1 source2 destination\n", argv[0] );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Source directory 1 doesn't exist.\n" );
        exit( 1 );
    }
    if( !Exists( argv[2] ) )
    {
        fprintf( stderr, "Source directory 2 doesn't exist.\n" );
        exit( 1 );
    }
    if( Exists( argv[3] ) )
    {
        fprintf( stderr, "Destination directory already exists.\n" );
        exit( 1 );
    }

    CreateDirStruct( argv[3] );

    std::string base1( argv[1] );
    base1 += "/";
    std::string base2( argv[2] );
    base2 += "/";
    std::string base3( argv[3] );
    base3 += "/";

    FileMap<RawImportMeta> meta1( base1 + "meta" );
    FileMap<char> data1( base1 + "data" );
    FileMap<char> middata1( base1 + "middata" );
    FileMap<uint32_t> midhash1( base1 + "midhash" );
    FileMap<uint32_t> midhashdata1( base1 + "midhashdata" );

    FileMap<RawImportMeta> meta2( base2 + "meta" );
    FileMap<char> data2( base2 + "data" );
    FileMap<uint32_t> midmeta2( base2 + "midmeta" );
    FileMap<char> middata2( base2 + "middata" );

    printf( "Src1 size: %i. Src2 size: %i.\n", meta1.Size() / sizeof( RawImportMeta ), meta2.Size() / sizeof( RawImportMeta ) );
    fflush( stdout );

    std::string meta3fn = base3 + "meta";
    std::string data3fn = base3 + "data";

    FILE* meta3 = fopen( meta3fn.c_str(), "wb" );
    FILE* data3 = fopen( data3fn.c_str(), "wb" );

    fwrite( meta1, 1, meta1.Size(), meta3 );
    fwrite( data1, 1, data1.Size(), data3 );

    uint32_t added = 0;
    uint32_t dupes = 0;
    auto size1 = meta1.Size() / sizeof( RawImportMeta );
    auto size2 = midmeta2.Size() / sizeof( uint32_t );
    uint64_t offset1 = data1.Size();
    for( int i=0; i<size2; i++ )
    {
        if( ( i & 0x1FFF ) == 0 )
        {
            printf( "%i/%i\r", i, size2 );
            fflush( stdout );
        }

        const char* msgid = middata2 + midmeta2[i];
        auto hash = XXH32( msgid, strlen( msgid ), 0 ) & MsgIdHashMask;
        auto offset = midhash1[hash] / sizeof( uint32_t );
        const uint32_t* ptr = midhashdata1 + offset;
        auto num = *ptr++;
        bool found = false;
        for( int j=0; j<num; j++ )
        {
            if( strcmp( msgid, middata1 + *ptr++ ) == 0 )
            {
                dupes++;
                found = true;
                break;
            }
            ptr++;
        }
        if( !found )
        {
            added++;
            fwrite( data2 + meta2[i].offset, 1, meta2[i].compressedSize, data3 );
            RawImportMeta metaPacket = { offset1, meta2[i].size, meta2[i].compressedSize };
            fwrite( &metaPacket, 1, sizeof( RawImportMeta ), meta3 );
            offset1 += meta2[i].compressedSize;
        }
    }

    fclose( meta3 );
    fclose( data3 );

    printf( "%i posts checked. %i added, %i dupes. %i unique in src1.\n", size2, added, dupes, size1 - dupes );

    return 0;
}
