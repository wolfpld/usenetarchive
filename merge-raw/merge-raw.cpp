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
#include "../common/StringCompress.hpp"

int main( int argc, char** argv )
{
    if( argc < 4 )
    {
        fprintf( stderr, "USAGE: %s destination source1 source2 [source3...]\n", argv[0] );
        fprintf( stderr, "NOTE: Specifying more than 2 sources will introduce duplicates in destination directory.\n" );
        exit( 1 );
    }
    if( Exists( argv[1] ) )
    {
        fprintf( stderr, "Destination directory already exists.\n" );
        exit( 1 );
    }
    for( int i=2; i<argc; i++ )
    {
        if( !Exists( argv[i] ) )
        {
            fprintf( stderr, "Source directory %i doesn't exist.\n", i-1 );
            exit( 1 );
        }
    }

    CreateDirStruct( argv[1] );

    std::string base1( argv[2] );
    base1 += "/";
    std::string basedst( argv[1] );
    basedst += "/";

    const MessageView mview1( base1 + "meta", base1 + "data" );
    const HashSearch<uint8_t> hash1( base1 + "middata", base1 + "midhash", base1 + "midhashdata" );
    StringCompress compress1( base1 + "msgid.codebook" );

    std::string metadstfn = basedst + "meta";
    std::string datadstfn = basedst + "data";

    FILE* meta3 = fopen( metadstfn.c_str(), "wb" );
    FILE* data3 = fopen( datadstfn.c_str(), "wb" );

    auto ptrs = mview1.Pointers();
    uint64_t offset1 = ptrs.datasize;
    fwrite( ptrs.meta, 1, ptrs.metasize, meta3 );
    fwrite( ptrs.data, 1, ptrs.datasize, data3 );

    for( int k=3; k<argc; k++ )
    {
        std::string base2( argv[k] );
        base2 += "/";

        const MessageView mview2( base2 + "meta", base2 + "data" );
        const MetaView<uint32_t, uint8_t> mid2( base2 + "midmeta", base2 + "middata" );
        StringCompress compress2( base2 + "msgid.codebook" );

        printf( "Src1 size: %i. Src2 size: %i.\n", mview1.Size(), mview2.Size() );
        fflush( stdout );

        uint32_t added = 0;
        uint32_t dupes = 0;
        const auto size1 = mview1.Size();
        const auto size2 = mid2.Size();
        for( int i=0; i<size2; i++ )
        {
            if( ( i & 0x1FFF ) == 0 )
            {
                printf( "%i/%i\r", i, size2 );
                fflush( stdout );
            }

            uint8_t repack[2048];
            compress1.Repack( mid2[i], repack, compress2 );
            if( hash1.Search( repack ) >= 0 )
            {
                dupes++;
            }
            else
            {
                added++;
                const auto raw = mview2.Raw( i );
                fwrite( raw.ptr, 1, raw.compressedSize, data3 );
                RawImportMeta metaPacket = { offset1, uint32_t( raw.size ), uint32_t( raw.compressedSize ) };
                fwrite( &metaPacket, 1, sizeof( RawImportMeta ), meta3 );
                offset1 += raw.compressedSize;
            }
        }

        printf( "%i posts checked. %i added, %i dupes. %i unique in src1.\n", size2, added, dupes, size1 - dupes );
    }

    fclose( meta3 );
    fclose( data3 );

    return 0;
}
