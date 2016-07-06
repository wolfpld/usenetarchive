#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#include "../common/Filesystem.hpp"
#include "../common/HashSearch.hpp"
#include "../common/MessageView.hpp"
#include "../common/MetaView.hpp"
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

    MessageView mview1( base1 + "meta", base1 + "data" );
    HashSearch hash1( base1 + "middata", base1 + "midhash", base1 + "midhashdata" );

    MessageView mview2( base2 + "meta", base2 + "data" );
    MetaView<uint32_t, char> mid2( base2 + "midmeta", base2 + "middata" );

    printf( "Src1 size: %i. Src2 size: %i.\n", mview1.Size(), mview2.Size() );
    fflush( stdout );

    std::string meta3fn = base3 + "meta";
    std::string data3fn = base3 + "data";

    FILE* meta3 = fopen( meta3fn.c_str(), "wb" );
    FILE* data3 = fopen( data3fn.c_str(), "wb" );

    auto ptrs = mview1.Pointers();
    fwrite( ptrs.meta, 1, ptrs.metasize, meta3 );
    fwrite( ptrs.data, 1, ptrs.datasize, data3 );

    uint32_t added = 0;
    uint32_t dupes = 0;
    const auto size1 = mview1.Size();
    const auto size2 = mid2.Size();
    uint64_t offset1 = ptrs.datasize;
    for( int i=0; i<size2; i++ )
    {
        if( ( i & 0x1FFF ) == 0 )
        {
            printf( "%i/%i\r", i, size2 );
            fflush( stdout );
        }

        if( hash1.Search( mid2[i] ) >= 0 )
        {
            dupes++;
        }
        else
        {
            added++;
            const auto raw = mview2.Raw( i );
            fwrite( raw.ptr, 1, raw.compressedSize, data3 );
            RawImportMeta metaPacket = { offset1, raw.size, raw.compressedSize };
            fwrite( &metaPacket, 1, sizeof( RawImportMeta ), meta3 );
            offset1 += raw.compressedSize;
        }
    }

    fclose( meta3 );
    fclose( data3 );

    printf( "%i posts checked. %i added, %i dupes. %i unique in src1.\n", size2, added, dupes, size1 - dupes );

    return 0;
}
